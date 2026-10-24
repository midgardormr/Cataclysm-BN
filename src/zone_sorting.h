#pragma once

#include <functional>
#include <unordered_set>
#include "coordinates.h"

class item;
class player;

struct zone_destination_query {
    tripoint_abs_ms where;
    int range;
    std::function < auto( const tripoint_abs_ms & ) -> bool > accepts;
};

/// Available destinations of the preferred zone type, reachable by the sorting actor.
auto zone_sorting_destinations( const item &candidate, const player &actor,
                                int range ) -> std::unordered_set<tripoint_abs_ms>;
