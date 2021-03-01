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
void SocketReader::StartRecieveDataThread(std::function<void(const QImage&img)> renderImageCb)
{
    m_buffer = (uint8_t*) malloc(m_buffer_size);

    m_reader_thread = std::async(std::launch::async, [this,renderImageCb](){
        int read_buffer_size = m_buffer_size;
        static uint8_t * read_buffer = (uint8_t*) malloc(read_buffer_size);

        ssize_t jpeg_head = -1;
        while(!m_stop)
        {
            struct sockaddr_in sa;
            memset(&sa, 0, sizeof sa);
            sa.sin_family = AF_INET;
            socklen_t fromlen = sizeof IN_CLASSA_HOST;
            //osm: todo use select here so we don't block forever
            ssize_t bytes_recvd = recvfrom(m_fp, (void*)read_buffer, read_buffer_size, 0, (struct sockaddr*)&sa, &fromlen);

//            mylog("recvd %i bytes", recsize);

            {
                std::lock_guard<std::mutex> lk(m_mutex);
                if(m_buffer_size - m_buffer_tail < bytes_recvd)
                {
                    qDebug() << "Read buffer is full, dropping received data";
                    exit(-1);
                }
                memcpy(&m_buffer[m_buffer_tail], read_buffer, bytes_recvd);
                m_buffer_tail += bytes_recvd;

                if(jpeg_head == -1)
                {
                    ssize_t idx = FindJpegHeader(m_buffer, m_buffer_tail);
                    if(idx == -1) continue;
                    memmove(m_buffer, &m_buffer[idx], m_buffer_tail - idx);
                    m_buffer_tail -= idx;
                    jpeg_head = 0;
                    continue;
                }
                ssize_t idx = FindJpegHeader(m_buffer + 10, m_buffer_tail);
                if(idx == -1) continue;
                static int counter1 = 0;

                idx += 10;
                {
                    QImage img;
                    img.loadFromData(m_buffer, idx, "JPG");
                    if(!img.isNull())
                    {
                        static int counter = 0;
                        qDebug() << "Complete jpeg recvd " << counter++ << ", bytes " << idx;
                        renderImageCb(img);
                    }
                    memmove(m_buffer, &m_buffer[idx], m_buffer_tail - idx);
                    m_buffer_tail -= (idx);
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
    return false;//osm
    m_render_image_cb = renderImageCb;
    m_playback_thread = std::async(std::launch::async, [this,renderImageCb](){
        constexpr int buf_size = 8192*10;
        uint8_t buf[buf_size];
        size_t jpeg_data_sz = 1024*1024;
        size_t jpeg_data_pos = 0;
        uint8_t* jpeg_data = nullptr;
        jpeg_data = (uint8_t*)malloc(jpeg_data_sz);
        while(true)
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait_for(lk, std::chrono::seconds(1), [this]{
                return (m_stop || m_buffer_tail > 0);
            });
            if(m_stop)
            { return false; }

            bool found_jpeg_header = false;
            ssize_t idx = 0;
            for(; idx < m_buffer_tail - 10; ++idx)
            {
//jpeg header                ff d8 ff e0 00 10 4a 46  49 46
                if(m_buffer[idx] == 0xff &&
                        m_buffer[idx+1] == 0xd8 &&
                        m_buffer[idx+2] == 0xff &&
                        m_buffer[idx+3] == 0xe0 &&
                        m_buffer[idx+4] == 0x00 &&
                        m_buffer[idx+5] == 0x10 &&
                        m_buffer[idx+6] == 0x4a &&
                        m_buffer[idx+7] == 0x46 &&
                        m_buffer[idx+8] == 0x49 &&
                        m_buffer[idx+9] == 0x46
                        )
                {
                    found_jpeg_header = true;
                    break;
                }
            }
            if(found_jpeg_header)
            {
                memcpy(&jpeg_data[jpeg_data_pos], m_buffer, idx);
                jpeg_data_pos += idx;

                static int counter = 0;
                QImage img;
                img.loadFromData(jpeg_data, jpeg_data_pos, "JPG");
                if(!img.isNull())
                {
                    qDebug() << "Complete jpeg recvd " << counter++;
                    renderImageCb(img);
                }

                memcpy(jpeg_data, m_buffer, idx);
                jpeg_data_pos = idx;

                memmove(m_buffer, &m_buffer[idx], m_buffer_tail - idx);
                m_buffer_tail -= idx;
            }
            else if(jpeg_data_pos > 0 && m_buffer_tail >= 10)
            {
                memcpy(&jpeg_data[jpeg_data_pos], m_buffer, m_buffer_tail - 10);
                jpeg_data_pos += (m_buffer_tail - 10);

                memmove(m_buffer, &m_buffer[m_buffer_tail - 10], 10);
                m_buffer_tail -= 10;
            }
        }
    });
    return true;
}

uint16_t SocketReader::GetPort() const
{
    return m_port;
}


