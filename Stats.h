#pragma once

#include <chrono>
#include <stdlib.h>

struct Stats
{
    void Update(ssize_t frame_sz);

    std::chrono::steady_clock::time_point m_begin = std::chrono::steady_clock::now();
    int m_frame_counter = 0;
    size_t m_total_bytes = 0;
};
