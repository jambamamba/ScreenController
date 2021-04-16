#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>
#include <QImage>
#include <QTimer>

#include "Command.h"
#include "ImageConverterInterface.h"
#include "LockFreeRingBuffer.cpp"

struct CommandAutoDestruct
{
    CommandAutoDestruct(size_t size)
    {
        _cmd = (Command *)malloc(size);
    }
    ~CommandAutoDestruct()
    {
        free(_cmd);
    }
    CommandAutoDestruct(const CommandAutoDestruct& rhs) = delete;
    const CommandAutoDestruct& operator=(const CommandAutoDestruct& rhs) = delete;
    Command *_cmd;
};
template class LockFreeRingBuffer<Command*>;

class MouseInterface;
class QScreen;
class RegionMapper;
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
    static constexpr int _datagram_size = 1500 - sizeof(Command); // to determine MTU size, ping -s $((1500 - 28)) -M do 192.168.1.22 -c 1, or on Internet ping -s $((96 - 28)) -M do 8.8.8.8 -c 1
    static constexpr int _ring_buffer_size = 10000;
    static constexpr int _retry_request_frame_timeout_ms = 1000;
    static constexpr int _send_next_frame_timeout_ms = 20;
    static constexpr ImageConverterInterface::Types _default_decoder = ImageConverterInterface::Types::X265;
public slots:
    void StartStreaming(uint32_t ip, uint32_t sequence_number, uint32_t decoder_type);
    void StopStreaming(uint32_t ip);

signals:
    void SendNextFrame(uint32_t ip) const;
protected slots:
    void SendFrameBySequenceNumber(uint32_t ip) const;

protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);
    void StreamWebpImages(uint32_t ip, uint32_t decoder_type, ImageConverterInterface *img_converter);
    void StreamX265(uint32_t ip, uint32_t decoder_type, int width, int height, uint32_t sequence_number);
    void BufferEncodedData(uint32_t ip, uint32_t decoder_type, const Region &&region, int screen_width, int screen_height, const EncodedChunk &enc,
                         uint32_t region_num, size_t num_regions) const;
    void InitializeX265Decoder(uint32_t ip, uint32_t decoder_type, int width, int height);

    std::future<void> _webp_thread;
    QList<QScreen *> _screens;
    MouseInterface *m_mouse;
    std::unique_ptr<RegionMapper> _region_mapper;
    bool _streaming = false;
    char *_rgb_buffer = nullptr;
    ImageConverterInterface *_img_converter = nullptr;
    X265Encoder *_x265enc = nullptr;
    mutable uint32_t _sequence_num_encoded = 0;
    mutable std::atomic<uint32_t> _sequence_num_to_send = 0;
    std::function<void(uint32_t ip, const Command &cmd)> _sendCommand = nullptr;
    std::unique_ptr<LockFreeRingBuffer<Command*>> _ring_buffer;
    mutable std::mutex _ring_buffer_mutex;
    QTimer *_send_next_frame_timer = nullptr;
    QMetaObject::Connection _send_next_frame_timer_conn;
    std::atomic<bool> _die;
    void StopThreads();
};
