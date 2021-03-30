#include "SocketReader.h"

#include <QImage>
#include <QDebug>

#include <arpa/inet.h>
#include <fcntl.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "CommandMessage.h"
#include "JpegConverter.h"
#include "WebPConverter.h"

extern "C" void mylog(const char *fmt, ...);

namespace  {

//-------------------------------------------------------------------------------
void InitFileDescriptorSet( timeval &tv, int socketfd, fd_set *set )
{
   FD_ZERO( set );
   tv.tv_sec = 3;
   tv.tv_usec = 0;
   FD_SET( socketfd, set );
}

//-------------------------------------------------------------------------------
bool WaitForSocketIO(int socket, fd_set *readset, fd_set *writeset)
{
    struct timeval tv;
    if(readset) {InitFileDescriptorSet( tv, socket, readset );}
    if(writeset) {InitFileDescriptorSet( tv, socket, writeset );}

    int ret = select( socket + 1, readset, writeset, nullptr, &tv );
    if( ret == -1 )
    {
        if(errno == EAGAIN)// try again
        {
            return false;
        }
        else if (errno == EBADF) // file descriptor closed while trying to read from socket
        {
            qDebug() << "error in select call: EBADF, File Closed\n";
            return false;
        }

        qDebug() << "error in select call, errno: " << errno << ", " << strerror(errno) << "\n";
        return false;
    }
    else if( ret == 0 )// timeout
    {
       return false;
    }
    return readset ? FD_ISSET(socket, readset) :
                     writeset ? FD_ISSET(socket, writeset) :
                                false;
}
}//namespace

struct Stats
{
    void Update(ssize_t frame_sz)
    {
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_begin).count();
        m_total_bytes += frame_sz;

//osm
        qDebug() << "frame# " << (m_frame_counter++) << ","
                 << frame_sz/1024
                 << "KBytes, "
                 << (m_frame_counter*1000/elapsed)
                 << "fps, "
                 << (m_total_bytes*8/elapsed)
                 << "kbps.";
        if(elapsed > 1000 * 60)
        {
            m_begin = end;
            m_frame_counter = 0;
            m_total_bytes = 0;
        }
    }
    std::chrono::steady_clock::time_point m_begin = std::chrono::steady_clock::now();
    int m_frame_counter = 0;
    size_t m_total_bytes = 0;
};

SocketReader::SocketReader(uint16_t port)
    : m_port(port)
    , m_decoders{
        {ImageConverterInterface::Types::Command, std::make_shared<CommandMessage>()},
        {ImageConverterInterface::Types::Jpeg, std::make_shared<JpegConverter>()},
        {ImageConverterInterface::Types::Webp, std::make_shared<WebPConverter>()}
    }
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(m_port);
    m_server_socket = socket(PF_INET,
                             m_using_udp ? SOCK_DGRAM:SOCK_STREAM,
                             m_using_udp ? IPPROTO_UDP: IPPROTO_TCP);
    if (bind(m_server_socket, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
      close(m_server_socket);
      exit(-1);
    }
    if(m_using_udp)
    {
        fcntl(m_server_socket, F_SETFL, fcntl(m_server_socket, F_GETFL, 0) | O_NONBLOCK);
    }
    else if (listen(m_server_socket, 10) == -1)
    {
      qDebug() << "listen failed";
      close(m_server_socket);
      exit(-1);
    }
}

SocketReader::~SocketReader()
{
    m_die = true;
    m_reader_thread.wait();
    m_playback_thread.wait();
}

SocketReader::HeaderMetaData SocketReader::FindHeader(uint8_t *buffer, ssize_t sz)
{
    SocketReader::HeaderMetaData meta;
    ImageConverterInterface::Types decoder_type = ImageConverterInterface::Types::Command;
    const auto decoder = m_decoders[decoder_type];
//    for(const auto &decoder : m_decoders)
    {
        ssize_t pos = decoder->FindHeader(buffer, sz);
        if(pos != -1)
        {
            meta.m_pos = pos;
            meta.m_type = decoder_type;//decoder.first;
            return meta;
        }
    }
    return meta;
}

