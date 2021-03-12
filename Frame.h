#pragma once

#include <cstdint>
#include <QImage>

struct Frame {
    ssize_t m_x = 0;
    ssize_t m_y = 0;
    ssize_t m_width = 0;
    ssize_t m_height = 0;
    ssize_t m_screen_width = 0;
    ssize_t m_screen_height = 0;
    QImage m_img;
    Frame(ssize_t x = 0, ssize_t y = 0, ssize_t width = 0, ssize_t height = 0, ssize_t screen_width = 0, ssize_t screen_height = 0)
        : m_x(x), m_y(y), m_width(width), m_height(height), m_screen_width(screen_width), m_screen_height(screen_height) {}
};
Q_DECLARE_METATYPE(Frame);
