#include "ScreenStreamer.h"

#include <algorithm>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

#include "ImageConverterInterface.h"
#include "LockFreeRingBuffer.h"
#include "MouseInterface.h"
#include "RegionMapper.h"
#if defined(Win32) || defined(Win64)
#include "WindowsMouse.h"
#elif defined(Linux)
#include "X11Mouse.h"
#else
#include "NullMouse.h"
#endif
#include "JpegConverter.h"
#include "WebPConverter.h"
#include "X265Converter.h"

namespace  {
static QImage &bltCursorOnImage(QImage &img, const QImage &cursor, const QPoint &pos)
{
    QPainter painter(&img);
    painter.drawImage(pos, cursor);
    painter.end();
    return img;
}

Command *NewFrameCommandPacket(
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        uint32_t screen_width,
        uint32_t screen_height,
        uint32_t decoder_type,
        const uint8_t *data,
        ssize_t data_size,
        uint32_t region_num,
        uint32_t max_regions,
        uint32_t sequence_number)
{
    Command cmd;
//    CommandAutoDestruct cmd_auto(sizeof (Command) + data_size);
    Command *pkt =  (Command *)malloc(sizeof (Command) + data_size);//cmd_auto._cmd;
    memcpy(pkt, &cmd, sizeof cmd);

    pkt->m_event = Command::EventType::FrameInfo;
    pkt->u.m_frame.m_sequence_number = sequence_number++;
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

    return pkt;
}
}//namespace

ScreenStreamer::ScreenStreamer(
        std::function<void(uint32_t ip, const Command &cmd)> sendCommand,
        QObject *parent)
    : QObject(parent)
#if defined(Win32) || defined(Win64)
    ,m_mouse(new WindowsMouse(parent))
#elif defined(Linux)
    ,m_mouse(new X11Mouse(parent))
#else
    ,m_mouse(new NullMouse(parent))
#endif
    ,_region_mapper(std::make_unique<RegionMapper>())
    ,_sendCommand(sendCommand)
    , _ring_buffer(std::make_unique<LockFreeRingBuffer<Command*>>(100))
{
    InitAvailableScreens();
}

void ScreenStreamer::StopThreads()
{
    _die = true;
    if(_webp_thread.valid()) {_webp_thread.wait();}
    if(_x265enc) { delete _x265enc; _x265enc = nullptr; }
    if(_rgb_buffer) { free(_rgb_buffer); _rgb_buffer = nullptr; }
    if(_img_converter) { delete _img_converter; _img_converter = nullptr; }
}

ScreenStreamer::~ScreenStreamer()
{
    StopThreads();
}

void ScreenStreamer::InitAvailableScreens()
{
    _screens = QGuiApplication::screens();
    std::sort(_screens.begin(), _screens.end(),
              [](QScreen *a, QScreen *b){
        return (a->geometry().x() != b->geometry().x()) ?
                    a->geometry().x() < b->geometry().x() :
                    a->geometry().y() < b->geometry().y();
    });
}

