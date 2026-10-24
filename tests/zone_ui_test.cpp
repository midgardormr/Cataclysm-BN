#include "catch/catch.hpp"
#include "clzones.h"
#include "json.h"
#include "output.h"
#include "language.h"
#include "options_helpers.h"
#include "zone_ui.h"
#include "zone_presets.h"
#include "item.h"
#include "item_category.h"
#include "avatar.h"
#include "map/map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "cata_utility.h"

#include <sstream>
#include <string>

TEST_CASE( "zone_row_keeps_room_for_long_name_and_location", "[zone][zone_ui]" )
{
    const auto nearby = make_zone_row_layout( 43, "0 Ц" );
    CHECK( nearby.name_width == 35 );
    CHECK( nearby.location_x == 40 );
    CHECK( nearby.vehicle_x == 39 );
    const auto distant = make_zone_row_layout( 43, "12345 СЗ" );
    CHECK( distant.name_width == 30 );
    CHECK( distant.location_x == 35 );
    CHECK( distant.location_x + utf8_width( "12345 СЗ" ) == 43 );
    const auto wide = make_zone_row_layout( 63, "0 Ц" );
    CHECK( wide.name_width > nearby.name_width );
}

TEST_CASE( "custom_zone_details_show_only_filter_text", "[zone][zone_ui][json]" )
{
    auto stream = std::istringstream{R"({"mark":"c:еда,m:железо,q:забивания"})"};
    auto input = JsonIn{stream};
    auto options = loot_options{};
    options.deserialize( input.get_object() );
    CHECK( options.get_zone_name_suggestion().empty() );
    const auto descriptions = options.get_descriptions();
    REQUIRE( descriptions.size() == 1 );
    CHECK( descriptions.front().first.empty() );
    CHECK( descriptions.front().second == "c:еда,m:железо,q:забивания" );
}

TEST_CASE( "built_in_zone_names_follow_game_language", "[zone][zone_ui][i18n]" )
{
    const auto food = zone_type_id( "LOOT_FOOD" );
    {
        const auto language = override_option{"USE_LANG", "ru_RU"};
        set_language();
        CHECK( food->name() == "Еда" );
    }
    {
        const auto language = override_option{"USE_LANG", "en_US"};
        set_language();
        CHECK( food->name() == "Food" );
    }
    set_language();
}

TEST_CASE( "superzone_template_categories_and_round_trip", "[zone_ui][superzone]" )
{
    for( const auto &preset : loot_zone_presets() ) {
        REQUIRE( !preset.categories.empty() );
        for( const auto &category : preset.categories ) {
            CHECK( category.is_valid() );
        }
    }
    const auto tools = std::ranges::find( loot_zone_presets(), "tools", &loot_zone_preset::id );
    REQUIRE( tools != loot_zone_presets().end() );
    auto options = loot_options();
    options.set_preset( *tools );
    const auto hammer = item::spawn_temporary( "hammer" );
    const auto crowbar = item::spawn_temporary( "crowbar" );
    const auto rock = item::spawn_temporary( "rock" );
    REQUIRE( options.matches( *hammer ) );
    REQUIRE( options.matches( *crowbar ) );
    CHECK_FALSE( options.matches( *rock ) );
    auto stream = std::istringstream(
                      R"({"superzone":"tools","categories":["LOOT_WORKSHOP_TOOLS"],"mark":""})" );
    auto input = JsonIn( stream );
    options.deserialize( input.get_object() );
    CHECK( options.matches( *hammer ) );
    CHECK_FALSE( options.matches( *crowbar ) );
    auto out = std::ostringstream();
    auto json = JsonOut( out );
    json.start_object();
    options.serialize( json );
    json.end_object();
    auto restored = loot_options();
    auto saved = std::istringstream( out.str() );
    auto saved_input = JsonIn( saved );
    restored.deserialize( saved_input.get_object() );
    CHECK( restored.is_superzone() );
    CHECK( restored.matches( *hammer ) );
    CHECK_FALSE( restored.matches( *crowbar ) );
    {
        const auto language = override_option( "USE_LANG", "en_US" );
        set_language();
        CHECK( restored.matches( *hammer ) );
        CHECK_FALSE( restored.matches( *crowbar ) );
    }
    set_language();
}

