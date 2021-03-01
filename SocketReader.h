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
    uint16_t m_port;
    int m_fp;
    struct sockaddr_in m_sa;
    std::future<void> m_reader_thread;
    std::future<bool> m_playback_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QImage m_display_img;
    bool m_stop = false;
};
