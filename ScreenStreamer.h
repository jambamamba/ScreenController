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
class RegionMapper;
class SocketReader;
struct ImageConverterInterface;
struct X265Encoder;
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
    void StartStreaming(uint32_t ip, uint32_t decoder_type);
    void StopStreaming(uint32_t ip);

protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);
    void StreamWebpImages(uint32_t ip, uint32_t decoder_type, ImageConverterInterface *img_converter);
    void StreamX265(uint32_t ip, uint32_t decoder_type, int width, int height);

    std::future<void> _webp_thread;
    QList<QScreen *> _screens;
    SocketReader &_socket;
    int _img_quality_percent = 5;
    MouseInterface *m_mouse;
    std::unique_ptr<RegionMapper> _region_mapper;
    bool _streaming = false;
    char *_rgb_buffer = nullptr;
    ImageConverterInterface *_img_converter = nullptr;
    X265Encoder *_x265enc = nullptr;
    std::atomic<bool> _die;
    void StopThreads();
};
