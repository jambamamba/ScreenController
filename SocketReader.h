#pragma once

#include <QImage>
#include <condition_variable>
#include <future>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "libavutil/avutil.h"

class SocketReader
{
public:
    SocketReader(uint16_t port);
    ~SocketReader();
    void StartRecieveDataThread();
    int SendData(uint8_t *buf, int buf_size, const std::string &ip, size_t port, int throttle_ms);
    bool PlaybackImages(std::function<void(const QImage&img)> renderImageCb);
    uint16_t GetPort() const;
protected:
    int ReadSocketData(uint8_t *buf, int buf_size);
    uint16_t m_port;
    int fp_;
    struct sockaddr_in sa_;
    std::future<void> reader_thread_;
    std::future<void> playback_thread_;
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<uint8_t> buffer_;
    std::function<void(const QImage&img)> renderImageCb_ = nullptr;
    bool stop_ = false;
};
