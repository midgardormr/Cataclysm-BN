#include "catch/catch.hpp"
#include "clzones.h"
#include "zone_sorting.h"
#include "activity_type.h"
#include "player_activity.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"
#include "json.h"
#include "output.h"
#include "language.h"
#include "options_helpers.h"
#include "zone_ui.h"
#include "zone_presets.h"
#include "item.h"
#include "flag.h"
#include "itype.h"
#include "relic.h"
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
    options.set_preset( { .id = "categories",
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
    for( const auto &id : groups.back().categories ) {
        CHECK( std::ranges::contains( zone_creation_rules(), id ) );
    }
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

TEST_CASE( "zone_group_names_and_suggestions_follow_language", "[zone_ui][i18n]" )
{
    auto storage = loot_options();
    auto english_order = std::vector<zone_type_id>();
    auto english_groups = std::vector<std::string>();
    storage.set_preset( { .id = "categories", .categories = { zone_type_id( "LOOT_WORKSHOP_TOOLS" ) } } );
    {
        const auto language = override_option( "USE_LANG", "en_US" );
        set_language();
        english_order = zone_storage_categories();
        for( const auto &group : loot_zone_presets() ) {
            english_groups.push_back( group.id );
        }
        CHECK( storage.get_zone_name_suggestion() == "Tools" );
        CHECK( zone_selection_name( { zone_type_id( "LOOT_FOOD" ) } ) == "Food and drinks" );
        CHECK( zone_selection_name( { zone_type_id( "LOOT_BOOKS" ), zone_type_id( "LOOT_TOOLS" ) } ) ==
               "Tools, Books and maps" );
    }
    {
        const auto language = override_option( "USE_LANG", "ru_RU" );
        set_language();
        CHECK( zone_storage_categories() == english_order );
        auto russian_groups = std::vector<std::string>();
        for( const auto &group : loot_zone_presets() ) {
            russian_groups.push_back( group.id );
        }
        CHECK( russian_groups == english_groups );
        CHECK( storage.get_zone_name_suggestion() == "Инструменты" );
        CHECK( zone_selection_name( { zone_type_id( "LOOT_FOOD" ) } ) == "Еда и напитки" );
        CHECK( zone_selection_name( { zone_type_id( "LOOT_BOOKS" ), zone_type_id( "LOOT_TOOLS" ) } ) ==
               "Инструменты, Книги и карты" );
    }
    set_language();
}

TEST_CASE( "zone_group_json_extensions_keep_unassigned_categories_available", "[zone_ui][json]" )
{
    auto saved = std::ostringstream();
    auto out = JsonOut( saved );
    out.start_array();
    for( const auto &group : loot_zone_presets() ) {
        out.start_object();
        out.member( "id", group.id );
        out.member( "name", std::string( group.title.debug_get_raw() ) );
        out.member( "kind", group.kind );
        out.member( "sort_order", group.sort_order );
        out.member( "categories", group.categories );
        out.end_object();
    }
    out.end_array();
    const auto restore = on_out_of_scope( [&]() {
        reset_loot_zone_groups();
        auto stream = std::istringstream( saved.str() );
        auto input = JsonIn( stream );
        for( const auto entry : input.get_array() ) {
            load_loot_zone_group( entry );
        }
    } );
    const auto load = []( const std::string & text ) {
        auto stream = std::istringstream( text );
        auto input = JsonIn( stream );
        load_loot_zone_group( input.get_object() );
    };
    load( R"({"id":"tools","delete":{"categories":["LOOT_TOOLS"]}})" );
    const auto available = zone_storage_categories();
    CHECK( std::ranges::contains( available, zone_type_id( "LOOT_TOOLS" ) ) );
    CHECK( std::ranges::count( available, zone_type_id( "LOOT_TOOLS" ) ) == 1 );
    load( R"({"id":"tools","extend":{"categories":["LOOT_TOOLS","LOOT_TOOLS"]}})" );
    const auto tools = std::ranges::find( loot_zone_presets(), "tools", &loot_zone_preset::id );
    REQUIRE( tools != loot_zone_presets().end() );
    CHECK( std::ranges::count( tools->categories, zone_type_id( "LOOT_TOOLS" ) ) == 1 );
    load( R"({"id":"test_mod_group","name":"Mod tools","sort_order":-10,"categories":["LOOT_TOOLS","not_installed"]})" );
    CHECK( loot_zone_presets().front().id == "test_mod_group" );
    const auto reordered = zone_storage_categories();
    CHECK( std::ranges::count( reordered, zone_type_id( "LOOT_TOOLS" ) ) == 1 );
    CHECK_FALSE( std::ranges::contains( reordered, zone_type_id( "not_installed" ) ) );
    load( R"({"id":"rules","sort_order":-1000})" );
    CHECK( loot_zone_presets().back().kind == "rules" );
    reset_loot_zone_groups();
    CHECK( zone_creation_rules().empty() );
    auto english_order = std::vector<zone_type_id>();
    {
        const auto language = override_option( "USE_LANG", "en_US" );
        set_language();
        english_order = zone_storage_categories();
    }
    {
        const auto language = override_option( "USE_LANG", "ru_RU" );
        set_language();
        CHECK( zone_storage_categories() == english_order );
    }
    set_language();
}

TEST_CASE( "zone_filter_prefix_highlights_follow_filter_syntax", "[zone_ui]" )
{
    const auto text = std::string( "c:еда,-m:iron,b:q:cut;k:survival,http:c:literal" );
    auto marked = std::vector<std::string>();
    for( const auto byte : zone_filter_prefixes( text ) ) {
        marked.push_back( text.substr( byte, 2 ) );
    }
    CHECK( marked == std::vector<std::string> { "c:", "m:", "b:", "q:", "k:" } );
    CHECK( zone_filter_prefixes( "ordinary text without a prefix" ).empty() );
    CHECK( zone_filter_prefixes( "<color_red>text</color>,m:wood" ) ==
           std::vector<size_t> { 24 } );
}

TEST_CASE( "simple_zone_functions_share_storage_area", "[zone_ui]" )
{
    CHECK( zone_can_share_area( zone_type_id( "AUTO_EAT" ) ) );
    CHECK( zone_can_share_area( zone_type_id( "AUTO_DRINK" ) ) );
    CHECK( zone_can_share_area( zone_type_id( "SOURCE_FIREWOOD" ) ) );
    CHECK( zone_can_share_area( zone_type_id( "NPC_RETREAT" ) ) );
    CHECK_FALSE( zone_can_share_area( zone_type_id( "FARM_PLOT" ) ) );
    CHECK_FALSE( zone_can_share_area( zone_type_id( "CONSTRUCTION_BLUEPRINT" ) ) );
    CHECK_FALSE( zone_can_share_area( zone_type_id( "LOOT_CUSTOM" ) ) );
    CHECK( std::ranges::contains( zone_creation_rules(), zone_type_id( "AUTO_EAT" ) ) );
    clear_avatar();
    clear_map();
    auto &mgr = zone_manager::get_manager();
    auto saved = std::ostringstream();
    auto saved_out = JsonOut( saved );
    mgr.serialize( saved_out );
    const auto restore = on_out_of_scope( [&]() {
        auto stream = std::istringstream( saved.str() );
        auto input = JsonIn( stream );
        mgr.deserialize( input );
        mgr.cache_data();
    } );
    auto empty = std::istringstream( "[]" );
    auto input = JsonIn( empty );
    mgr.deserialize( input );
    mgr.cache_data();
    const auto origin = get_avatar().abs_pos();
    const auto spot = origin + tripoint_rel_ms( 1, 0, 0 );
    auto storage = make_shared_fast<loot_options>();
    storage->set_preset( { .id = "categories", .categories = { zone_type_id( "LOOT_PFOOD" ) } } );
    mgr.add( "Perishable food", zone_type_id( "LOOT_CUSTOM" ), your_fac, false, true, spot, spot,
             storage );
    mgr.add( "Auto eat", zone_type_id( "AUTO_EAT" ), your_fac, false, true, spot, spot );
    const auto apple = item::spawn_temporary( "apple" );
    REQUIRE( storage->matches( *apple ) );
    CHECK( mgr.get_near_zone_type_for_item( *apple, origin, 10 ) == zone_type_id( "LOOT_CUSTOM" ) );
    CHECK( mgr.get_near( zone_type_id( "AUTO_EAT" ), origin, 10 ).contains( spot ) );
    auto out = std::ostringstream();
    auto json = JsonOut( out );
    mgr.serialize( json );
    auto round_trip = std::istringstream( out.str() );
    auto restored = JsonIn( round_trip );
    mgr.deserialize( restored );
    mgr.cache_data();
    CHECK( mgr.get_near_zone_type_for_item( *apple, origin, 10 ) == zone_type_id( "LOOT_CUSTOM" ) );
    CHECK( mgr.get_near( zone_type_id( "AUTO_EAT" ), origin, 10 ).contains( spot ) );
}

namespace {
struct sorting_fixture {
    std::string saved;
    std::optional<override_option> language;

    sorting_fixture() {
        language.emplace("USE_LANG", "en_US");
        set_language();
        auto stream = std::ostringstream();
        auto output = JsonOut(stream);
        zone_manager::get_manager().serialize(output);
        saved = stream.str();
        clear_avatar();
        clear_map();
        zone_manager::reset_manager();
    }
    ~sorting_fixture() {
        clear_map();
        zone_manager::reset_manager();
        auto stream = std::istringstream(saved);
        auto input = JsonIn(stream);
        zone_manager::get_manager().deserialize(input);
        zone_manager::get_manager().cache_data();
        language.reset();
        set_language();
    }

    auto add(const tripoint_abs_ms& point, const zone_type_id& type) -> void {
        zone_manager::get_manager().add(type.str(), type, your_fac, false, true, point, point);
    }
    auto custom(const tripoint_abs_ms& point, const std::string& json) -> void {
        auto options = make_shared_fast<loot_options>();
        auto stream = std::istringstream(json);
        auto input = JsonIn(stream);
        options->deserialize(input.get_object());
        zone_manager::get_manager().add(
            "Custom", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, point, point, options);
    }
    auto sort() -> void {
        auto& actor = get_avatar();
        actor.assign_activity(activity_id("ACT_MOVE_LOOT"));
        process_activity(actor);
    }
};
} // namespace

TEST_CASE(
    "zone_sorting_moves_items_with_capacity_overlap_and_path_constraints",
    "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& actor = get_avatar();
    const auto source = actor.abs_pos();
    const auto fallback = source + tripoint_rel_ms(0, 2, 0);
    const auto specialized = source + tripoint_rel_ms(4, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(
        fallback, R"({"superzone":"categories","categories":["LOOT_WORKSHOP_TOOLS"],"mark":""})");
    fixture.add(specialized, zone_type_id("LOOT_WORKSHOP_TOOLS"));
    auto expected = specialized;
    auto scenario = std::string();
    SECTION("specialized_storage_wins") { scenario = "specialized_storage_wins"; }
    SECTION("full_specialized_storage_falls_back") {
        scenario = "full_specialized_storage_falls_back";
        auto rocks = item::spawn("rock");
        rocks->charges = 1000000;
        here.add_item(abs_to_bub(specialized), std::move(rocks));
        expected = fallback;
    }
    SECTION("sealed_specialized_storage_falls_back") {
        scenario = "sealed_specialized_storage_falls_back";
        for (const auto point : here.points_in_radius(abs_to_bub(specialized), 1)) {
            if (point != abs_to_bub(specialized)) { here.ter_set(point, ter_id("t_wall")); }
        }
        expected = fallback;
    }
    SECTION("overlapping_nonmatching_custom_zone_at_source_does_not_trap_items") {
        scenario = "overlapping_nonmatching_custom_zone_at_source_does_not_trap_items";
        fixture.custom(source, R"({"mark":"rock"})");
        here.ter_set(abs_to_bub(specialized), ter_id("t_wall"));
        expected = fallback;
    }
    SECTION("unreachable_manual_filter_does_not_suppress_category_storage") {
        scenario = "unreachable_manual_filter_does_not_suppress_category_storage";
        fixture.custom(specialized, R"({"mark":"hammer"})");
        for (const auto point : here.points_in_radius(abs_to_bub(specialized), 1)) {
            if (point != abs_to_bub(specialized)) { here.ter_set(point, ter_id("t_wall")); }
        }
        expected = fallback;
    }
    SECTION("overlapping_filters_accept_union_of_matches") {
        scenario = "overlapping_filters_accept_union_of_matches";
        fixture.custom(specialized, R"({"mark":"rock"})");
        fixture.custom(specialized, R"({"mark":"hammer"})");
    }
    SECTION("accessible_manual_filter_takes_precedence") {
        scenario = "accessible_manual_filter_takes_precedence";
        fixture.custom(fallback, R"({"mark":"hammer"})");
        expected = fallback;
    }
    SECTION("closed_unlocked_door_is_reachable") {
        scenario = "closed_unlocked_door_is_reachable";
        for (const auto point : here.points_in_radius(abs_to_bub(specialized), 2)) {
            if (square_dist(point, abs_to_bub(specialized)) == 2) {
                here.ter_set(point, ter_id("t_wall"));
            }
        }
        here.ter_set(abs_to_bub(specialized) + tripoint_rel_ms(-2, 0, 0), ter_id("t_door_c"));
    }
    SECTION("locked_door_does_not_make_storage_reachable") {
        scenario = "locked_door_does_not_make_storage_reachable";
        for (const auto point : here.points_in_radius(abs_to_bub(specialized), 2)) {
            if (square_dist(point, abs_to_bub(specialized)) == 2) {
                here.ter_set(point, ter_id("t_wall"));
            }
        }
        here.ter_set(abs_to_bub(specialized) + tripoint_rel_ms(-2, 0, 0), ter_id("t_door_locked"));
        expected = fallback;
    }
    SECTION("already_sorted_item_stays_in_full_specialized_source") {
        scenario = "already_sorted_item_stays_in_full_specialized_source";
        fixture.add(source, zone_type_id("LOOT_WORKSHOP_TOOLS"));
        auto rocks = item::spawn("rock");
        rocks->charges = 1000000;
        here.add_item(abs_to_bub(source), std::move(rocks));
        expected = source;
    }
    SECTION("no_reachable_storage_leaves_item_at_source") {
        scenario = "no_reachable_storage_leaves_item_at_source";
        here.ter_set(abs_to_bub(specialized), ter_id("t_wall"));
        here.ter_set(abs_to_bub(fallback), ter_id("t_wall"));
        expected = source;
    }
    here.add_item(abs_to_bub(source), item::spawn("hammer"));
    CAPTURE(scenario);
    fixture.sort();
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(expected)), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    }));
    if (expected != source) { CHECK(here.i_at(abs_to_bub(source)).empty()); }
}

