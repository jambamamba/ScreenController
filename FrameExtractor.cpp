#include "FrameExtractor.h"

#include <QDebug>

#include "JpegConverter.h"
#include "Stats.h"
#include "WebPConverter.h"
#include "X265Converter.h"

FrameExtractor::FrameExtractor()
{

}

FrameExtractor::~FrameExtractor()
{
    m_die = true;
    m_playback_thread.wait();
}

uint32_t FrameExtractor::ExtractX265Frame(
        const Command::Frame &frame,
        uint32_t ip,
        const EncodedChunk &chunk)
{
    if(_prev_sequence_number+1 != frame.m_sequence_number)
    {
        qDebug() << "#### skipped frame, expecting" << (_prev_sequence_number+1) << ", instead got" << frame.m_sequence_number;
        return _prev_sequence_number+1;
    }
    if(!_x265dec || frame.m_sequence_number == 0)
    {
        _prev_sequence_number = -1;
        _x265dec = std::make_unique<X265Decoder>(frame.m_width, frame.m_height, [ip,frame,this](const QImage &img){
            ProcessImageFromDecoder(frame,ip,img);
        });
    }

    qDebug() << "#### received command packet #" << frame.m_sequence_number << "with payload of size " << chunk._size << frame.m_size;
    _x265dec->Decode(ip, frame.m_width, frame.m_height, chunk);
    _prev_sequence_number = frame.m_sequence_number;
    return _prev_sequence_number+1;
}

void FrameExtractor::ProcessImageFromDecoder(
        const Command::Frame &frame,
        uint32_t ip,
        const QImage &img)
{
    m_regions_of_frame[ip].push_back(Frame(frame.m_x,
                                           frame.m_y,
                                           frame.m_width,
                                           frame.m_height,
                                           frame.m_screen_width,
                                           frame.m_screen_height
                                           ));
    if(img.isNull())
    { return; }
    m_regions_of_frame[ip].back().m_img = img;
#ifdef osm//osm
    {
        char filename[128] = {0};
        static int i = 0;
        sprintf(filename, "/tmp/foo/frame%i.png", i++);
        img.save(filename);
    }
#endif
    m_cv.notify_one();
    _stats.Update(frame.m_size);
}
uint32_t FrameExtractor::ExtractWebpFrame(
        const Command::Frame &frame,
        uint32_t ip,
        const EncodedChunk &enc,
        std::shared_ptr<ImageConverterInterface> decoder)
{
    std::lock_guard<std::mutex> lk(m_mutex);

    //osm TODO: now the frame is no longer a complete frame! Use region_num field to aggregate data of same regions
    if(frame.m_region_num == 0)//&& no region with this region_num exists in m_regions_of_frame
    {
        m_regions_of_frame[ip].clear();
    }
    //if no region with this region_num exists in m_regions_of_frame
    m_regions_of_frame[ip].push_back(Frame(frame.m_x,
                        frame.m_y,
                        frame.m_width,
                        frame.m_height,
                        frame.m_screen_width,
                        frame.m_screen_height
                        ));
    //else find region with same region_num in the map, and aggregate the data!

    QImage &img = m_regions_of_frame[ip].back().m_img;
    if(img.isNull())
    {
        img = QImage(frame.m_width, frame.m_height, QImage::Format::Format_RGB888);
    }
    img = decoder->Decode(enc);
    if(!img.isNull())
    {
        qDebug() << "recvd frame"
                 << "region_num" << frame.m_region_num
                 << ", max_regions" << frame.m_max_regions
                 << ", region size" << frame.m_x << frame.m_y << frame.m_width << frame.m_height;
#if 0//osm
        {
            char name[1024];
            static int i = 0;
            sprintf(name, "/home/dev/oosman/foo/frame%i.png", i);
            img.save(name);
            sprintf(name, "/home/dev/oosman/foo/frame%i.txt", i);
            FILE*fp = fopen(name, "wt");
            fprintf(fp, "region (x,y,w,h) (%i,%i,%i,%i), frame.m_size %i\n", frame.m_x, frame.m_y, frame.m_width, frame.m_height, frame.m_size);
            fclose(fp);
            i++;
        }
#endif
        if(frame.m_max_regions == frame.m_region_num+1)
        {
            m_cv.notify_one();
            _stats.Update(frame.m_size);
        }
    }
    return 0;//osm todo request next frame#
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
                for(const auto &region: m_regions_of_frame[ip])
                if(!region.m_img.isNull())
                {
                    renderImageCb(region, ip);
                }
            }
        }
        return true;
    });
    return true;
}

uint32_t FrameExtractor::ExtractFrame(uint8_t *buffer,
                                  const Command::Frame &frame,
                                  uint32_t ip)
{
    ImageConverterInterface::Types decoder_type = static_cast<ImageConverterInterface::Types>
            (frame.m_decoder_type);
    EncodedChunk enc(buffer, frame.m_size, frame.m_width, frame.m_height);

    switch(decoder_type)
    {
    case ImageConverterInterface::Types::X265:
        return ExtractX265Frame(frame, ip, enc);
    default:
//osm        return ExtractWebpFrame(frame, ip, enc, m_decoders[decoder_type]);
        break;
    }
    return 0;
}

void FrameExtractor::Stop(uint32_t ip)
{
    m_play = false;
    if(m_regions_of_frame.find(ip) != m_regions_of_frame.end())
    {
        m_regions_of_frame[ip].clear();
    }
    if(_x265dec)
    {
        _x265dec.reset();
    }
}

void FrameExtractor::ReadyToReceive(uint32_t ip)
{
    m_play = true;
}

