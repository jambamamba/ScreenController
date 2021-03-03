#include "ScreenStreamer.h"

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
    stop_ = true;
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

void ScreenStreamer::SendCommand(uint32_t ip, const CommandMessage::Packet &pkt)
{
    m_socket.SendData((uint8_t*)&pkt, sizeof pkt, ip, m_socket.GetPort());
}

void ScreenStreamer::StartStreaming(uint32_t ip, int decoder_type)
{
    thread_ = std::async(std::launch::async, [this, ip, decoder_type](){
        ImageConverterInterface *img_converter = nullptr;
        if(decoder_type == ImageConverterInterface::Types::Webp)
        { img_converter = new WebPConverter; }
        else if (decoder_type == ImageConverterInterface::Types::Jpeg)
        { img_converter = new JpegConverter; }
        else
        { qDebug() << "Invalid image decoder"; exit(-1); }

        while(!stop_){
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            QImage screen_shot = ScreenShot();
            screen_shot = ApplyMouseCursor(screen_shot);
            if(m_scale_factor*100 != 100)
            {
                screen_shot = screen_shot.scaled(screen_shot.width()*m_scale_factor, screen_shot.height()*m_scale_factor);
            }
            EncodedImage enc = img_converter->Encode(screen_shot.bits(), screen_shot.width(), screen_shot.height(), m_jpeg_quality_percent);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            m_socket.SendData(enc.m_enc_data, enc.m_enc_sz, ip, m_socket.GetPort());
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
            qDebug() << "elapsed " << elapsed;
        }

        delete img_converter;
    });
}