TEST_CASE(
    "legacy_zones_keep_names_filters_coordinates_and_behavior", "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& manager = zone_manager::get_manager();
    auto stream = std::istringstream(R"([
        {"name":"Old tools","type":"LOOT_WORKSHOP_TOOLS","invert":false,"enabled":true,
         "start_x":61,"start_y":60,"start_z":0,"end_x":61,"end_y":60,"end_z":0},
        {"name":"My old filter","type":"LOOT_CUSTOM","invert":false,"enabled":true,
         "start":[62,60,0],"end":[62,60,0],"mark":"hammer"}
    ])");
    auto input = JsonIn(stream);
    manager.deserialize(input);
    manager.cache_data();
    auto output = std::ostringstream();
    auto json = JsonOut(output);
    manager.serialize(json);
    auto saved = std::istringstream(output.str());
    auto parsed = JsonIn(saved);
    const auto zones = parsed.get_array();
    REQUIRE(zones.size() == 2);
    const auto old_tools = zones.get_object(0);
    const auto old_filter = zones.get_object(1);
    old_tools.allow_omitted_members();
    old_filter.allow_omitted_members();
    CHECK(old_tools.get_string("name") == "Old tools");
    CHECK(old_filter.get_string("mark") == "hammer");
    CHECK_FALSE(old_filter.has_member("superzone"));
    auto reloaded = std::istringstream(output.str());
    auto reload_input = JsonIn(reloaded);
    manager.deserialize(reload_input);
    const auto custom = manager.get_zones(zone_type_id("LOOT_CUSTOM"), tripoint_abs_ms(62, 60, 0));
    REQUIRE(custom.size() == 1);
    const auto& options = dynamic_cast<const loot_options&>(custom.front().get_options());
    CHECK(options.matches(*item::spawn_temporary("hammer")));
    CHECK_FALSE(options.matches(*item::spawn_temporary("rock")));
}