int SocketReader::SendData(uint8_t *buf, int buf_size, uint32_t ip, size_t port)
{
    struct sockaddr_in sa_server;
    memset(&sa_server, 0, sizeof sa_server);
    sa_server.sin_family = AF_INET;
    sa_server.sin_addr.s_addr = ip;
    sa_server.sin_port = htons(port);

    if(m_using_udp)
    {
        m_client_socket = m_server_socket;
    }
    else if(m_client_socket == 0)
    {
        m_client_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (connect(m_client_socket, (struct sockaddr *)&sa_server, sizeof sa_server) == -1)
        {
            qDebug() << "Failed to connect to server " << IpToString(ip) << ":" << port;
            close(m_client_socket);
            m_client_socket = 0;
            return 0;
        }
    }

    int total_sent = 0;
    int datagram_size = 8192*4;
    for(int i =0 ; i < buf_size; i+= datagram_size)
    {
        if(!WaitForSocketIO(m_client_socket, nullptr, &m_write_set))
        {
            continue;
        }
        int bytes_sent = sendto(m_client_socket, buf + i, std::min(datagram_size, buf_size - i), 0,(struct sockaddr*)&sa_server, sizeof sa_server);
        if(bytes_sent < 1)
        {
            qDebug() << "Failed in sendto " << errno << " : " << strerror(errno);
            close(m_client_socket);
            m_client_socket = 0;
            return -1;
            break;
        }
        total_sent += bytes_sent;
//        usleep(1000 * 100);
    }
    return total_sent;
}
void SocketReader::ExtractFrame(uint8_t *buffer,
                                ssize_t buffer_size,
                                const Command::Frame &frame,
                                uint32_t ip,
                                Stats &stats)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    EncodedImage enc(buffer, buffer_size);

    if(frame.m_region_num == 0)
    {
        m_regions_of_frame[ip].clear();
    }
    m_regions_of_frame[ip].push_back(Frame(frame.m_x,
                        frame.m_y,
                        frame.m_width,
                        frame.m_height,
                        frame.m_screen_width,
                        frame.m_screen_height
                        ));
    QImage &img = m_regions_of_frame[ip].back().m_img;
    ImageConverterInterface::Types decoder_type = static_cast<ImageConverterInterface::Types>
            (frame.m_decoder_type);
    auto &decoder = m_decoders[decoder_type];
    img = decoder->Decode(enc, img);
    qDebug() << "recvd frame, region_num" << frame.m_region_num << ", max_regions" << frame.m_max_regions;
    if(frame.m_max_regions == frame.m_region_num+1)
    {
        m_cv.notify_one();
        stats.Update(frame.m_size);
    }
}
bool SocketReader::ParseBuffer(uint8_t *buffer,
                                ssize_t buffer_size,
                                ImageConverterInterface::Types decoder_type,
                                uint32_t ip,
                                Stats &stats)
{
    switch(decoder_type)
    {
    case ImageConverterInterface::Types::Command:
    {
        Command *pkt = (Command*)buffer;
        if(pkt->m_event == Command::EventType::FrameInfo)
        {
//osm
//            qDebug() << "### next frame attributes (x,y,w,h,sz):"
//                     << pkt->u.m_frame.m_x
//                     << pkt->u.m_frame.m_y
//                     << pkt->u.m_frame.m_width
//                     << pkt->u.m_frame.m_height;
            ExtractFrame(pkt->m_tail_bytes,
                         buffer_size,
                         pkt->u.m_frame,
                         ip,
                         stats);
            return true;
        }
        return false;
    }
//    case ImageConverterInterface::Types::Jpeg:
//    case ImageConverterInterface::Types::Webp:
//    {
//        qDebug() << "### got image";
//        Command::Frame frame;
//        frame.m_decoder_type = decoder_type;
//        frame.m_width = 1920;
//        frame.m_height = 1080;
//        ExtractFrame(buffer,
//                     buffer_size,
//                     frame,
//                     ip,
//                     stats);
//        return true;
//    }
    default:
        return false;
    }
}

void SocketReader::Stop(uint32_t ip)
{
    m_play = false;
    if(m_regions_of_frame.find(ip) != m_regions_of_frame.end())
    {
        m_regions_of_frame[ip].clear();
    }
}

void SocketReader::Start(uint32_t ip)
{
    m_play = true;
}