int ScreenStreamer::ActiveScreenIdx() const
{
    int screen_idx = 0;
    for(auto screen : _screens)
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
    return _screens[ActiveScreenIdx()];
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

void ScreenStreamer::StreamWebpImages(uint32_t ip, uint32_t decoder_type, ImageConverterInterface *img_converter)
{
    _webp_thread = std::async(std::launch::async, [this, ip, decoder_type, img_converter](){
        _streaming = true;
        _sequence_num_encoded = 0;
        pthread_setname_np(pthread_self(), "scrncap");

        while(!_die)
        {
            QImage screen_shot = ScreenShot();
            uint32_t screen_width = screen_shot.width();
            uint32_t screen_height = screen_shot.height();

            auto regions = _region_mapper->GetRegionsOfInterest(screen_shot);
            uint32_t region_num = 0;
            for(const auto &region : regions)
            {
                EncodedChunk enc = img_converter->Encode(region._img.bits(),
                                                         region._width,
                                                         region._height,
                                                         _img_quality_percent);
                SendEncodedData(ip,decoder_type,std::move(region),screen_width,screen_height,enc,region_num++,regions.size());
#if 0//osm fix me: sometimes the encoded image is really bad quality
                {
                    QImage img = QImage(region.m_width, region.m_height, QImage::Format::Format_RGB888);;
                    WebPConverter cnv;
                    img = cnv.Decode(enc);
                    if(!img.isNull())
                    {
                        char name[1024];
                        static int i = 0;
                        sprintf(name, "/home/dev/oosman/foo/frame%i.png", i);
                        img.save(name);
                        sprintf(name, "/home/dev/oosman/foo/frame%i.txt", i);
                        FILE*fp = fopen(name, "wt");
                        fprintf(fp, "region (x,y,w,h) (%i,%i,%i,%i), enc.m_enc_sz %i\n", region.m_x, region.m_y, region.m_width, region.m_height, enc.m_enc_sz);
                        fclose(fp);
                        i++;
                    }
                }
#endif
            }
        }

        _streaming = false;
    });
}

void ScreenStreamer::StreamX265(uint32_t ip, uint32_t decoder_type, int width, int height, uint32_t sequence_number)
{
#if osm//osm testing purpose
    static X265Decoder x265dec(width, height, [ip](const QImage &img){
        if(img.isNull())
        { return; }
        //osm
        {
            char filename[128] = {0};
            static int i = 0;
            sprintf(filename, "/tmp/foo/frame%i.png", i++);
            img.save(filename);
        }
    });
#endif
    if(sequence_number != 0)
    {
        //osm TODO go to ring buffer and find sequence_number, then start sending frames from there, update _sequence_num_to_send to sequence_number
        qDebug() << "#### go back to frame #" << sequence_number;
        _sequence_num_to_send = sequence_number;
        return;
    }

    if(_rgb_buffer) { free(_rgb_buffer); }
    _rgb_buffer = (char*) malloc(width * 3 * height);

    if(_x265enc) { delete _x265enc; }
    _sequence_num_encoded = 0;
    _x265enc = new X265Encoder(
                width,
                height,
                [this,width,height](char **data, ssize_t *bytes, int width_, int height_, float quality_factor){
           QImage img = ScreenShot();
           //osm assert width_ == width and height_ == height
           memcpy(_rgb_buffer, img.bits(), width * 3 * height);
           *bytes = width * 3 * height;
           *data = _rgb_buffer;
           return *bytes;
       },
       [this,ip,decoder_type,width,height](EncodedChunk enc){
            SendEncodedData(ip,decoder_type,Region(0,0,width,height),width,height,enc,1,1);
#if osm//osm
            x265dec.Decode(ip, width, height, enc);
#endif
           return enc._size;
       }
    );
}
void ScreenStreamer::SendEncodedData(
        uint32_t ip,
        uint32_t decoder_type,
        const Region &&region,
        int screen_width,
        int screen_height,
        const EncodedChunk &enc,
        uint32_t region_num,
        size_t num_regions) const
{
    uint8_t *data = enc._data;
    size_t enc_size = enc._size;
    size_t total_bytes_buffered = 0;
    while(true)
    {
        size_t chunk_sz = std::min(static_cast<int>(enc_size), _datagram_size);
        auto *cmd = NewFrameCommandPacket(
                    region._x,region._y,
                    region._width,region._height,
                    screen_width,screen_height,
                    decoder_type,
                    data + total_bytes_buffered,
                    chunk_sz,
                    region_num,num_regions,
                    _sequence_num_encoded++
                    );

        //osm TODO do not send now but insert in ring buffer, have another thread send it out from correct sequence_num position
        _ring_buffer->Insert(&cmd, 1, [this](){
            Command *tmp = nullptr;
            _ring_buffer->Remove(&tmp, 1);
            free(tmp);
            return true;
        });

        total_bytes_buffered += chunk_sz;
        if(chunk_sz < enc_size)
        {
            enc_size -= chunk_sz;
        }
        else
        {
            break;
        }
    }

    for(size_t idx = 0; idx < _ring_buffer->Count(); ++idx)
    {
        Command *cmd = _ring_buffer->GetAt(idx);
        if(cmd->u.m_frame.m_sequence_number == _sequence_num_to_send)
        {
            _sendCommand(ip, *cmd);
            usleep(100);
            _sequence_num_to_send++;
            qDebug() << "#### sending command packet #" << cmd->u.m_frame.m_sequence_number << "with payload of size " << cmd->u.m_frame.m_size;
        }
//        if(cmd->u.m_frame.m_sequence_number < _sequence_num_to_send)
//        {
//            break;
//        }
    }
}
void ScreenStreamer::StartStreaming(uint32_t ip, uint32_t sequence_number, uint32_t decoder_type)
{
    if(_streaming)
    {
        return;//todo - start streaming to ip if its different than the one we are streaming to.
    }
    QImage img = ScreenShot();
    int width = img.width();
    int height = img.height();

    if(_img_converter) { delete _img_converter; }
    switch(decoder_type)
    {
    case ImageConverterInterface::Types::Webp:
        _img_converter = new WebPConverter;
        StreamWebpImages(ip, decoder_type, _img_converter);
        break;
    case ImageConverterInterface::Types::Jpeg:
        _img_converter = new JpegConverter;
        StreamWebpImages(ip, decoder_type, _img_converter);
        break;
    case ImageConverterInterface::Types::X265:
        StreamX265(ip, decoder_type, width, height, sequence_number);
        break;
    default:
        qDebug() << "Invalid image decoder"; exit(-1);
        break;
    }
}

void ScreenStreamer::StopStreaming(uint32_t ip)
{
    StopThreads();
    _die = false;
}