TEST_CASE(
    "vehicle_category_storage_moves_items_and_falls_back_when_full", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos();
    const auto cargo_point = source + tripoint_rel_ms(3, 0, 0);
    const auto fallback = source + tripoint_rel_ms(0, 2, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(
        fallback, R"({"superzone":"categories","categories":["LOOT_WORKSHOP_TOOLS"],"mark":""})");
    auto* cart =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(cargo_point), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(cargo_point)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    const auto part = cargo->part_index();
    cart->get_items(part).clear();
    auto zone = zone_data(
        "Workshop cart", zone_type_id("LOOT_WORKSHOP_TOOLS"), your_fac, false, true, cargo_point,
        cargo_point);
    zone.set_dimension(here.get_bound_dimension());
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    auto full = false;
    SECTION("available_vehicle_storage") {}
    SECTION("full_vehicle_storage_uses_ground_fallback") {
        const auto rock_volume = item::spawn_temporary("rock")->volume();
        while (cart->free_volume(part) >= rock_volume) {
            REQUIRE_FALSE(cart->add_item(part, item::spawn("rock")));
        }
        REQUIRE(cart->free_volume(part) < item::spawn_temporary("hammer")->volume());
        full = true;
    }
    SECTION("full_ground_beneath_vehicle_does_not_block_cargo") {
        for (auto i = 0; i < MAX_ITEM_IN_SQUARE; ++i) {
            here.add_item(abs_to_bub(cargo_point), item::spawn("rock"));
        }
        REQUIRE(here.i_at(abs_to_bub(cargo_point)).size() == MAX_ITEM_IN_SQUARE);
    }
    SECTION("moving_vehicle_updates_destination") {
        here.displace_vehicle(*cart, tripoint_rel_ms(0, 1, 0));
        manager.cache_vzones();
        CHECK_FALSE(manager.has(zone_type_id("LOOT_WORKSHOP_TOOLS"), cargo_point));
    }
    here.add_item(abs_to_bub(source), item::spawn("hammer"));
    fixture.sort();
    CHECK(here.i_at(abs_to_bub(source)).empty());
    const auto cargo_has_hammer = std::ranges::any_of(cart->get_items(part), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    });
    CHECK(cargo_has_hammer == !full);
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(fallback)),
                              [](const auto& it) { return it->typeId() == itype_id("hammer"); })
          == full);
}