void SocketReader::StartRecieveDataThread(std::function<void(const Command &pkt, uint32_t ip)> handleCommand)
{
    m_reader_thread = std::async(std::launch::async, [this,handleCommand](){
        pthread_setname_np(pthread_self(), "sockrdr");
        ssize_t buffer_size = 1024*1024;
        ssize_t buffer_tail = 0;
        uint8_t *buffer = (uint8_t*) malloc(buffer_size);
        ssize_t read_buffer_size = 8192*10;
        uint8_t *read_buffer = (uint8_t*) malloc(read_buffer_size);

        struct sockaddr_in sa_client;
        memset(&sa_client, 0, sizeof sa_client);
        sa_client.sin_family = AF_INET;
        socklen_t fromlen = sizeof sa_client;

        Stats stats;
        SocketReader::HeaderMetaData header;

        while(!m_die)
        {
            int client_socket = m_using_udp ?
                        m_server_socket :
                        accept(m_server_socket, (struct sockaddr *)&sa_client, &fromlen);
            if (client_socket < 0)
            {
              qDebug() << "accept failed";
              close(m_server_socket);
              exit(-1);
            }
            fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);

            while(!m_die)
            {
                //osm: todo use select here so we don't block forever
                if(!WaitForSocketIO(client_socket, &m_read_set, nullptr))
                {
                    continue;
                }

                ssize_t bytes_recvd = m_using_udp ?
                            recvfrom(client_socket, (void*)read_buffer, read_buffer_size, 0,
                                     (struct sockaddr*)&sa_client, &fromlen):
                            recv(client_socket, (void*)read_buffer, read_buffer_size, 0);

                if(bytes_recvd < 1)
                {
                    qDebug() << "recvfrom failed. Was connection closed? errno " << errno << ": " << strerror(errno);
                    shutdown(client_socket, SHUT_RDWR);
                    close(client_socket);
                    break;
                }
                else
                {
                    if(buffer_size - buffer_tail < bytes_recvd)
                    {
                        qDebug() << "Read buffer is full, dropping received data";
                        continue;
                    }
                    memcpy(&buffer[buffer_tail], read_buffer, bytes_recvd);
                    buffer_tail += bytes_recvd;

                    if(header.m_type == ImageConverterInterface::Types::None)
                    {
                        header = FindHeader(buffer, buffer_tail);
                        if(header.m_type == ImageConverterInterface::Types::None) { continue; }

                        memmove(buffer, &buffer[header.m_pos], buffer_tail - header.m_pos);
                        buffer_tail -= header.m_pos;
                        continue;
                    }
                    auto &decoder = m_decoders[header.m_type];
                    auto next_header = FindHeader(buffer + decoder->HeaderSize(), buffer_tail);
                    if(next_header.m_type == ImageConverterInterface::Types::None) { continue; }

                    next_header.m_pos += decoder->HeaderSize();
                    if(decoder->IsValid(buffer, next_header.m_pos))
                    {
                        if(!ParseBuffer(buffer, next_header.m_pos, header.m_type, sa_client.sin_addr.s_addr, stats) &&
                                (header.m_type == ImageConverterInterface::Types::Command))
                        {
                            handleCommand(*(Command*)buffer, sa_client.sin_addr.s_addr);
                        }
                    }
                    memmove(buffer, &buffer[next_header.m_pos], buffer_tail - next_header.m_pos);
                    buffer_tail -= (next_header.m_pos);
                    header = next_header;
                }

                static size_t total = 0;
                total += bytes_recvd;
                //            mylog("total recvd: %i", total);
            }
        }

        free(buffer);
        free(read_buffer);
    });
}

bool SocketReader::PlaybackImages(std::function<void (const Frame &, uint32_t)> renderImageCb)
{
    m_playback_thread = std::async(std::launch::async, [this,renderImageCb](){
        pthread_setname_np(pthread_self(), "playbk");
        while(!m_die)
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            auto sec = std::chrono::seconds(1);
            m_cv.wait_for(lk, 2*sec, [this]{
                if (m_die) {return true;}
                for(const auto &regions: m_regions_of_frame)
                {
                    for(const auto &region: regions)
                    {
                        if(!region.m_img.isNull()) { return true; }
                    }
                }
                return false;
            });
//            return false;//osm todo
            if(m_die) { return false; }
            if(!m_play) { continue; }

            for(uint32_t ip: m_regions_of_frame.keys())
            {
                for(const auto &region: m_regions_of_frame[ip])
                if(!region.m_img.isNull())
                {
                    renderImageCb(region, ip);
                }
            }
        }
        return true;
    });
    return true;
}

uint16_t SocketReader::GetPort() const
{
    return m_port;
}


char *SocketReader::IpToString(uint32_t ip)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_addr.s_addr = ip;
    return inet_ntoa(sa.sin_addr);
}
uint32_t SocketReader::IpFromString(const char* ip)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    inet_aton(ip, &sa.sin_addr);
    return sa.sin_addr.s_addr;
}

