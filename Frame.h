#pragma once

#include <cstdint>
#include <QImage>

struct Frame {
    uint32_t m_x = 0;
    uint32_t m_y = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_screen_width = 0;
    uint32_t m_screen_height = 0;
    QImage m_img;
    Frame(uint32_t x = 0, uint32_t y = 0, uint32_t width = 0, uint32_t height = 0, uint32_t screen_width = 0, uint32_t screen_height = 0)
        : m_x(x), m_y(y), m_width(width), m_height(height), m_screen_width(screen_width), m_screen_height(screen_height) {}
    void Update(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t screen_width, uint32_t screen_height)
    {
        m_x = x;
        m_y = y;
        m_width = width;
        m_height = height;
        m_screen_width = screen_width;
        m_screen_height = screen_height;
    }
};
Q_DECLARE_METATYPE(Frame);
