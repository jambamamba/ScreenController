#include "ScreenStreamer.h"

#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

#include "ImageConverterInterface.h"
#include "MouseInterface.h"
#include "RegionMapper.h"
#include "SocketReader.h"
#if defined(Win32) || defined(Win64)
#include "WindowsMouse.h"
#elif defined(Linux)
#include "X11Mouse.h"
#else
#include "NullMouse.h"
#endif

#include "Command.h"
#include "JpegConverter.h"
#include "WebPConverter.h"

namespace  {
static QImage &bltCursorOnImage(QImage &img, const QImage &cursor, const QPoint &pos)
{
    QPainter painter(&img);
    painter.drawImage(pos, cursor);
    painter.end();
    return img;
}

struct CommandAutoDestruct
{
    CommandAutoDestruct(size_t size)
    {
        m_pkt = (Command *)malloc(size);
    }
    ~CommandAutoDestruct()
    {
        free(m_pkt);
    }
    Command *m_pkt;
};

CommandAutoDestruct CreateFrameCommandPacket(
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        uint32_t screen_width,
        uint32_t screen_height,
        uint32_t decoder_type,
        const uint8_t *data,
        uint32_t data_size,
        uint32_t region_num,
        uint32_t max_regions)
{
    Command cmd;
    CommandAutoDestruct cmd_auto(sizeof (Command) + data_size);
    Command *pkt = cmd_auto.m_pkt;
    memcpy(pkt, &cmd, sizeof cmd);

    pkt->m_event = Command::EventType::FrameInfo;
    pkt->u.m_frame.m_x = x;
    pkt->u.m_frame.m_y = y;
    pkt->u.m_frame.m_width = width;
    pkt->u.m_frame.m_height = height;
    pkt->u.m_frame.m_screen_width = screen_width;
    pkt->u.m_frame.m_screen_height = screen_height;
    pkt->u.m_frame.m_size = data_size;
    pkt->u.m_frame.m_region_num = region_num;
    pkt->u.m_frame.m_max_regions = max_regions;
    pkt->u.m_frame.m_decoder_type = decoder_type;
    pkt->m_size = sizeof (Command) + data_size;

    uint8_t tail_bytes[4];
    memcpy(tail_bytes, cmd.m_tail_bytes, sizeof tail_bytes);
    memcpy(pkt->m_tail_bytes, data, data_size);
    memcpy(pkt->m_tail_bytes + data_size, tail_bytes, sizeof tail_bytes);

//    qDebug() << "### create frame packet of size" << pkt->m_size
//             << ", frame size" << pkt->u.m_frame.m_size
//             << ", x,y,w,h" << x << y << width << height;

    return cmd_auto;
}
}//namespace
ScreenStreamer::ScreenStreamer(SocketReader &socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
#if defined(Win32) || defined(Win64)
    ,m_mouse(new WindowsMouse(parent))
#elif defined(Linux)
    ,m_mouse(new X11Mouse(parent))
#else
    ,m_mouse(new NullMouse(parent))
#endif
    ,m_region_mapper(std::make_unique<RegionMapper>())
{
    InitAvailableScreens();
}

ScreenStreamer::~ScreenStreamer()
{
    m_die = true;
    if(thread_.valid()){thread_.wait();}
}

void ScreenStreamer::InitAvailableScreens()
{
    m_screens = QGuiApplication::screens();
    std::sort(m_screens.begin(), m_screens.end(),
              [](QScreen *a, QScreen *b){
        return (a->geometry().x() != b->geometry().x()) ?
                    a->geometry().x() < b->geometry().x() :
                    a->geometry().y() < b->geometry().y();
    });
}

int ScreenStreamer::ActiveScreenIdx() const
{
    int screen_idx = 0;
    for(auto screen : m_screens)
    {
        if(screen->geometry().contains(QCursor::pos()))
        {
            break;
        }
        screen_idx++;
    }
    return screen_idx;

}

QImage ScreenStreamer::ScreenShot()
{
    QImage screen_shot = ActiveScreen()->grabWindow(0).toImage().convertToFormat(QImage::Format::Format_RGB888);
    screen_shot = ApplyMouseCursor(screen_shot);
    return screen_shot;
}

QScreen *ScreenStreamer::ActiveScreen()
{
    return m_screens[ActiveScreenIdx()];
}

QImage& ScreenStreamer::ApplyMouseCursor(QImage& img)
{
    QPoint mouse_pos;
    QImage cursor = m_mouse->getMouseCursor(mouse_pos);
    if(cursor.isNull())
    {
        return img;
    }
//    qDebug() << "mouse" << mouse_pos
//             << "geom" << QGuiApplication::screens().at(m_screen_idx)->geometry();
    QRect screen_rect = ActiveScreen()->geometry();
    if(!screen_rect.contains(mouse_pos))
    {
        return img;
    }
    mouse_pos.setX(mouse_pos.x() - screen_rect.x());
    mouse_pos.setY(mouse_pos.y() - screen_rect.y());

//    if(!m_region.size().isNull())
//    {
//        QPoint region_top_left(m_region_top_left.rx(), m_region_top_left.ry());
//        mouse_pos = mouse_pos - region_top_left;
//    }
    img = bltCursorOnImage(img, cursor, mouse_pos);

    return img;
}

void ScreenStreamer::SendCommand(uint32_t ip, uint16_t event)
{
    Command pkt;
    pkt.m_event = event;
    SendCommand(ip, pkt);
}

void ScreenStreamer::SendCommand(uint32_t ip, const Command &pkt)
{
    m_socket.SendData((uint8_t*)&pkt, pkt.m_size, ip, m_socket.GetPort());

    Command noop;
    noop.m_event = Command::EventType::None;
    m_socket.SendData((uint8_t*)&noop, sizeof(Command), ip, m_socket.GetPort());
}

void ScreenStreamer::StartStreaming(uint32_t ip, uint32_t decoder_type)
{
    if(m_streaming)
    {
        return;//todo - start streaming to ip if its different than the one we are streaming to.
    }
    thread_ = std::async(std::launch::async, [this, ip, decoder_type](){
        m_streaming = true;
        pthread_setname_np(pthread_self(), "scrncap");
        ImageConverterInterface *img_converter = nullptr;
        if(decoder_type == ImageConverterInterface::Types::Webp)
        { img_converter = new WebPConverter; }
        else if (decoder_type == ImageConverterInterface::Types::Jpeg)
        { img_converter = new JpegConverter; }
        else
        { qDebug() << "Invalid image decoder"; exit(-1); }

        while(!m_die)
        {
            QImage screen_shot = ScreenShot();
            uint32_t screen_width = screen_shot.width();
            uint32_t screen_height = screen_shot.height();

            auto regions = m_region_mapper->GetRegionsOfInterest(screen_shot);
            uint32_t region_num = 0;
            for(const auto &region : regions)
            {
                EncodedImage enc = img_converter->Encode(region.m_img.bits(),
                                                         region.m_width,
                                                         region.m_height,
                                                         m_img_quality_percent);
                auto cmd = CreateFrameCommandPacket(
                            region.m_x,
                            region.m_y,
                            region.m_width,
                            region.m_height,
                            screen_width,
                            screen_height,
                            decoder_type,
                            enc.m_enc_data,
                            enc.m_enc_sz,
                            region_num++,
                            regions.size()
                            );
                SendCommand(ip, *cmd.m_pkt);

                {
                    char name[1024];
                    static int i = 0;
                    sprintf(name, "/home/dev/oosman/foo/frame%i.png", i);
                    QImage img;
                    img = img_converter->Decode(enc.m_enc_data, img);
                    img.save(name);
                    sprintf(name, "/home/dev/oosman/foo/frame%i.txt", i);
                    FILE*fp = fopen(name, "wt");
                    fprintf(fp, "region (x,y,w,h) (%i,%i,%i,%i)\n", region.m_x, region.m_y, region.m_width, region.m_height);
                    fclose(fp);
                    i++;
                }
            }
        }

        delete img_converter;
        m_streaming = false;
    });
}

void ScreenStreamer::StopStreaming(uint32_t ip)
{
    m_die = true;
    if(thread_.valid()) {thread_.wait();}
    m_die = false;
    SendCommand(ip, Command::EventType::StoppedStreaming);
}
