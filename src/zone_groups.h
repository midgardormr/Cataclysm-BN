#pragma once

#include <optional>
#include <string>
#include <vector>
#include <variant>
#include "type_id.h"
#include "translations.h"

class JsonObject;

struct loot_zone_group {
    std::string id;
    translation title;
    std::vector<zone_type_id> categories;
    std::string kind = "storage";
    int sort_order = 0;
};

/// Ordered presentation groups for the zone creation checklist.
auto loot_zone_groups() -> const std::vector<loot_zone_group> &; // *NOPAD*

/// A storage selection is one category zone; each rule remains a native functional zone.
struct zone_category_selection {
    std::vector<zone_type_id> categories;
    std::vector<zone_type_id> rules;
};

auto zone_storage_categories() -> std::vector<zone_type_id>;
auto zone_creation_rules() -> std::vector<zone_type_id>;
auto toggle_zone_categories( std::vector<zone_type_id> &selected,
                             const std::vector<zone_type_id> &members ) -> void;
using zone_creation_choice = std::variant<zone_category_selection, zone_type_id>;

/// Keep checkbox state when a separate setup dialog is canceled.
auto query_zone_creation( zone_category_selection &selection,
                          bool categories_only = false ) -> std::optional<zone_creation_choice>;

auto load_loot_zone_group( const JsonObject &jo ) -> void;
auto reset_loot_zone_groups() -> void;
auto item_category_zone_types() -> std::vector<zone_type_id>;
auto zone_selection_name( const std::vector<zone_type_id> &categories ) -> std::string;

/// Whether a native functional zone can be added without a separate options dialog.
auto zone_can_share_area( const zone_type_id &type ) -> bool;
