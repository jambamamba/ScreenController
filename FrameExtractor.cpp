#include "FrameExtractor.h"

#include <QDebug>

#include "Stats.h"
#include "X265Converter.h"

FrameExtractor::FrameExtractor()
{

}

FrameExtractor::~FrameExtractor()
{
    m_die = true;
}

void FrameExtractor::ExtractX265Frame(
        const EncodedChunk &chunk,
        uint32_t ip
        )
{
    if(!_x265dec)
    {
        size_t sz = chunk._size;
        _x265dec = std::make_unique<X265Decoder>(chunk._width, chunk._height, [ip,this,sz](const QImage &img){
//            ProcessImageFromDecoder(frame,ip,img);
            _stats.Update(sz);
        });
    }

//    qDebug() << "#### received command packet #" << frame.m_sequence_number << "with payload of size " << chunk._size << frame.m_size;
    _x265dec->Decode(ip, chunk._width, chunk._height, chunk);
}

bool FrameExtractor::PlaybackImages(
        std::function<void (const Frame &, uint32_t)> renderImageCb)
{
    m_playback_thread = std::async(std::launch::async, [this,renderImageCb](){
        pthread_setname_np(pthread_self(), "playbk");
        while(!m_die)
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            auto sec = std::chrono::seconds(1);
            m_cv.wait_for(lk, 2*sec, [this]{
                if (m_die) {return true;}
                for(const auto &regions: m_regions_of_frame)
                {
                    for(const auto &region: regions)
                    {
                        if(!region.m_img.isNull()) { return true; }
                    }
                }
                return false;
            });
            if(m_die) { return false; }
            if(!m_play) { continue; }

            for(uint32_t ip: m_regions_of_frame.keys())
            {
                for(auto it = m_regions_of_frame[ip].begin(); it != m_regions_of_frame[ip].end(); )
                {
                    if(!it->m_img.isNull())
                    {
                        renderImageCb(*it, ip);
                    }
                    it = m_regions_of_frame[ip].erase(it);
                }
            }
        }
        return true;
    });
    return true;
}

uint32_t FrameExtractor::ExtractFrame(uint8_t *buffer,
                                  size_t sz,
                                  uint32_t ip)
{
    EncodedChunk enc(buffer, sz, width, height);

    return ExtractX265Frame(enc, ip);
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

