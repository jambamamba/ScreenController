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

#include "Command.h"
#include "Frame.h"
#include "ImageConverterInterface.h"

struct Command;
struct Stats;
class SocketReader
{
public:
    SocketReader(uint16_t port);
    ~SocketReader();
    void StartRecieveDataThread(std::function<void(const Command &pkt, uint32_t ip)> handleCommand);
    int SendData(uint8_t *buf, int buf_size, uint32_t ip, size_t port);
    bool PlaybackImages(std::function<void(const Frame &frame, uint32_t from_ip)> renderImageCb);
    uint16_t GetPort() const;
    static char *IpToString(uint32_t ip);
    static uint32_t IpFromString(const char* ip);
    bool ParseBuffer(uint8_t *buffer,
                      ssize_t buffer_size,
                      ImageConverterInterface::Types decoder_type,
                      uint32_t ip,
                      Stats &stats);
    void ExtractFrame(uint8_t *buffer, ssize_t buffer_size,
                      const Command::Frame &frame,
                      uint32_t ip,
                      Stats &stats);
    void Stop(uint32_t ip);
    void Start(uint32_t ip);

protected:
    struct HeaderMetaData
    {
        ImageConverterInterface::Types m_type = ImageConverterInterface::Types::None;
        ssize_t m_pos = -1;
    };
    HeaderMetaData FindHeader(uint8_t *buffer, ssize_t sz);

    bool m_using_udp = true;
    uint16_t m_port;
    int m_server_socket = 0;
    int m_client_socket = 0;
    fd_set m_read_set;
    fd_set m_write_set;
    std::future<void> m_reader_thread;
    std::future<bool> m_playback_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QMap<uint32_t /*ip*/, std::vector<Frame>> m_regions_of_frame;
    std::map<ImageConverterInterface::Types, std::shared_ptr<ImageConverterInterface>> m_decoders;
    bool m_play = false;
    bool m_die = false;
};