TEST_CASE(
    "mod_storage_without_group_or_loot_prefix_remains_selectable",
    "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto zone_stream = std::istringstream(
        R"({"id":"test_mod_parts_storage","name":"Mod parts","color":"white"})");
    auto zone_input = JsonIn(zone_stream);
    zone_type::load_zones(zone_input.get_object(), "zone_test_mod");
    auto category_stream = std::istringstream(
        R"({"id":"test_mod_parts","name":"MOD PARTS","sort_rank":0,"zone":"test_mod_parts_storage"})");
    auto category_input = JsonIn(category_stream);
    item_category::load_item_cat(category_input.get_object(), "zone_test_mod");
    zone_manager::reset_manager();
    const auto id = zone_type_id("test_mod_parts_storage");
    CHECK(std::ranges::contains(zone_storage_categories(), id));
    auto type = itype(*item::spawn_temporary("hammer")->type);
    type.category_force = item_category_id("test_mod_parts");
    auto candidate = item::spawn(&type);
    auto options = loot_options();
    options.set_preset({.id = "categories", .categories = {id}});
    CHECK(options.matches(*candidate));
    const auto destination = get_avatar().abs_pos() + tripoint_rel_ms(2, 0, 0);
    fixture.add(destination, id);
    CHECK(zone_sorting_destinations(*candidate, get_avatar(), 10).contains(destination));
}

