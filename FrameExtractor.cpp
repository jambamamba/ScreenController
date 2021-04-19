#include "FrameExtractor.h"

#include <QDebug>

#include "Stats.h"
#include "X265Converter.h"

FrameExtractor::FrameExtractor(std::function <void (const Frame &frame, uint32_t from_ip)> playbackFrameFn)
    : _playbackFrameFn(playbackFrameFn)
{}

FrameExtractor::~FrameExtractor()
{
    m_die = true;
}

void FrameExtractor::ExtractFrame(uint8_t *buffer,
                                  size_t sz,
                                  uint32_t ip)
{
    if(_width == 0 || _height == 0)
    {
        qDebug() << "Width/Height not known! Cannot decode.";
        return;
    }
    EncodedChunk enc(buffer, sz, _width, _height);

    if(!_x265dec)
    {
        _x265dec = std::make_unique<X265Decoder>(_width, _height, [ip,this,sz](const QImage &img){
            Frame frame(0, 0, _width, _height, _width, _height);
            frame.m_img = img;
            _playbackFrameFn(frame, ip);
            _stats.Update(sz);
        });
    }

//    qDebug() << "#### received command packet #" << frame.m_sequence_number << "with payload of size " << chunk._size << frame.m_size;
    EncodedChunk chunk(buffer, sz, _width, _height);
    _x265dec->Decode(ip, _width, _height, chunk);
}

void FrameExtractor::SetDecoderFrameWidthHeight(uint32_t ip, uint32_t width, uint32_t height)
{
    _width = width;
    _height = height;
}

void FrameExtractor::Stop(uint32_t ip)
{
    m_play = false;
    if(_x265dec)
    {
        _x265dec.reset();
    }
}

void FrameExtractor::ReadyToReceive(uint32_t ip)
{
    m_play = true;
}

