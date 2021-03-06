#pragma once

#include <future>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>
#include <QImage>

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
    void SendCommand(uint32_t ip, const Command &pkt);
    QImage ScreenShot();
    QScreen *ActiveScreen();
public slots:
    void StartStreaming(uint32_t ip, int decoder_type);
    void StopStreaming(uint32_t ip);

protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);

    std::future<void> thread_;
    QList<QScreen *> m_screens;
    SocketReader &m_socket;
    int m_jpeg_quality_percent = 25;
    float m_scale_factor = 1;//.85;
    MouseInterface *m_mouse;
    bool m_streaming = false;
    bool m_die = false;
};
