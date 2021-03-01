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


}//namespace


SocketReader::SocketReader(uint16_t port)
    : m_port(port)
{
    memset(&m_sa, 0, sizeof m_sa);
    m_sa.sin_family = AF_INET;
    m_sa.sin_addr.s_addr = htonl(INADDR_ANY);
    m_sa.sin_port = htons(m_port);
    m_fp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bind(m_fp, (struct sockaddr *)&m_sa, sizeof m_sa) == -1)
    {
      close(m_fp);
      exit(-1);
    }

}

SocketReader::~SocketReader()
{
    m_stop = true;
    m_reader_thread.wait();
    m_playback_thread.wait();
}

int SocketReader::SendData(uint8_t *buf, int buf_size, const std::string &ip, size_t port, int throttle_ms)
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
        int bytes_sent = sendto(m_fp, buf + i, std::min(datagram_size, buf_size - i), 0,(struct sockaddr*)&sa, sizeof sa);
        if(bytes_sent < 1)
        {
            return -1;
            break;
        }
        total_sent += bytes_sent;
        usleep(1000 * throttle_ms);
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
            ssize_t bytes_recvd = recvfrom(m_fp, (void*)read_buffer, buffer_size, 0, (struct sockaddr*)&sa, &fromlen);

//            mylog("recvd %i bytes", recsize);

            {
                std::lock_guard<std::mutex> lk(m_mutex);
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
                {
                    QImage &img = m_display_img[m_display_img_idx];
                    m_display_img_idx = (m_display_img_idx + 1) %2;
                    img.loadFromData(buffer, idx, "JPG");
                    if(!img.isNull())
                    {
                        static int counter = 0;
                        qDebug() << "Complete jpeg recvd " << counter++ << ", bytes " << idx;
//                        renderImageCb(img);
                    }
                    memmove(buffer, &buffer[idx], buffer_tail - idx);
                    buffer_tail -= (idx);
                }
            }
            m_cv.notify_one();

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
                QImage &img = m_display_img[(m_display_img_idx + 1) %2];
                return (m_stop || !img.isNull());
            });
            if(m_stop)
            { return false; }

            QImage &img = m_display_img[(m_display_img_idx + 1) %2];
            if(!img.isNull())
            {
                renderImageCb(img);
            }
        }
    });
    return true;
}

uint16_t SocketReader::GetPort() const
{
    return m_port;
}


