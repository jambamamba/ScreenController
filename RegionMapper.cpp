#include "RegionMapper.h"

#include <cstdlib>

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

QImage DiffImages(const QImage &img1, const QImage &img2)
{
    if(img1.isNull() ||
            img2.isNull() ||
            img1.width() != img2.width() ||
            img1.height() != img2.height() ||
            img1.format() != img2.format() ||
            img1.format() != QImage::Format::Format_RGB888)
    {
        return QImage();
    }
    QImage diff(img1.width(), img1.height(), img1.format());
    for(ssize_t i = 0; i < img1.width() * 3; ++i)
    {
        for(ssize_t j = 0; j < img1.height(); ++j)
        {
            ssize_t pos = i * img1.width() * 3 + j;
            if(std::abs( (ssize_t)img1.bits()[pos] - (ssize_t)img2.bits()[pos] ) >> 1 > 0)
            {
                diff.bits()[pos] = ( (ssize_t)img1.bits()[pos] - (ssize_t)img2.bits()[pos] ) >> 1;
            }
        }
    }
    return diff;
}

void GrowRegionToIncludePoint(RegionMapper::Region &region, ssize_t x, ssize_t y)
{
    if(x < region.m_x) { region.m_x = x; }
    else if(x >= region.m_x + region.m_width) { region.m_width = x - region.m_x; }

    if(y < region.m_y) { region.m_y = y; }
    else if(y >= region.m_y + region.m_height) { region.m_height = y - region.m_y; }
}

std::vector<RegionMapper::Region> &UpdateRegions(std::vector<RegionMapper::Region> &regions, const QImage &mask)
{
    for(ssize_t i = 0; i < mask.width(); ++i)
    {
        for(ssize_t j = 0; j < mask.height(); ++j)
        {
            RegionMapper::Region *closest_region = nullptr;
            Distance distance_to_closest_region;
            bool already_in_region = false;
            for(auto &region : regions)
            {
                size_t dx = (i < region.m_x) ?
                            (region.m_x - i) :
                            (i >= region.m_x + region.m_width) ?
                                (region.m_x + region.m_width - i) : -1;
                size_t dy = (j < region.m_y) ?
                            (region.m_y - j) :
                            (j >= region.m_y + region.m_height) ?
                                (region.m_y + region.m_height - i) : -1;

                if(dx == (size_t)-1 && dy == (size_t)-1)
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
                RegionMapper::Region region(i, j, 1, 1);
                regions.push_back(region);
                continue;
            }

            if(distance_to_closest_region < (1920/10)*(1920/10))
            {
                GrowRegionToIncludePoint(*closest_region, i, j);
            }
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
        m_prev_screen_shot = screen_shot;
        return regions;
    }

    auto mask = DiffImages(screen_shot, m_prev_screen_shot);
    regions = UpdateRegions(regions, mask);

    for(auto &region : regions)
    {
        region.CopyImage(screen_shot);
    }

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
