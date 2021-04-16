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
        return;//todo - start streaming to ip if its different than the one we are streaming to.
    }
    QImage img = ScreenShot();
//    int width = img.width();
//    int height = img.height();

    //osm todo convert to x265 frames and call push_frame:

    constexpr uint32_t timestamp = 0;
    if (_rtp->MediaStream()->push_frame(img.bits(), img.width() * img.height() * 3, timestamp, RTP_NO_FLAGS) != RTP_OK)
    {
        qDebug() << "Failed to send RTP frame!";
    }
}

void ScreenStreamer::StopStreaming(uint32_t ip)
{
    StopThreads();
    _rtp.reset();
    _die = false;
}
