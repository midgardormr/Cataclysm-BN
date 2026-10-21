#pragma once

#include <algorithm>
#include <string>

#include "catacharset.h"

/// Remove a leading private-use icon (including icons made of several glyphs).
/// Ordinary text and icons elsewhere in a user-supplied name are preserved.
inline auto zone_name_without_icon( const std::string &name ) -> std::string
{
    const auto text = utf8_to_utf32( name );
    const auto first_text = std::ranges::find_if_not( text, []( const auto ch ) {
        return ( ch >= 0xe000 && ch <= 0xf8ff ) ||
               ( ch >= 0xf0000 && ch <= 0xffffd ) ||
               ( ch >= 0x100000 && ch <= 0x10fffd );
    } );
    if( first_text == text.begin() ) {
        return name;
    }
    const auto first_name = std::find_if_not( first_text, text.end(), []( const auto ch ) {
        return ch == U' ';
    } );
    return first_name == text.end() ? name : utf32_to_utf8( std::u32string( first_name, text.end() ) );
}

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
