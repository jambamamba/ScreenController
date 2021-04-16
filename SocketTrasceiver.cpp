#include "SocketTrasceiver.h"

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

#include "Command.h"
#include "CommandMessage.h"

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

SocketTrasceiver::SocketTrasceiver(uint16_t port)
    : m_port(port)
    , m_decoders{
        {ImageConverterInterface::Types::Command, std::make_shared<CommandMessage>()}
//        ,{ImageConverterInterface::Types::Jpeg, std::make_shared<JpegConverter>()}
//        ,{ImageConverterInterface::Types::Webp, std::make_shared<WebPConverter>()}
    }
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(m_port);
    _socket = socket(PF_INET,
                             SOCK_DGRAM,
                             IPPROTO_UDP);
    if (bind(_socket, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
      close(_socket);
      exit(-1);
    }
    fcntl(_socket, F_SETFL, fcntl(_socket, F_GETFL, 0) | O_NONBLOCK);
}

SocketTrasceiver::~SocketTrasceiver()
{
    m_die = true;
    m_reader_thread.wait();
}

SocketTrasceiver::HeaderMetaData SocketTrasceiver::FindHeader(uint8_t *buffer, ssize_t sz)
{
    SocketTrasceiver::HeaderMetaData meta;
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



void SocketTrasceiver::StartRecieveDataThread(std::function<void(const Command &pkt, uint32_t ip)> handleCommand)
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

        SocketTrasceiver::HeaderMetaData header;

        while(!m_die)
        {
            fcntl(_socket, F_SETFL, fcntl(_socket, F_GETFL, 0) | O_NONBLOCK);

            while(!m_die)
            {
                if(!WaitForSocketIO(_socket, &m_read_set, nullptr))
                {
                    continue;
                }

                ssize_t bytes_recvd = recvfrom(_socket, (void*)read_buffer, read_buffer_size, 0,
                                     (struct sockaddr*)&sa_client, &fromlen);

                if(bytes_recvd < 1)
                {
                    qDebug() << "recvfrom failed. Was connection closed? errno " << errno << ": " << strerror(errno);
                    shutdown(_socket, SHUT_RDWR);
                    close(_socket);
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
                    if(decoder->IsValid(buffer, next_header.m_pos) &&
                            ImageConverterInterface::Types::Command == header.m_type)
                    {
                        handleCommand(*(Command*)buffer, sa_client.sin_addr.s_addr);
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
int SocketTrasceiver::SendData(uint8_t *buf, int buf_size, uint32_t ip)
{
    struct sockaddr_in sa_server;
    memset(&sa_server, 0, sizeof sa_server);
    sa_server.sin_family = AF_INET;
    sa_server.sin_addr.s_addr = ip;
    sa_server.sin_port = htons(m_port);

    int total_sent = 0;
    int datagram_size = 8192*4;
    for(int i =0, iter = 0 ; i < buf_size; i+= datagram_size, iter++)
    {
        if(!WaitForSocketIO(_socket, nullptr, &m_write_set))
        {
            continue;
        }
        int bytes_sent = sendto(_socket, buf + i, std::min(datagram_size, buf_size - i), 0,(struct sockaddr*)&sa_server, sizeof sa_server);
        if(bytes_sent < 1)
        {
            qDebug() << "Failed in sendto " << errno << " : " << strerror(errno);
            close(_socket);
            _socket = 0;
            return -1;
            break;
        }
        total_sent += bytes_sent;
        //TODO: Need a ack system, should only send next packet if previous one arrived.
    }
    return total_sent;
}

char *SocketTrasceiver::IpToString(uint32_t ip)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_addr.s_addr = ip;
    return inet_ntoa(sa.sin_addr);
}
uint32_t SocketTrasceiver::IpFromString(const char* ip)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    inet_aton(ip, &sa.sin_addr);
    return sa.sin_addr.s_addr;
}

void SocketTrasceiver::SendCommand(uint32_t ip, uint16_t event)
{
    Command cmd(event);
    SendCommand(ip, cmd);
}

void SocketTrasceiver::SendCommand(uint32_t ip, const Command &cmd)
{
    SendData((uint8_t*)&cmd, cmd.m_size, ip);

    Command noop(Command::EventType::None);
    SendData((uint8_t*)&noop, sizeof(Command), ip);
}

