#include "catch/catch.hpp"
#include "clzones.h"
#include "json.h"
#include "output.h"
#include "language.h"
#include "options_helpers.h"
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

TEST_CASE("custom_zone_details_show_only_filter_text", "[zone][zone_ui][json]") {
    auto stream = std::istringstream{R"({"mark":"c:еда,m:железо,q:забивания"})"};
    auto input = JsonIn{stream};
    auto options = loot_options{};
    options.deserialize(input.get_object());
    CHECK(options.get_zone_name_suggestion().empty());
    const auto descriptions = options.get_descriptions();
    REQUIRE(descriptions.size() == 1);
    CHECK(descriptions.front().first.empty());
    CHECK(descriptions.front().second == "c:еда,m:железо,q:забивания");
}

TEST_CASE("zone_icon_is_separate_from_translated_name", "[zone][zone_ui][json]") {
    auto stream = std::istringstream{R"({"id":"zone_ui_icon","name":{"str":"Food","ctxt":"zone UI test"},"icon":"\ue241"})"};
    auto input = JsonIn{stream};
    auto type = zone_type{};
    type.load(input.get_object(), "zone_ui_test");
    CHECK(type.name() == "Food");
    CHECK(type.display_name() == "\ue241 Food");
    CHECK(type.default_name() == type.display_name());
    CHECK(utf8_width(type.display_name()) == 7);
}

TEST_CASE("built_in_zone_names_follow_game_language", "[zone][zone_ui][i18n]") {
    const auto food = zone_type_id("LOOT_FOOD");
    {
        const auto language = override_option{"USE_LANG", "ru_RU"};
        set_language();
        CHECK(food->name() == "Еда");
        CHECK(food->display_name() == "\ue241 Еда");
    }
    {
        const auto language = override_option{"USE_LANG", "en_US"};
        set_language();
        CHECK(food->name() == "Food");
        CHECK(food->display_name() == "\ue241 Food");
    }
    set_language();
}
