#pragma once

#include <QMap>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <memory>

#include "Command.h"
#include "Frame.h"
#include "ImageConverterInterface.h"
#include "Stats.h"

class X265Decoder;
class FrameExtractor
{
public:
    FrameExtractor(std::function <void (const Frame &frame, uint32_t from_ip)> playbackFrameFn);
    ~FrameExtractor();
    void Stop(uint32_t ip);
    void ReadyToReceive(uint32_t ip);
    void ExtractFrame(uint8_t *buffer,
                      size_t sz,
                      uint32_t ip);
    void SetDecoderFrameWidthHeight(
            uint32_t ip,
            uint32_t width,
            uint32_t height);
signals:
    void PlaybackImage(const Frame &frame, uint32_t from_ip);
protected:
    std::unique_ptr<X265Decoder> _x265dec;
    std::function <void (const Frame &frame, uint32_t from_ip)> _playbackFrameFn = nullptr;
    Stats _stats;
    uint32_t _width = 0;
    uint32_t _height = 0;
    bool m_play = false;
    bool m_die = false;
};
