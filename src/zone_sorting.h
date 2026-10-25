#pragma once

#include <optional>
#include "coordinates.h"

class item;

/// Choose one storage tile, preferring narrower category selections and preserving sorted items.
auto zone_sorting_destination( const item &candidate, const tripoint_abs_ms &where,
                               int range ) -> std::optional<tripoint_abs_ms>;
