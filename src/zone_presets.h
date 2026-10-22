#pragma once

#include <string>
#include <vector>
#include "type_id.h"

struct loot_zone_preset {
    std::string id;
    std::string title;
    std::string name;
    int group;
    std::vector<zone_type_id> categories;
};

/// Menu templates create ordinary LOOT_CUSTOM zones, not additional zone types.
auto loot_zone_presets() -> const std::vector<loot_zone_preset> &; // *NOPAD*
