#include "catch/catch.hpp"
#include "clzones.h"
#include "json.h"
#include "zone_ui.h"

#include <sstream>
#include <string>

TEST_CASE("zone_suggested_name_without_private_use_icon", "[zone][zone_ui]") {
    CHECK(zone_name_without_icon("\ue200\ue201 Ingredients") == "Ingredients");
    CHECK(zone_name_without_icon("\ue300 Одежда") == "Одежда");
    CHECK(zone_name_without_icon("\U000f0001 Books") == "Books");
    CHECK(zone_name_without_icon("Tools") == "Tools");
    CHECK(zone_name_without_icon("  My pantry") == "  My pantry");
    CHECK(zone_name_without_icon("Pantry \ue200\ue201") == "Pantry \ue200\ue201");
    CHECK(zone_name_without_icon("").empty());
    CHECK(zone_name_without_icon("\ue200\ue201 ") == "\ue200\ue201 ");
}

TEST_CASE("zone_type_separate_default_name", "[zone][zone_ui][json]") {
    auto stream = std::istringstream{
        R"({"id":"zone_ui_test","name":"\ue200\ue201 Type","default_name":"My pantry"})"};
    auto input = JsonIn{stream};
    auto zone = zone_type{};
    zone.load(input.get_object(), "zone_ui_test");
    CHECK(zone.name() == "\ue200\ue201 Type");
    CHECK(zone.default_name() == "My pantry");
}

TEST_CASE("zone_type_default_name_fallback", "[zone][zone_ui]") {
    const auto plain = zone_type{"Unsorted", ""};
    CHECK(plain.default_name() == plain.name());
    const auto decorated = zone_type{"\ue200\ue201 Ингредиенты", ""};
    CHECK(decorated.default_name() == "Ингредиенты");
    CHECK(decorated.name() == "\ue200\ue201 Ингредиенты");
}

TEST_CASE("zone_row_keeps_room_for_long_name_and_location", "[zone][zone_ui]") {
    const auto nearby = make_zone_row_layout(43, "0 Ц");
    CHECK(nearby.name_width == 35);
    CHECK(nearby.location_x == 40);
    CHECK(nearby.vehicle_x == 39);
    const auto distant = make_zone_row_layout(43, "12345 СЗ");
    CHECK(distant.name_width == 30);
    CHECK(distant.location_x == 35);
    CHECK(distant.location_x + utf8_width("12345 СЗ") == 43);
    const auto wide = make_zone_row_layout(63, "0 Ц");
    CHECK(wide.name_width > nearby.name_width);
}
