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
#include <libavutil/avutil.h>

#include "ImageConverterInterface.h"

struct Command;
struct Frame;
struct Stats;
class SocketTrasceiver
{
public:
    SocketTrasceiver(uint16_t port);
    ~SocketTrasceiver();
    void StartRecieveDataThread(std::function<void(const Command &pkt, uint32_t ip)> handleCommand);
    static char *IpToString(uint32_t ip);
    static uint32_t IpFromString(const char* ip);
    void SendCommand(uint32_t ip,
                     uint16_t event);
    void SendCommand(uint32_t ip,
                     const Command &cmd);

protected:
    int SendData(uint8_t *buf, int buf_size, uint32_t ip);
    struct HeaderMetaData
    {
        ImageConverterInterface::Types m_type = ImageConverterInterface::Types::None;
        ssize_t m_pos = -1;
    };
    HeaderMetaData FindHeader(uint8_t *buffer, ssize_t sz);

    uint16_t m_port;
    int _socket = 0;
    fd_set m_read_set;
    fd_set m_write_set;
    std::map<ImageConverterInterface::Types, std::shared_ptr<ImageConverterInterface>> m_decoders;
    std::future<void> m_reader_thread;
    bool m_die = false;
};
