#pragma once

#include <functional>
#include <lib.hh>

class UvgRTP
{
public:
    UvgRTP(uint32_t ip, int remote_port, int source_port, std::function<void (uint8_t*, size_t)> cb = nullptr);
    ~UvgRTP();

    uvgrtp::media_stream *MediaStream() const;
protected:
    uvgrtp::context _ctx;
    uvgrtp::session *_sess = nullptr;
    uvgrtp::media_stream *_hevc = nullptr;
    std::function<void (uint8_t*, size_t)> _receiveCallback = nullptr;
};
