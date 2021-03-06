#pragma once

#include <cstdint>
#include <QImage>

class RegionMapper
{
public:
    struct Region
    {
        ssize_t m_x;
        ssize_t m_y;
        ssize_t m_width;
        ssize_t m_height;
        QImage m_img;
        Region(ssize_t x = 0, ssize_t y = 0, ssize_t width = 0, ssize_t height = 0, const QImage &img = QImage())
            : m_x(x), m_y(y), m_width(width), m_height(height), m_img(img) {}
        void CopyImage(const QImage &img);
    };
    RegionMapper();
    ~RegionMapper();
    std::vector<Region> GetRegionsOfInterest(const QImage &screen_shot);

protected:
    QImage m_prev_screen_shot;
};
