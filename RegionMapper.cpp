#include "RegionMapper.h"

RegionMapper::RegionMapper()
{}

RegionMapper::~RegionMapper()
{}

std::vector<RegionMapper::Region> RegionMapper::GetRegions(const QImage &screen_shot)
{
    std::vector<RegionMapper::Region> regions;

    if(m_prev_screen_shot.isNull())
    {
        regions.push_back(Region(0, 0, screen_shot.width(), screen_shot.height(), screen_shot));
    }
    return regions;
}
