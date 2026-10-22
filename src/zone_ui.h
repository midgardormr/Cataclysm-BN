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

struct zone_panel_layout {
    int details_height;
    int footer_height;
    int list_height;
};

/// Grow details upwards while keeping shortcuts and at least one list row on screen.
inline auto make_zone_panel_layout( const int terminal_height,
                                    const int details_rows ) -> zone_panel_layout
{
    constexpr auto shortcuts = 5;
    constexpr auto borders = 3;
    constexpr auto minimum_list = 3;
    const auto limit = std::max( 3, terminal_height - shortcuts - borders - minimum_list );
    const auto details = std::clamp( details_rows, 3, limit );
    const auto footer = details + shortcuts + borders;
    return { .details_height = details, .footer_height = footer,
             .list_height = terminal_height - footer };
}
