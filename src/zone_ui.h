#pragma once

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

#include "output.h"

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
    constexpr auto borders = 2;
    constexpr auto minimum_list = 3;
    const auto limit = std::max( 3, terminal_height - shortcuts - borders - minimum_list );
    const auto details = std::clamp( details_rows, 3, limit );
    const auto footer = details + shortcuts + borders;
    return { .details_height = details, .footer_height = footer,
             .list_height = terminal_height - footer };
}

/// Keep comma-separated conditions together when moving to the next line.
inline auto wrap_zone_filter( const std::string &text, const int width ) -> std::vector<std::string>
{
    auto lines = std::vector<std::string>();
    auto line = std::string();
    for( const auto part : text | std::views::split( ',' ) ) {
        auto token = std::string( part.begin(), part.end() );
        if( part.end() != text.end() ) {
            token += ",";
        }
        if( !line.empty() && utf8_width( line + token ) > width ) {
            lines.push_back( line );
            line.clear();
        }
        if( line.empty() ) {
            token.erase( 0, token.find_first_not_of( ' ' ) );
        }
        const auto wrapped = foldstring( token, width );
        for( auto i = 0; i + 1 < static_cast<int>( wrapped.size() ); ++i ) {
            lines.push_back( wrapped[i] );
        }
        if( !wrapped.empty() ) {
            line += wrapped.back();
        }
    }
    if( !line.empty() ) {
        lines.push_back( line );
    }
    return lines;
}
