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


SocketReader::SocketReader(uint16_t port)
    : m_port(port)
{
    memset(&m_sa, 0, sizeof m_sa);
    m_sa.sin_family = AF_INET;
    m_sa.sin_addr.s_addr = htonl(INADDR_ANY);
    m_sa.sin_port = htons(m_port);
    m_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bind(m_socket, (struct sockaddr *)&m_sa, sizeof m_sa) == -1)
    {
      close(m_socket);
      exit(-1);
    }
    fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL, 0) | O_NONBLOCK);
}

SocketReader::~SocketReader()
{
    m_stop = true;
    m_reader_thread.wait();
    m_playback_thread.wait();
}

int SocketReader::SendData(uint8_t *buf, int buf_size, const std::string &ip, size_t port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(ip.c_str());
    sa.sin_port = htons(port);

    int total_sent = 0;
    int datagram_size = 8192;
    for(int i =0 ; i < buf_size; i+= datagram_size)
    {
        if(!WaitForSocketIO(m_socket, nullptr, &m_write_set))
        {
            continue;
        }
        int bytes_sent = sendto(m_socket, buf + i, std::min(datagram_size, buf_size - i), 0,(struct sockaddr*)&sa, sizeof sa);
        if(bytes_sent < 1)
        {
            return -1;
            break;
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

ssize_t FindJpegHeader(uint8_t* buffer, ssize_t buffer_tail)
{
    ssize_t idx = 0;
    for(; idx < buffer_tail - 10; ++idx)
    {
//jpeg header                ff d8 ff e0 00 10 4a 46  49 46
        if(buffer[idx] == 0xff &&
                buffer[idx+1] == 0xd8 &&
                buffer[idx+2] == 0xff &&
                buffer[idx+3] == 0xe0 &&
                buffer[idx+4] == 0x00 &&
                buffer[idx+5] == 0x10 &&
                buffer[idx+6] == 0x4a &&
                buffer[idx+7] == 0x46 &&
                buffer[idx+8] == 0x49 &&
                buffer[idx+9] == 0x46
                )
        {
            return idx;
        }
    }
    return -1;
}
void SocketReader::StartRecieveDataThread()
{
    m_reader_thread = std::async(std::launch::async, [this](){
        ssize_t buffer_size = 1024*1024;
        ssize_t buffer_tail = 0;
        uint8_t *buffer = (uint8_t*) malloc(buffer_size);
        uint8_t *read_buffer = (uint8_t*) malloc(buffer_size);

        ssize_t jpeg_head = -1;
        while(!m_stop)
        {
            struct sockaddr_in sa;
            memset(&sa, 0, sizeof sa);
            sa.sin_family = AF_INET;
            socklen_t fromlen = sizeof IN_CLASSA_HOST;
            //osm: todo use select here so we don't block forever
            if(!WaitForSocketIO(m_socket, &m_read_set, nullptr))
            {
                continue;
            }

            ssize_t bytes_recvd = recvfrom(m_socket, (void*)read_buffer, buffer_size, 0, (struct sockaddr*)&sa, &fromlen);

//            mylog("recvd %i bytes", recsize);

            {
                if(buffer_size - buffer_tail < bytes_recvd)
                {
                    qDebug() << "Read buffer is full, dropping received data";
                    exit(-1);
                }
                memcpy(&buffer[buffer_tail], read_buffer, bytes_recvd);
                buffer_tail += bytes_recvd;

                if(jpeg_head == -1)
                {
                    ssize_t idx = FindJpegHeader(buffer, buffer_tail);
                    if(idx == -1) continue;
                    memmove(buffer, &buffer[idx], buffer_tail - idx);
                    buffer_tail -= idx;
                    jpeg_head = 0;
                    continue;
                }
                ssize_t idx = FindJpegHeader(buffer + 10, buffer_tail);
                if(idx == -1) continue;

                idx += 10;
                if(buffer[idx-2] == 0xff && buffer[idx-1] == 0xd9)
                {
                    bool loaded = false;
                    {
                        std::lock_guard<std::mutex> lk(m_mutex);
                        loaded = m_display_img.loadFromData(buffer, idx, "JPG");
                    }
                    if(loaded)
                    {
                        m_cv.notify_one();
                    }
                }
                memmove(buffer, &buffer[idx], buffer_tail - idx);
                buffer_tail -= (idx);
            }

            static size_t total = 0;
            total += bytes_recvd;
//            mylog("total recvd: %i", total);
        }
    });
}

bool SocketReader::PlaybackImages(std::function<void(const QImage&img)> renderImageCb)
{
    if(m_playback_thread.valid())
    {
        m_stop = true;
        m_playback_thread.wait();
        m_stop = false;
    }
    m_playback_thread = std::async(std::launch::async, [this,renderImageCb](){
        while(true)
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait_for(lk, std::chrono::seconds(1), [this]{
                return (m_stop || !m_display_img.isNull());
            });
            if(m_stop)
            { return false; }

            if(!m_display_img.isNull())
            {
                static int counter = 0;
//                qDebug() << "Complete jpeg recvd " << counter++;
                renderImageCb(m_display_img);
            }
        }
    });
    return true;
}

uint16_t SocketReader::GetPort() const
{
    return m_port;
}