TEST_CASE( "superzone_is_fallback_for_unavailable_specialized_zone", "[zone_ui][superzone]" )
{
    clear_avatar();
    clear_map();
    auto &mgr = zone_manager::get_manager();
    auto out = std::ostringstream();
    auto json = JsonOut( out );
    mgr.serialize( json );
    const auto restore = on_out_of_scope( [&]() {
        auto stream = std::istringstream( out.str() );
        auto input = JsonIn( stream );
        mgr.deserialize( input );
        mgr.cache_data();
    } );
    auto empty = std::istringstream( "[]" );
    auto input = JsonIn( empty );
    mgr.deserialize( input );
    mgr.cache_data();
    auto &here = get_map();
    const auto origin = get_avatar().abs_pos();
    const auto broad = origin + tripoint_rel_ms( 1, 0, 0 );
    const auto specific = origin + tripoint_rel_ms( 2, 0, 0 );
    auto options = make_shared_fast<loot_options>();
    const auto tools = std::ranges::find( loot_zone_presets(), "tools", &loot_zone_preset::id );
    REQUIRE( tools != loot_zone_presets().end() );
    options->set_preset( *tools );
    mgr.add( "Tools", zone_type_id( "LOOT_CUSTOM" ), your_fac, false, true, broad, broad, options );
    const auto hammer = item::spawn_temporary( "hammer" );
    REQUIRE( hammer->get_category().zone() == zone_type_id( "LOOT_WORKSHOP_TOOLS" ) );
    CHECK( mgr.get_near_zone_type_for_item( *hammer, origin, 10 ) == zone_type_id( "LOOT_CUSTOM" ) );
    mgr.add( "Workshop", zone_type_id( "LOOT_WORKSHOP_TOOLS" ), your_fac, false, true, specific,
             specific );
    CHECK( mgr.get_near_zone_type_for_item( *hammer, origin,
                                            10 ) == zone_type_id( "LOOT_WORKSHOP_TOOLS" ) );
    auto rocks = item::spawn( "rock" );
    rocks->charges = 1000000;
    here.add_item( abs_to_bub( specific ), std::move( rocks ) );
    REQUIRE( here.free_volume( abs_to_bub( specific ) ) < hammer->volume() );
    CHECK( mgr.get_near_zone_type_for_item( *hammer, origin, 10 ) == zone_type_id( "LOOT_CUSTOM" ) );
    here.i_clear( abs_to_bub( specific ) );
    here.ter_set( abs_to_bub( specific ), ter_id( "t_wall" ) );
    CHECK( mgr.get_near_zone_type_for_item( *hammer, origin, 10 ) == zone_type_id( "LOOT_CUSTOM" ) );
}

TEST_CASE( "superzone_food_choices_are_independent_and_filter_is_intersection",
           "[zone_ui][superzone]" )
{
    auto options = loot_options();
    auto stream = std::istringstream( R"({"superzone":"food","categories":["LOOT_DRINK"],"mark":""})" );
    auto input = JsonIn( stream );
    options.deserialize( input.get_object() );
    const auto water = item::spawn_temporary( "water_clean" );
    const auto apple = item::spawn_temporary( "apple" );
    CHECK( options.matches( *water ) );
    CHECK_FALSE( options.matches( *apple ) );
    auto filtered = std::istringstream(
                        R"({"superzone":"food","categories":["LOOT_DRINK"],"mark":"this_name_does_not_exist_87263"})" );
    auto filtered_input = JsonIn( filtered );
    options.deserialize( filtered_input.get_object() );
    CHECK_FALSE( options.matches( *water ) );
}

TEST_CASE( "zone_details_panel_grows_and_shrinks_with_selected_content", "[zone_ui]" )
{
    const auto short_details = make_zone_panel_layout( 40, 3 );
    const auto long_details = make_zone_panel_layout( 40, 14 );
    CHECK( long_details.details_height == 14 );
    CHECK( long_details.footer_height == long_details.details_height + 7 );
    CHECK( long_details.list_height < short_details.list_height );
    CHECK( long_details.footer_height + long_details.list_height == 40 );
    CHECK( make_zone_panel_layout( 40, 3 ).details_height == short_details.details_height );
    const auto overflow = make_zone_panel_layout( 24, 100 );
    CHECK( overflow.list_height >= 3 );
    CHECK( overflow.footer_height + overflow.list_height == 24 );
    CHECK( overflow.details_height < 100 );
}

