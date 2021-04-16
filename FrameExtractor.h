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
                      const Command::Frame &frame,
                      uint32_t ip);
    bool PlaybackImages(
            std::function<void(const Frame &frame, uint32_t from_ip)> renderImageCb);
protected:
    uint32_t ExtractX265Frame(const Command::Frame &frame,
                          uint32_t ip,
                          const EncodedChunk &chunk);
    uint32_t ExtractWebpFrame(const Command::Frame &frame,
                          uint32_t ip,
                          const EncodedChunk &enc,
                          std::shared_ptr<ImageConverterInterface> decoder);
    void ProcessImageFromDecoder(const Command::Frame &frame,
                                 uint32_t ip,
                                 const QImage &img);

    QMap<uint32_t /*ip*/, std::vector<Frame>> m_regions_of_frame;
    std::unique_ptr<X265Decoder> _x265dec;
    uint32_t _prev_sequence_number = -1;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::future<bool> m_playback_thread;
    Stats _stats;
    bool m_play = false;
    bool m_die = false;
};
