#pragma once

#include <cstdint>
#include <QImage>

struct Region
{
    ssize_t _x;
    ssize_t _y;
    ssize_t _width;
    ssize_t _height;
    QImage _img;
    Region(ssize_t x = 0, ssize_t y = 0, ssize_t width = 0, ssize_t height = 0, const QImage &img = QImage())
        : _x(x), _y(y), _width(width), _height(height), _img(img) {}
    void CopyImage(const QImage &img);
};

class RegionMapper
{
public:
    RegionMapper() = default;
    ~RegionMapper() = default;
    std::vector<Region> GetRegionsOfInterest(const QImage &screen_shot);

protected:
    QImage _prev_screen_shot;
};
