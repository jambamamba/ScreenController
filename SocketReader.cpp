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
    memset(&sa_, 0, sizeof sa_);
    sa_.sin_family = AF_INET;
    sa_.sin_addr.s_addr = htonl(INADDR_ANY);
    sa_.sin_port = htons(m_port);
    fp_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bind(fp_, (struct sockaddr *)&sa_, sizeof sa_) == -1)
    {
      close(fp_);
      exit(-1);
    }

}

SocketReader::~SocketReader()
{
    stop_ = true;
    reader_thread_.wait();
    playback_thread_.wait();
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
        int bytes_sent = sendto(fp_, buf + i, std::min(datagram_size, buf_size - i), 0,(struct sockaddr*)&sa, sizeof sa);
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

void SocketReader::StartRecieveDataThread()
{
    int buf_size = 8192*10;
    static uint8_t * buf = (uint8_t*) malloc(buf_size);

    reader_thread_ = std::async(std::launch::async, [this, buf_size](){
        while(!stop_)
        {
            struct sockaddr_in sa;
            memset(&sa, 0, sizeof sa);
            sa.sin_family = AF_INET;
            socklen_t fromlen = sizeof IN_CLASSA_HOST;
            //osm: todo use select here so we don't block forever
            ssize_t recsize = recvfrom(fp_, (void*)buf, buf_size, 0, (struct sockaddr*)&sa, &fromlen);

//            mylog("recvd %i bytes", recsize);

            {
                std::lock_guard<std::mutex> lk(m_);
                for(int i = 0; i < recsize; ++i)
                {
                    buffer_.push_back(buf[i]);
                }
            }
            cv_.notify_one();

            static size_t total = 0;
            total += recsize;
//            mylog("total recvd: %i", total);
        }
    });
}

int SocketReader::ReadSocketData(uint8_t *buf, int buf_size)
{
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait_for(lk, std::chrono::seconds(1), [this]{
        return (stop_ || buffer_.size() > 0);
    });
    if(stop_)
    { return 0; }
    int i;
    for(i = 0; i < buf_size && buffer_.size() > 0; ++i)
    {
        buf[i] = buffer_.at(0);
        buffer_.erase(buffer_.begin());
    }
    return i;
}
bool SocketReader::PlaybackImages(std::function<void(const QImage&img)> renderImageCb)
{
    renderImageCb_ = renderImageCb;
    playback_thread_ = std::async(std::launch::async, [this,renderImageCb](){
        constexpr int buf_size = 8192*10;
        uint8_t buf[buf_size];
        size_t jpeg_data_sz = 1024*1024;
        size_t jpeg_data_pos = 0;
        uint8_t* jpeg_data = nullptr;
        jpeg_data = (uint8_t*)malloc(jpeg_data_sz);
        while(true)
        {
            int bytes_read = ReadSocketData(buf, buf_size);
            if(bytes_read <= 0)
            {
                usleep(1000 * 1000);
                continue;
            }

            bool found_jpeg_header = false;
            ssize_t idx = 0;
            for(; idx < bytes_read - 10; ++idx)
            {
//jpeg header                ff d8 ff e0 00 10 4a 46  49 46
                if(buf[idx] == 0xff &&
                        buf[idx+1] == 0xd8 &&
                        buf[idx+2] == 0xff &&
                        buf[idx+3] == 0xe0 &&
                        buf[idx+4] == 0x00 &&
                        buf[idx+5] == 0x10 &&
                        buf[idx+6] == 0x4a &&
                        buf[idx+7] == 0x46 &&
                        buf[idx+8] == 0x49 &&
                        buf[idx+9] == 0x46
                        )
                {
                    found_jpeg_header = true;
                    break;
                }
            }
            if(found_jpeg_header)
            {
                memcpy(&jpeg_data[jpeg_data_pos], buf, idx);
                jpeg_data_pos += idx;
                static int counter = 0;
                QImage img;
                img.loadFromData(jpeg_data, jpeg_data_pos, "JPG");
                if(!img.isNull())
                {
                    qDebug() << "Complete jpeg recvd " << counter++;
                    renderImageCb(img);
                }

                jpeg_data_pos = 0;
                memcpy(&jpeg_data[jpeg_data_pos], &buf[idx], bytes_read-idx);
                jpeg_data_pos += bytes_read-idx;
            }
            else
            {
                memcpy(&jpeg_data[jpeg_data_pos], buf, bytes_read - 10);
                jpeg_data_pos += bytes_read - 10;
            }
        }
    });
    return true;
}

uint16_t SocketReader::GetPort() const
{
    return m_port;
}


