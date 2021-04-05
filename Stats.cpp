#include "Stats.h"

#include <QDebug>

void Stats::Update(ssize_t frame_sz)
{
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_begin).count();
    m_total_bytes += frame_sz;

//osm
    qDebug() << "frame# " << (m_frame_counter++) << ","
             << frame_sz/1024
             << "KBytes, "
             << (m_frame_counter*1000/elapsed)
             << "fps, "
             << (m_total_bytes*8/elapsed)
             << "kbps.";
    if(elapsed > 1000 * 60)
    {
    m_begin = end;
    m_frame_counter = 0;
    m_total_bytes = 0;
    }
}

