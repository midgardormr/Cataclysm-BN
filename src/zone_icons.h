#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

/// Selected Nerd Fonts code points, in zone category display order.
inline constexpr auto zone_icon_codepoints = std::array<uint32_t, 18> {
    0xf04e1, 0xf025b, 0xf0787, 0xf1064, 0xf0a7b, 0xf11ec,
    0xf14f7, 0xf0ae2, 0xf0493, 0xf032a, 0xf0f58, 0xf068c,
    0xf0710, 0xf08ea, 0xf010b, 0xf006a, 0xf073a, 0xf0004
};

/// Unrecognized icons follow the built-in categories.
inline auto zone_icon_order( const uint32_t codepoint ) -> int
{
    const auto found = std::ranges::find( zone_icon_codepoints, codepoint );
    return static_cast<int>( found - zone_icon_codepoints.begin() );
}
