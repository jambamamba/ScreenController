#include "RegionMapper.h"

#include <cstdlib>
#include <QDebug>

namespace  {

struct Distance
{
    size_t m_dx = -1;
    size_t m_dy = -1;
    Distance(size_t dx = -1, size_t dy = -1)
        : m_dx(dx), m_dy(dy) {}
    bool operator<(const Distance& rhs)
    {
        return ( (m_dx*m_dx + m_dy*m_dy) < (rhs.m_dx*rhs.m_dx + rhs.m_dy*rhs.m_dy) );
    }
};

uint8_t *DiffImages(const uint8_t * b0,
                    const uint8_t * b1,
                    size_t width,
                    size_t height)
{
    uint8_t * mask =  (uint8_t *) malloc(width * 3 * height);
    memset(mask, 0, width * height * 3);
    for(size_t i = 0; i < width * 3 * height; ++i)
    {
        uint8_t diff = std::abs(b0[i] - b1[i]);
        diff = diff >> 1;
        if(diff > 0)
        {
            mask[i] = diff;
        }
    }
    return mask;
}

void GrowRegionToIncludePoint(RegionMapper::Region &region, ssize_t x, ssize_t y)
{
    if(x < region.m_x) { region.m_x = x; }
    else if(x >= region.m_x + region.m_width) { region.m_width = x - region.m_x; }

    if(y < region.m_y) { region.m_y = y; }
    else if(y >= region.m_y + region.m_height) { region.m_height = y - region.m_y; }
}

std::vector<RegionMapper::Region> &UpdateRegions(
        std::vector<RegionMapper::Region> &regions,
        const uint8_t *mask,
        size_t cols,
        size_t rows)
{
    for(size_t pos = 0; pos < rows * cols * 3; pos += 3)
    {
        uint8_t r = (mask[pos]);
        uint8_t g = (mask[pos + 1]);
        uint8_t b = (mask[pos + 2]);
        if( r == 0 && g == 0 && b == 0)
        {
            continue;
        }

//             0,  1,  2,  3,  4,  5,  6,  7,  8,  9
//            10, 11, 12, 13, 14, 15, 16, 17, 18, 19
//            20, 21, 22, 23, 24, 25, 26, 27, 28, 29
//            30, 31, 32, 33, 34, 35, 36, 37, 38, 39

        size_t x = pos/cols;
        size_t y = pos - cols*x;

        RegionMapper::Region *closest_region = nullptr;
        Distance distance_to_closest_region;
        bool already_in_region = false;
        for(auto &region : regions)
        {
            ssize_t dx = (x < region.m_x) ?
                        (region.m_x - x) :
                        (x >= region.m_x + region.m_width) ?
                            (x - (region.m_x + region.m_width)) : 0;
            ssize_t dy = (y < region.m_y) ?
                        (region.m_y - y) :
                        (y >= region.m_y + region.m_height) ?
                            (y - (region.m_y + region.m_height)) : 0;

            if(dx == 0 && dy == 0)
            {
                already_in_region = true;
                break;
            }

            Distance distance(dx, dy);
            if(distance < distance_to_closest_region)
            {
                distance_to_closest_region = distance;
                closest_region = &region;
            }
        }

        if(already_in_region)
        {continue;}

        if(!closest_region)
        {
            RegionMapper::Region region(x, y, 1, 1);
            regions.push_back(region);
            continue;
        }

        if(distance_to_closest_region < (1920/10)*(1920/10))
        {
            GrowRegionToIncludePoint(*closest_region, x, y);
        }
    }
    return regions;
}
}//namespace
RegionMapper::RegionMapper()
{}

RegionMapper::~RegionMapper()
{}

std::vector<RegionMapper::Region> RegionMapper::GetRegionsOfInterest(const QImage &screen_shot)
{
    std::vector<RegionMapper::Region> regions;

    if(m_prev_screen_shot.isNull())
    {
        regions.push_back(Region(0, 0, screen_shot.width(), screen_shot.height(), screen_shot));
        m_prev_screen_shot = QImage(screen_shot.width(), screen_shot.height(), screen_shot.format());
        memcpy(m_prev_screen_shot.bits(), screen_shot.bits(), screen_shot.width() * screen_shot.height() * 3);
        return regions;
    }

    int cmp = memcmp(screen_shot.bits(), m_prev_screen_shot.bits(),
                     screen_shot.width() * screen_shot.height() * 3);
    uint8_t *mask = DiffImages(screen_shot.bits(),
                           m_prev_screen_shot.bits(),
                           screen_shot.width(),
                           screen_shot.height());
    regions = UpdateRegions(regions, mask, screen_shot.width(), screen_shot.height());
    free(mask);

    int idx = 0;
    for(auto &region : regions)
    {
        qDebug() << "region (x,y,w,h)" << region.m_x << region.m_y << region.m_width << region.m_height;
        region.CopyImage(screen_shot);
    }
    qDebug() << "regions" << idx;

    m_prev_screen_shot = QImage(screen_shot.width(), screen_shot.height(), screen_shot.format());
    memcpy(m_prev_screen_shot.bits(), screen_shot.bits(), screen_shot.width() * screen_shot.height() * 3);
    return regions;
}

void RegionMapper::Region::CopyImage(const QImage &src)
{
    m_img = QImage(m_width, m_height, src.format());

    for(ssize_t j = 0; j < m_height; ++j)
    {
        memcpy(&m_img.bits()[j * m_width * 3], &src.bits()[m_x + m_y * src.width() * 3], m_width * 3);
    }
}
