#include "catch/catch.hpp"
#include "clzones.h"
#include "json.h"
#include "output.h"
#include "zone_ui.h"

#include <sstream>
#include <string>

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
    CHECK(decorated.default_name() == decorated.name());
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

TEST_CASE("whole_zone_icons_occupy_two_cells", "[zone][zone_ui]") {
    CHECK(utf8_width("\ue240") == 2);
    CHECK(utf8_width("\ue251") == 2);
    CHECK(utf8_width("\ue200\ue201") == 2);
    CHECK(utf8_width("\ue23f") == 1);
    CHECK(utf8_width("\ue252") == 1);
    CHECK(utf8_width("\ue240 Еда") == 6);
    CHECK(trim_by_length("\ue240 Еда", 6) == "\ue240 Еда");
}