TEST_CASE(
    "sorting_takes_items_from_vehicle_source_into_category_storage", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos() + tripoint_rel_ms(1, 0, 0);
    const auto destination = get_avatar().abs_pos() + tripoint_rel_ms(0, 2, 0);
    fixture.custom(destination,
                   R"({"superzone":"categories","categories":["LOOT_WORKSHOP_TOOLS"],"mark":""})");
    auto* cart = here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(source), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(source)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    cart->get_items(cargo->part_index()).clear();
    REQUIRE_FALSE(cart->add_item(cargo->part_index(), item::spawn("hammer")));
    auto zone = zone_data(
        "Unsorted cart", zone_type_id("LOOT_UNSORTED"), your_fac, false, true, source, source);
    zone.set_dimension(here.get_bound_dimension());
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    fixture.sort();
    CHECK(cart->get_items(cargo->part_index()).empty());
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(destination)), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    }));
}

TEST_CASE(
    "vehicle_custom_category_filter_survives_save_and_accepts_matching_items",
    "[zone_ui][zone_sorting][json]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    auto& manager = zone_manager::get_manager();
    const auto source = get_avatar().abs_pos();
    const auto destination = source + tripoint_rel_ms(3, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    auto* cart =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(destination), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = here.veh_at(abs_to_bub(destination)).part_with_feature("CARGO", false);
    REQUIRE(cargo);
    cart->get_items(cargo->part_index()).clear();
    auto options = make_shared_fast<loot_options>();
    options->set_preset({.id = "categories", .categories = {zone_type_id("LOOT_WORKSHOP_TOOLS")}});
    auto zone = zone_data(
        "Category cart", zone_type_id("LOOT_CUSTOM"), your_fac, false, true, destination,
        destination, options);
    zone.set_dimension(here.get_bound_dimension());
    auto saved = std::ostringstream();
    auto output = JsonOut(saved);
    zone.serialize(output);
    auto stream = std::istringstream(saved.str());
    auto input = JsonIn(stream);
    zone.deserialize(input);
    manager.create_vehicle_loot_zone(*cart, cargo->mount(), zone);
    CHECK(manager.custom_loot_has(destination, item::spawn_temporary("hammer")));
    CHECK_FALSE(manager.custom_loot_has(destination, item::spawn_temporary("rock")));
    here.add_item(abs_to_bub(source), item::spawn("hammer"));
    here.add_item(abs_to_bub(source), item::spawn("rock"));
    fixture.sort();
    CHECK(std::ranges::any_of(cart->get_items(cargo->part_index()), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    }));
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(source)), [](const auto& it) {
        return it->typeId() == itype_id("rock");
    }));
}

