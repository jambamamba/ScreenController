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
#include "UvgRTP.h"
#include "X265Converter.h"

namespace  {
static QImage &bltCursorOnImage(QImage &img, const QImage &cursor, const QPoint &pos)
{
    QPainter painter(&img);
    painter.drawImage(pos, cursor);
    painter.end();
    return img;
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
    ,_sendCommand(sendCommand)
{
    InitAvailableScreens();
}

void ScreenStreamer::StopThreads()
{
    _die = true;
    if(_x265enc) { delete _x265enc; _x265enc = nullptr; }
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

void ScreenStreamer::StartStreaming(uint32_t ip)
{
    if(!_rtp)
    {
        _rtp = std::make_unique<UvgRTP>(ip, 8888, 8889);
        QImage img = ScreenShot();
        Command cmd(Command::EventType::FrameInfo);
        cmd.u._frame.width = img.width();
        cmd.u._frame.height = img.height();
        _sendCommand(ip, cmd);
        StartEncoding(img.width(), img.height());
        return;//todo - start streaming to ip if its different than the one we are streaming to.
    }
}

void ScreenStreamer::StopStreaming(uint32_t ip)
{
    StopThreads();
    _rtp.reset();
    _die = false;
}

void ScreenStreamer::StartEncoding(int width, int height)
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

    if(_rgb_buffer) { free(_rgb_buffer); }
    _rgb_buffer = (char*) malloc(width * 3 * height);

    if(_x265enc) { delete _x265enc; }
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
       [this](EncodedChunk enc){

        constexpr uint32_t timestamp = 0;
        if (_rtp->MediaStream()->push_frame(enc._data, enc._size, timestamp, RTP_NO_FLAGS) != RTP_OK)
        {
            qDebug() << "Failed to send RTP frame!";
        }

#if osm//osm
            x265dec.Decode(ip, width, height, enc);
#endif
           return enc._size;
       }
    );
}
