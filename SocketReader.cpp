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

#include "JpegConverter.h"

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
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(m_port);
    m_server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (bind(m_server_socket, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
      close(m_server_socket);
      exit(-1);
    }
//    fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL, 0) | O_NONBLOCK);
    if (listen(m_server_socket, 10) == -1)
    {
      qDebug() << "listen failed";
      close(m_server_socket);
      exit(-1);
    }
}

SocketReader::~SocketReader()
{
    m_stop = true;
    m_reader_thread.wait();
    m_playback_thread.wait();
}

int SocketReader::SendData(uint8_t *buf, int buf_size, const std::string &ip, size_t port)
{
    struct sockaddr_in sa_server;
    memset(&sa_server, 0, sizeof sa_server);
    sa_server.sin_family = AF_INET;
    sa_server.sin_addr.s_addr = inet_addr(ip.c_str());
    sa_server.sin_port = htons(port);

    if(m_client_socket == 0)
    {
        m_client_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (connect(m_client_socket, (struct sockaddr *)&sa_server, sizeof sa_server) == -1)
        {
            qDebug() << "Failed to connect to server " << ip.c_str() << ":" << port;
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
//        usleep(1000 * 30);
    }
    return total_sent;
}

void SocketReader::StartRecieveDataThread()
{
    m_reader_thread = std::async(std::launch::async, [this](){
        ssize_t buffer_size = 1024*1024;
        ssize_t buffer_tail = 0;
        uint8_t *buffer = (uint8_t*) malloc(buffer_size);
        uint8_t *read_buffer = (uint8_t*) malloc(buffer_size);

        struct sockaddr_in sa_client;
        memset(&sa_client, 0, sizeof sa_client);
        sa_client.sin_family = AF_INET;
        socklen_t fromlen = sizeof sa_client;

        bool found_jpeg_head = false;
        while(!m_stop)
        {
            int client_socket = accept(m_server_socket, (struct sockaddr *)&sa_client, &fromlen);
            if (client_socket < 0)
            {
              qDebug() << "accept failed";
              close(m_server_socket);
              exit(-1);
            }
            fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);

            while(!m_stop)
            {
                //osm: todo use select here so we don't block forever
                if(!WaitForSocketIO(client_socket, &m_read_set, nullptr))
                {
                    continue;
                }

                ssize_t bytes_recvd = //recvfrom(client_socket, (void*)read_buffer, buffer_size, 0, (struct sockaddr*)&sa, &fromlen);
                        recv(client_socket, (void*)read_buffer, buffer_size, 0);

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

                    if(!found_jpeg_head)
                    {
                        ssize_t idx = JpegConverter::FindJpegHeader(buffer, buffer_tail);
                        if(idx == -1) continue;
                        memmove(buffer, &buffer[idx], buffer_tail - idx);
                        buffer_tail -= idx;
                        found_jpeg_head = true;
                        continue;
                    }
                    ssize_t idx = JpegConverter::FindJpegHeader(buffer + 10, buffer_tail);
                    if(idx == -1) continue;

                    idx += 10;
                    if(JpegConverter::ValidJpegFooter(buffer[idx-2], buffer[idx-1]))
                    {
                        bool loaded = false;
                        {
                            std::lock_guard<std::mutex> lk(m_mutex);
                            m_display_img[sa_client.sin_addr.s_addr] = JpegConverter::FromJpeg(buffer, idx, m_display_img[sa_client.sin_addr.s_addr]);
                            loaded = !m_display_img[sa_client.sin_addr.s_addr].isNull();
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
        }
    });
}

bool SocketReader::PlaybackImages(std::function<void(const QImage&img, uint32_t from_ip)> renderImageCb)
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
                if (m_stop) {return true;}
                for(const auto &display_image: m_display_img)
                {
                    if(!display_image.isNull()) { return true; }
                }
                return false;
            });
            if(m_stop) { return false; }

            for(uint32_t ip: m_display_img.keys())
            {
                if(!m_display_img[ip].isNull())
                {
                    renderImageCb(m_display_img[ip], ip);
                }
            }
        }
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

