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

Command *CreateFrameCommandPacket(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint8_t *data, uint32_t data_size)
{
    Command cmd;
    Command *pkt = (Command *)malloc(sizeof (Command) + data_size);
    memcpy(pkt, &cmd, sizeof cmd);

    pkt->m_event = Command::EventType::FrameInfo;
    pkt->u.m_frame.m_x = 0;
    pkt->u.m_frame.m_y = 0;
    pkt->u.m_frame.m_width = width;
    pkt->u.m_frame.m_height = height;
    pkt->u.m_frame.m_size = data_size;
    pkt->m_size = sizeof (Command) + data_size;

    uint8_t tail_bytes[4];
    memcpy(tail_bytes, cmd.m_tail_bytes, sizeof tail_bytes);
    memcpy(pkt->m_tail_bytes, data, data_size);
    memcpy(pkt->m_tail_bytes + data_size, tail_bytes, sizeof tail_bytes);

    qDebug() << "create frame packet of size" << pkt->m_size << ", frame size" << pkt->u.m_frame.m_size;

    return pkt;
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
    return ActiveScreen()->grabWindow(0).toImage().convertToFormat(QImage::Format::Format_RGB888);
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

void ScreenStreamer::StartStreaming(uint32_t ip, int decoder_type)
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
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            QImage screen_shot = ScreenShot();
            screen_shot = ApplyMouseCursor(screen_shot);

            {
                screen_shot = QImage(":/resources/♂_Common_Kingfisher_(Alcedo_atthis)_Photograph_By_Shantanu_Kuveskar,_Mangaon,_Maharashtra,_India.jpg").
                        convertToFormat(QImage::Format::Format_RGB888);
            }
            EncodedImage enc = img_converter->Encode(screen_shot.bits(), screen_shot.width(), screen_shot.height(), m_img_quality_percent);
            Command *pkt = CreateFrameCommandPacket(
                        0, 0,
                        screen_shot.width(),
                        screen_shot.height(),
                        enc.m_enc_data,
                        enc.m_enc_sz
                        );
            SendCommand(ip, *pkt);
            free(pkt);
//            m_socket.SendData(
//                        enc.m_enc_data,
//                        enc.m_enc_sz,
//                        ip,
//                        m_socket.GetPort());
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
//            qDebug() << "elapsed " << elapsed;
            QApplication::processEvents();
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
