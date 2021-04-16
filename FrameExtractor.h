#pragma once

#include <QMap>
#include <condition_variable>
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
    FrameExtractor();
    ~FrameExtractor();
    void Stop(uint32_t ip);
    void ReadyToReceive(uint32_t ip);
    uint32_t ExtractFrame(uint8_t *buffer,
                      size_t sz,
                      uint32_t ip);
    bool PlaybackImages(
            std::function<void(const Frame &frame, uint32_t from_ip)> renderImageCb);
protected:
    void ExtractX265Frame(
            const EncodedChunk &chunk,
            uint32_t ip);
    std::unique_ptr<X265Decoder> _x265dec;
    Stats _stats;
    bool m_play = false;
    bool m_die = false;
};
