#include "ScreenStreamer.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>

#include "JpegConverter.h"
#include "SocketReader.h"

namespace  {

}//namespace
ScreenStreamer::ScreenStreamer(SocketReader &socket)
    : m_socket(socket)
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
    return ActiveScreen()->grabWindow(0).toImage();
}

QScreen *ScreenStreamer::ActiveScreen()
{
    return m_screens[ActiveScreenIdx()];
}

void ScreenStreamer::StartStreaming(const std::string &ip, size_t port)
{
    thread_ = std::async(std::launch::async, [this, ip](){
        while(!stop_){
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            QImage screen_shot = ScreenShot();
            if(m_scale_factor*100 != 100)
            {
                screen_shot = screen_shot.scaled(screen_shot.width()*m_scale_factor, screen_shot.height()*m_scale_factor);
            }
            unsigned long jpegSize = 0;
            unsigned char* jpegBuf = nullptr;
            JpegConverter::ToJpeg(screen_shot.bits(), screen_shot.width(), screen_shot.height(), m_jpeg_quality_percent,
                   "osletek", &jpegSize, &jpegBuf);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            m_socket.SendData(jpegBuf, jpegSize, ip, m_socket.GetPort());
            free(jpegBuf);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
            qDebug() << "elapsed " << elapsed;
        }
    });
}
