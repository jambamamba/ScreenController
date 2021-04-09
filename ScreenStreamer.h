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

#include "ImageConverterInterface.h"

class MouseInterface;
class QScreen;
class RegionMapper;
struct Command;
struct EncodedChunk;
struct ImageConverterInterface;
struct Region;
struct X265Encoder;
class ScreenStreamer : public QObject
{
    Q_OBJECT

public:
    ScreenStreamer(std::function<void(uint32_t ip, const Command &cmd)> sendCommand, QObject *parent);
    ~ScreenStreamer();
    QImage ScreenShot();
    QScreen *ActiveScreen();

    static constexpr int _img_quality_percent = 5;
    static constexpr int _datagram_size = 1024 * 8;
    static constexpr ImageConverterInterface::Types _default_decoder = ImageConverterInterface::Types::X265;

public slots:
    void StartStreaming(uint32_t ip, uint32_t sequence_number, uint32_t decoder_type);
    void StopStreaming(uint32_t ip);

protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);
    void StreamWebpImages(uint32_t ip, uint32_t decoder_type, ImageConverterInterface *img_converter);
    void StreamX265(uint32_t ip, uint32_t decoder_type, int width, int height, uint32_t sequence_number);
    void SendEncodedData(uint32_t ip, uint32_t decoder_type, const Region &&region, int screen_width, int screen_height, const EncodedChunk &enc,
                         uint32_t region_num, size_t num_regions) const;

    std::future<void> _webp_thread;
    QList<QScreen *> _screens;
    MouseInterface *m_mouse;
    std::unique_ptr<RegionMapper> _region_mapper;
    bool _streaming = false;
    char *_rgb_buffer = nullptr;
    ImageConverterInterface *_img_converter = nullptr;
    X265Encoder *_x265enc = nullptr;
    mutable uint32_t _sequence_num = 0;
    std::function<void(uint32_t ip, const Command &cmd)> _sendCommand = nullptr;
    std::atomic<bool> _die;
    void StopThreads();
};
