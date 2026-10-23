#pragma once

#include <optional>
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

/// A storage selection is one category zone; each rule remains a native functional zone.
struct zone_creation_selection {
    std::vector<zone_type_id> categories;
    std::vector<zone_type_id> rules;
    std::optional<zone_type_id> single_type;
};

auto zone_storage_categories() -> std::vector<zone_type_id>;
auto zone_creation_rules() -> std::vector<zone_type_id>;
auto toggle_zone_categories( std::vector<zone_type_id> &selected,
                             const std::vector<zone_type_id> &members ) -> void;
auto query_zone_creation( const zone_creation_selection &initial = {},
bool categories_only = false ) -> std::optional<zone_creation_selection>;