TEST_CASE(
    "missing_mod_category_is_preserved_without_becoming_catch_all",
    "[zone_ui][zone_sorting][json]") {
    auto stream = std::istringstream(
        R"({"superzone":"categories","categories":["removed_mod_storage"],"mark":""})");
    auto input = JsonIn(stream);
    auto options = loot_options();
    options.deserialize(input.get_object());
    CHECK_FALSE(options.matches(*item::spawn_temporary("hammer")));
    CHECK_FALSE(options.matches(*item::spawn_temporary("rock")));
    REQUIRE(options.get_descriptions().size() == 1);
    CHECK(options.get_descriptions().front().second == "removed_mod_storage");
    auto saved = std::ostringstream();
    auto output = JsonOut(saved);
    output.start_object();
    options.serialize(output);
    output.end_object();
    CHECK(saved.str().find("removed_mod_storage") != std::string::npos);
}

TEST_CASE("narrow_category_storage_precedes_general_storage", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& actor = get_avatar();
    const auto source = actor.abs_pos();
    auto broad = source + tripoint_rel_ms(2, 0, 0);
    auto narrow = source + tripoint_rel_ms(0, 2, 0);
    auto expected = narrow;
    SECTION("first_placement") {}
    SECTION("reverse_placement") {
        std::swap(broad, narrow);
        expected = narrow;
    }
    SECTION("full_narrow_storage_uses_general_storage") {
        auto rocks = item::spawn("rock");
        rocks->charges = 1000000;
        get_map().add_item(abs_to_bub(narrow), std::move(rocks));
        expected = broad;
    }
    SECTION("blocked_narrow_storage_uses_general_storage") {
        get_map().ter_set(abs_to_bub(narrow), ter_id("t_wall"));
        expected = broad;
    }
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.custom(
        narrow, R"({"superzone":"categories","categories":["LOOT_WORKSHOP_TOOLS"],"mark":""})");
    fixture.custom(
        broad,
        R"({"superzone":"categories","categories":["LOOT_WORKSHOP_TOOLS","LOOT_ENTRY_TOOLS","LOOT_COOKING_TOOLS","LOOT_CHEMISTRY_TOOLS","LOOT_FARM_TOOLS","LOOT_TOOLS"],"mark":""})");
    const auto hammer = item::spawn_temporary("hammer");
    const auto destinations = zone_sorting_destinations(*hammer, actor, 10);
    CHECK(destinations.size() == 1);
    CHECK(destinations.contains(expected));
    get_map().add_item(abs_to_bub(source), item::spawn("hammer"));
    fixture.sort();
    CHECK(std::ranges::any_of(get_map().i_at(abs_to_bub(expected)), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    }));
}

