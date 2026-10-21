#pragma once

#include <algorithm>
#include <string>

#include "catacharset.h"

struct zone_row_layout {
    int name_width;
    int location_x;
    int vehicle_x;
};

/// Keep distance/direction against the right edge; place a vehicle marker before them.
inline auto make_zone_row_layout( const int width, const std::string &location ) -> zone_row_layout
{
    const auto location_x = std::max( 4, width - utf8_width( location ) );
    return { .name_width = std::max( 1, location_x - 5 ),
             .location_x = location_x, .vehicle_x = location_x - 1 };
}
