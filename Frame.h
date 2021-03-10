#pragma once

#include <cstdint>
#include <QImage>

struct Frame {
    uint32_t m_x = 0;
    uint32_t m_y = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    QImage m_img;
    Frame(uint32_t x = 0, uint32_t y = 0, uint32_t width = 0, uint32_t height = 0)
        : m_x(x), m_y(y), m_width(width), m_height(height) {}
};
Q_DECLARE_METATYPE(Frame);
