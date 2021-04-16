#include "UvgRTP.h"

#include <QDebug>

#include "SocketTrasceiver.h"

namespace {
void receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    if (!frame)
    {
        qDebug() << "Invalid frame received!";
        return;
    }

    auto cb = (std::function<void (uint8_t*, size_t)> *) arg;
    if(cb)
    {
        (*cb)(frame->payload, frame->payload_len);
    }

    /* Now we own the frame. Here you could give the frame to the application
     * if f.ex "arg" was some application-specific pointer
     *
     * arg->copy_frame(frame) or whatever
     *
     * When we're done with the frame, it must be deallocated manually */
    (void)uvgrtp::frame::dealloc_frame(frame);
}
}//namespace

UvgRTP::UvgRTP(uint32_t ip,
               int remote_port,
               int source_port,
               std::function<void (uint8_t*, size_t)> cb)
    :_sess(_ctx.create_session(SocketTrasceiver::IpToString(ip)))
    ,_hevc(_sess->create_stream(remote_port, source_port, RTP_FORMAT_H265, RTP_NO_FLAGS))
    ,_receiveCallback(cb)
{
    if(_receiveCallback)
    {
        _hevc->install_receive_hook(&_receiveCallback, receive_hook);
    }
}

UvgRTP::~UvgRTP()
{
    _ctx.destroy_session(_sess);
    _sess = nullptr;
}

uvgrtp::media_stream *UvgRTP::MediaStream() const
{
    return _hevc;
}