TEST_CASE( "zone_creation_checklist_mixes_categories_without_rules_becoming_storage",
           "[zone_ui][superzone]" )
{
    const auto available = zone_storage_categories();
    const auto rules = zone_creation_rules();
    CHECK( std::ranges::contains( available, zone_type_id( "LOOT_BOOKS" ) ) );
    CHECK( std::ranges::contains( available, zone_type_id( "LOOT_WORKSHOP_TOOLS" ) ) );
    CHECK_FALSE( std::ranges::contains( available, zone_type_id( "LOOT_CUSTOM" ) ) );
    for( const auto &rule : rules ) {
        CHECK( rule.is_valid() );
        CHECK_FALSE( std::ranges::contains( available, rule ) );
    }
    auto selected = std::vector<zone_type_id> { zone_type_id( "LOOT_BOOKS" ) };
    const auto tools = std::ranges::find( loot_zone_presets(), "tools", &loot_zone_preset::id );
    REQUIRE( tools != loot_zone_presets().end() );
    toggle_zone_categories( selected, { tools->categories.front() } );
    toggle_zone_categories( selected, tools->categories );
    CHECK( selected.size() == tools->categories.size() + 1 );
    toggle_zone_categories( selected, tools->categories );
    CHECK( selected == std::vector<zone_type_id> { zone_type_id( "LOOT_BOOKS" ) } );
    toggle_zone_categories( selected, tools->categories );
    auto options = loot_options();
    options.set_preset( { .id = "categories", .title = "",
                          .categories = selected } );
    CHECK( options.matches( *item::spawn_temporary( "hammer" ) ) );
    CHECK( options.matches( *item::spawn_temporary( "novel_pulp" ) ) );
    CHECK_FALSE( options.matches( *item::spawn_temporary( "rock" ) ) );
    auto out = std::ostringstream();
    auto json = JsonOut( out );
    json.start_object();
    options.serialize( json );
    json.end_object();
    auto restored = loot_options();
    auto stream = std::istringstream( out.str() );
    auto input = JsonIn( stream );
    restored.deserialize( input.get_object() );
    CHECK( restored.matches( *item::spawn_temporary( "hammer" ) ) );
    CHECK( restored.matches( *item::spawn_temporary( "novel_pulp" ) ) );
    CHECK_FALSE( restored.matches( *item::spawn_temporary( "rock" ) ) );
}

TEST_CASE( "zone_filter_wrap_keeps_conditions_intact", "[zone_ui]" )
{
    const auto lines =
        wrap_zone_filter( "c:еда,c:кухонные инструменты,m:железо", 25 );
    REQUIRE( lines.size() == 3 );
    CHECK( lines[0] == "c:еда," );
    CHECK( lines[1] == "c:кухонные инструменты," );
    CHECK( lines[2] == "m:железо" );
    CHECK( wrap_zone_filter( "c:еда,m:железо", 40 ) ==
           std::vector<std::string> { "c:еда,m:железо" } );
    for( const auto &line :
         wrap_zone_filter( "c:сельскохозяйственные инструменты", 12 ) ) {
        CHECK( utf8_width( line ) <= 12 );
    }
}

TEST_CASE( "zone_groups_follow_document_order_without_duplicate_types", "[zone_ui]" )
{
    const auto &groups = loot_zone_presets();
    REQUIRE( groups.size() == 17 );
    CHECK( groups.front().id == "food" );
    CHECK( groups[4].id == "technology" );
    CHECK( groups.back().id == "rules" );
    CHECK( groups.back().categories == zone_creation_rules() );
    auto seen = std::vector<zone_type_id>();
    for( const auto &group : groups ) {
        for( const auto &id : group.categories ) {
            CHECK( id.is_valid() );
            CHECK_FALSE( std::ranges::contains( seen, id ) );
            seen.push_back( id );
        }
    }
    const auto categories = zone_storage_categories();
    CHECK( std::ranges::contains( categories, zone_type_id( "LOOT_SCRAP_ELECTRONICS" ) ) );
    CHECK_FALSE( std::ranges::contains( categories, zone_type_id( "FARM_PLOT" ) ) );
}

TEST_CASE( "zone_category_continuations_start_at_left_edge", "[zone_ui]" )
{
    const auto lines =
        wrap_zone_filter( "Инструменты мастерской, Инструменты взлома, Книги",
                          30 );
    REQUIRE( lines.size() == 2 );
    CHECK( lines[0] == "Инструменты мастерской," );
    CHECK( lines[1] == "Инструменты взлома, Книги" );
}
