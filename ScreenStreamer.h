#pragma once

#include <future>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>

class MouseInterface;
class QScreen;
class SocketReader;
struct ImageConverterInterface;
class ScreenStreamer
{
public:
    ScreenStreamer(SocketReader &socket, QObject *parent);
    ~ScreenStreamer();
    void SendCommand(uint32_t ip);
    void StartStreaming(uint32_t ip, size_t port, ImageConverterInterface &img_converter);
    QImage ScreenShot();
    QScreen *ActiveScreen();
protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);

    std::future<void> thread_;
    QList<QScreen *> m_screens;
    SocketReader &m_socket;
    int m_jpeg_quality_percent = 10;
    float m_scale_factor = 1;//.85;
    MouseInterface *m_mouse;
    bool stop_ = false;
};
