#pragma once

#include <QImage>
#include <QMap>
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
    int SendData(uint8_t *buf, int buf_size, const std::string &ip, size_t port);
    bool PlaybackImages(std::function<void(const QImage&img, uint32_t from_ip)> renderImageCb);
    uint16_t GetPort() const;
    static char *IpToString(uint32_t ip);
    static uint32_t IpFromString(const char* ip);
protected:
    uint16_t m_port;
    int m_server_socket = 0;
    int m_client_socket = 0;
    fd_set m_read_set;
    fd_set m_write_set;
    std::future<void> m_reader_thread;
    std::future<bool> m_playback_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QMap<int /*ip*/, QImage> m_display_img;
    bool m_stop = false;
};
