#pragma once

#include <future>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>

struct Command;
class MouseInterface;
class QScreen;
class SocketReader;
struct ImageConverterInterface;
class ScreenStreamer : public QObject
{
    Q_OBJECT

public:
    ScreenStreamer(SocketReader &socket, QObject *parent);
    ~ScreenStreamer();
    void SendCommand(uint32_t ip, uint16_t event);
    QImage ScreenShot();
    QScreen *ActiveScreen();
public slots:
    void StartStreaming(uint32_t ip, int decoder_type);

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
