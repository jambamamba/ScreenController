#pragma once

#include <future>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>

struct ImageConverterInterface;
class QScreen;
class SocketReader;
class ScreenStreamer
{
public:
    ScreenStreamer(SocketReader &socket);
    ~ScreenStreamer();
    void StartStreaming(const std::string &ip, size_t port, ImageConverterInterface &img_converter);
    QImage ScreenShot();
    QScreen *ActiveScreen();
protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;

    std::future<void> thread_;
    QList<QScreen *> m_screens;
    SocketReader &m_socket;
    int m_jpeg_quality_percent = 10;
    float m_scale_factor = 1;//.85;
    bool stop_ = false;
};
