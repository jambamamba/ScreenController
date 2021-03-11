#pragma once

#include <cstdint>
#include <QImage>

class RegionMapper
{
public:
    struct Region
    {
        size_t m_x;
        size_t m_y;
        size_t m_width;
        size_t m_height;
        QImage m_img;
        Region(size_t x = 0, size_t y = 0, size_t width = 0, size_t height = 0, const QImage &img = QImage())
            : m_x(x), m_y(y), m_width(width), m_height(height), m_img(img) {}
        void CopyImage(const QImage &img);
    };
    RegionMapper();
    ~RegionMapper();
    std::vector<Region> GetRegionsOfInterest(const QImage &screen_shot);

protected:
    QImage m_prev_screen_shot;
};