TEST_CASE("mod_priority_storage_is_discovered_and_matches", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto zone_stream = std::istringstream(
        R"({"id":"REVIEW_PRIORITY_STORAGE","name":"Priority storage"})");
    auto zone_input = JsonIn(zone_stream);
    zone_type::load_zones(zone_input.get_object(), "review_mod");
    auto category_stream = std::istringstream(
        R"({"id":"review_priority_category","name":"Review category","sort_rank":0,"zone":"LOOT_OTHER","priority_zones":[{"id":"REVIEW_PRIORITY_STORAGE","flags":["FIREWOOD"]}]})");
    auto category_input = JsonIn(category_stream);
    item_category::load_item_cat(category_input.get_object(), "review_mod");
    zone_manager::reset_manager();
    auto type = itype(*item::spawn_temporary("hammer")->type);
    type.category_force = item_category_id("review_priority_category");
    auto candidate = item::spawn(&type);
    candidate->set_flag(flag_id("FIREWOOD"));
    REQUIRE(candidate->get_category().priority_zone(*candidate)
            == zone_type_id("REVIEW_PRIORITY_STORAGE"));
    CHECK(
        std::ranges::contains(zone_storage_categories(), zone_type_id("REVIEW_PRIORITY_STORAGE")));
    auto options = loot_options();
    options.set_preset(
        {.id = "categories", .categories = {zone_type_id("REVIEW_PRIORITY_STORAGE")}});
    CHECK(options.matches(*candidate));
}

TEST_CASE("sorting_cannot_cross_door_blocked_by_furniture", "[zone_ui][zone_sorting]") {
    auto fixture = sorting_fixture();
    auto& here = get_map();
    const auto source = get_avatar().abs_pos();
    const auto destination = source + tripoint_rel_ms(4, 0, 0);
    fixture.add(source, zone_type_id("LOOT_UNSORTED"));
    fixture.add(destination, zone_type_id("LOOT_WORKSHOP_TOOLS"));
    for (const auto point : here.points_in_radius(abs_to_bub(destination), 2)) {
        if (square_dist(point, abs_to_bub(destination)) == 2) {
            here.ter_set(point, ter_id("t_wall"));
        }
    }
    const auto gate = abs_to_bub(destination) + tripoint_rel_ms(-2, 0, 0);
    here.ter_set(gate, ter_id("t_door_c"));
    here.furn_set(gate, furn_id("f_bookcase"));
    const auto hammer = item::spawn_temporary("hammer");
    CHECK(zone_sorting_destinations(*hammer, get_avatar(), 10).empty());
    here.add_item(abs_to_bub(source), item::spawn("hammer"));
    fixture.sort();
    CHECK(std::ranges::any_of(here.i_at(abs_to_bub(source)), [](const auto& it) {
        return it->typeId() == itype_id("hammer");
    }));
    here.ter_set(gate, ter_id("t_door_o"));
    REQUIRE(here.impassable(gate));
}
