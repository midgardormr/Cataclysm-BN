#include "clzones.h"

#include "zone_icons.h"
#include "zone_presets.h"

#include "avatar.h"
#include "cata_utility.h"
#include "construction.h"
#include "construction_group.h"
#include "coordinates.h"
#include "cursesdef.h"
#include "debug.h"
#include "faction.h"
#include "fstream_utils.h"
#include "game.h"
#include "generic_factory.h"
#include "iexamine.h"
#include "int_id.h"
#include "item.h"
#include "item_category.h"
#include "item_search.h"
#include "itype.h"
#include "json.h"
#include "line.h"
#include "make_static.h"
#include "map/map.h"
#include "map_iterator.h"
#include "memory_fast.h"
#include "output.h"
#include "player.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id_implement.h"
#include "ui.h"
#include "value_ptr.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <iosfwd>
#include <iterator>
#include <mutex>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

static const item_category_id itcat_food( "food" );

static const zone_type_id zone_CONSTRUCTION_BLUEPRINT( "CONSTRUCTION_BLUEPRINT" );
static const zone_type_id zone_FARM_PLOT( "FARM_PLOT" );
static const zone_type_id zone_LOOT_CORPSE( "LOOT_CORPSE" );
static const zone_type_id zone_LOOT_CUSTOM( "LOOT_CUSTOM" );
static const zone_type_id zone_LOOT_DRINK( "LOOT_DRINK" );
static const zone_type_id zone_LOOT_FOOD( "LOOT_FOOD" );
static const zone_type_id zone_LOOT_IGNORE( "LOOT_IGNORE" );
static const zone_type_id zone_LOOT_PDRINK( "LOOT_PDRINK" );
static const zone_type_id zone_LOOT_PFOOD( "LOOT_PFOOD" );
static const zone_type_id zone_LOOT_SEEDS( "LOOT_SEEDS" );
static const zone_type_id zone_LOOT_UNSORTED( "LOOT_UNSORTED" );
static const zone_type_id zone_LOOT_DUMP( "LOOT_DUMP" );
static const zone_type_id zone_LOOT_WOOD( "LOOT_WOOD" );
static const zone_type_id zone_NO_AUTO_PICKUP( "NO_AUTO_PICKUP" );
static const zone_type_id zone_NO_NPC_PICKUP( "NO_NPC_PICKUP" );
static const zone_type_id zone_CONSTRUCTION_IGNORE( "CONSTRUCTION_IGNORE" );

namespace
{

auto current_dimension() -> const dimension_id & // *NOPAD*
{
    return get_map().get_bound_dimension();
}

auto current_dimension_zone_hash( const zone_type_id &type, const faction_id &fac ) -> std::string
{
    return zone_data::make_type_hash( type, fac, current_dimension() );
}

auto is_in_current_dimension( const zone_data &zone ) -> bool
{
    return zone.get_dimension() == current_dimension();
}

} // namespace

zone_manager::zone_manager()
{
    for( const zone_type &zone : zone_type::get_all() ) {
        types.emplace( zone.id, zone );
    }
}

zone_manager &zone_manager::get_manager()
{
    static zone_manager manager;
    return manager;
}

void zone_manager::reset_manager()
{
    get_manager() = zone_manager();
}

std::string zone_type::name() const
{
    return name_.translated();
}

auto zone_type::display_name() const -> std::string
{
    return icon_.empty() ? name() : icon_ + " " + name();
}

std::string zone_type::desc() const
{
    return _( desc_ );
}

auto zone_type::default_name() const -> std::string
{
    return default_name_.empty() ? display_name() : _( default_name_ );
}

auto zone_type::color() const -> nc_color { return color_; }

namespace
{
generic_factory<zone_type> zone_type_factory( "zone_type" );

struct zone_bounds {
    tripoint_abs_ms min;
    tripoint_abs_ms max;
};

auto make_zone_bounds( const tripoint_abs_ms &first, const tripoint_abs_ms &second ) -> zone_bounds
{
    return zone_bounds{
        tripoint_abs_ms( std::min( first.x(), second.x() ), std::min( first.y(), second.y() ),
        std::min( first.z(), second.z() ) ),
        tripoint_abs_ms( std::max( first.x(), second.x() ), std::max( first.y(), second.y() ),
        std::max( first.z(), second.z() ) )
    };
}

auto rectangle_points( const zone_bounds &bounds ) -> std::vector<tripoint_abs_ms>
{
    auto points = std::vector<tripoint_abs_ms>();
    const auto x_span = bounds.max.x() - bounds.min.x() + 1;
    const auto y_span = bounds.max.y() - bounds.min.y() + 1;
    const auto z_span = bounds.max.z() - bounds.min.z() + 1;
    points.reserve( static_cast<size_t>( x_span * y_span * z_span ) );
    for( auto z = bounds.min.z(); z <= bounds.max.z(); ++z ) {
        for( auto y = bounds.min.y(); y <= bounds.max.y(); ++y ) {
            for( auto x = bounds.min.x(); x <= bounds.max.x(); ++x ) {
                points.emplace_back( x, y, z );
            }
        }
    }
    return points;
}
} // namespace

IMPLEMENT_STRING_AND_INT_IDS( zone_type, zone_type_factory );

const std::vector<zone_type> &zone_type::get_all()
{
    return zone_type_factory.get_all();
}

void zone_type::load_zones( const JsonObject &jo, const std::string &src )
{
    zone_type_factory.load( jo, src );
}

void zone_type::reset_zones()
{
    zone_type_factory.reset();
}

void zone_type::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", name_ );
    optional( jo, was_loaded, "icon", icon_, "" );
    optional( jo, was_loaded, "default_name", default_name_, "" );
    mandatory( jo, was_loaded, "id", id );
    optional( jo, was_loaded, "description", desc_, "" );
    optional( jo, was_loaded, "color", color_, c_white );
}

namespace
{

auto zone_destination_accepts( const item &candidate, const tripoint_abs_ms &destination ) -> bool
{
    auto &here = get_map();
    const auto local = abs_to_bub( destination );
    if( !here.inbounds( local ) || !here.can_put_items_ter_furn( local ) ||
        static_cast<int>( here.i_at( local ).size() ) >= MAX_ITEM_IN_SQUARE ) {
        return false;
    }
    const auto cargo = here.veh_at( local ).part_with_feature( "CARGO", false );
    const auto space = cargo ? cargo->vehicle().free_volume( cargo->part_index() ) : here.free_volume(
                           local );
    return space >= candidate.volume();
}

} // namespace

auto loot_zone_presets() -> const std::vector<loot_zone_preset> & // *NOPAD*
{
    const auto ids = []( std::initializer_list<const char *> names ) {
        auto result = std::vector<zone_type_id>();
        for( const auto name : names ) {
            result.emplace_back( name );
        }
        return result;
    };
    static const auto presets = std::vector<loot_zone_preset> {
        {
            "food", translate_marker( "All food and drinks" ), translate_marker( "Food and drinks" ), 1,
            ids( { "LOOT_FOOD", "LOOT_PFOOD", "LOOT_COOKING_INGREDIENTS", "LOOT_DRINK", "LOOT_PDRINK" } )
        },
        {
            "weapons", translate_marker( "All weapons and ammunition" ), translate_marker( "Weapons and ammunition" ), 2,
            ids( { "LOOT_GUNS", "LOOT_WEAPONS", "LOOT_AMMO", "LOOT_MAGAZINES", "LOOT_MODS" } )
        },
        {
            "tools", translate_marker( "All tools" ), translate_marker( "Tools" ), 3,
            ids( { "LOOT_TOOLS", "LOOT_ENTRY_TOOLS", "LOOT_WORKSHOP_TOOLS", "LOOT_COOKING_TOOLS", "LOOT_CHEMISTRY_TOOLS", "LOOT_FARM_TOOLS" } )
        },
        {
            "materials", translate_marker( "All materials" ), translate_marker( "Materials" ), 10,
            ids( { "LOOT_SCRAP_WOOD", "LOOT_SCRAP_METAL", "LOOT_SCRAP_PLASTIC", "LOOT_SCRAP_GLASS", "LOOT_SCRAP_CERAMICS", "LOOT_SCRAP_FABRIC", "LOOT_SCRAP_ELECTRONICS" } )
        }
    };
    return presets;
}

auto loot_options::set_preset( const loot_zone_preset &preset ) -> void
{
    superzone = preset.id;
    categories = preset.categories;
    mark.clear();
}

auto loot_options::matches( const item &candidate ) const -> bool
{
    if( is_superzone() ) {
        const auto &cat = candidate.get_category();
        auto category = cat.zone().value_or( zone_type_id() );
        if( cat.get_id() == itcat_food ) {
            const auto food = candidate.get_food();
            const auto drink = food && food->get_comestible()->comesttype == "DRINK";
            const auto preserves = candidate.is_food_container() && candidate.type->container->preserves;
            const auto perishable = food && !preserves && food->goes_bad();
            category = drink ? ( perishable ? zone_LOOT_PDRINK : zone_LOOT_DRINK ) :
                       ( perishable ? zone_LOOT_PFOOD : zone_LOOT_FOOD );
        }
        if( std::ranges::contains( categories, zone_LOOT_WOOD ) &&
            candidate.has_flag( STATIC( flag_id( "FIREWOOD" ) ) ) ) {
            category = zone_LOOT_WOOD;
        }
        if( std::ranges::contains( categories, zone_LOOT_CORPSE ) && candidate.is_corpse() ) {
            category = zone_LOOT_CORPSE;
        }
        if( !std::ranges::contains( categories, category ) ) {
            return false;
        }
    }
    return is_superzone() && mark.empty() ? true : item_filter_from_string( mark )( candidate );
}

auto zone_creation_rules() -> std::vector<zone_type_id>
{
    return { zone_LOOT_UNSORTED, zone_LOOT_IGNORE, zone_type_id( "LOOT_IGNORE_FAVORITES" ),
             zone_LOOT_DUMP, zone_NO_AUTO_PICKUP, zone_NO_NPC_PICKUP };
}

auto zone_storage_categories() -> std::vector<zone_type_id>
{
    const auto rules = zone_creation_rules();
    auto result = std::vector<zone_type_id>();
    for( const auto &[id, type] : zone_manager::get_manager().get_types() ) {
        if( id.str().starts_with( "LOOT_" ) && id != zone_LOOT_CUSTOM &&
            !std::ranges::contains( rules, id ) ) {
            result.push_back( id );
        }
    }
    std::ranges::sort( result, []( const auto & left, const auto & right ) {
        const auto rank = []( const auto & id ) {
            const auto chars = utf8_to_utf32( id->display_name() );
            return zone_icon_order( chars.empty() ? 0 : chars.front() );
        };
        return rank( left ) == rank( right ) ? localized_compare( left->name(), right->name() ) :
               rank( left ) < rank( right );
    } );
    return result;
}

auto toggle_zone_categories( std::vector<zone_type_id> &selected,
                             const std::vector<zone_type_id> &members ) -> void
{
    const auto all = std::ranges::all_of( members, [&]( const auto & id ) {
        return std::ranges::contains( selected, id );
    } );
    for( const auto &id : members ) {
        if( all ) {
            std::erase( selected, id );
        } else if( !std::ranges::contains( selected, id ) ) {
            selected.push_back( id );
        }
    }
}

auto query_zone_creation( const zone_creation_selection &initial,
                          const bool categories_only ) -> std::optional<zone_creation_selection>
{
    struct checklist_row {
        std::string label;
        std::string description;
        std::vector<zone_type_id> members;
        bool rule = false;
    };
    auto rows = std::vector<checklist_row>();
    const auto available = zone_storage_categories();
    auto grouped = std::vector<zone_type_id>();
    for( const auto &preset : loot_zone_presets() ) {
        auto members = std::vector<zone_type_id>();
        for( const auto &id : preset.categories ) {
            if( std::ranges::contains( available, id ) ) {
                members.push_back( id );
                grouped.push_back( id );
            }
        }
        if( members.empty() ) {
            continue;
        }
        rows.push_back( { .label = _( preset.title ),
                          .description = _( "Toggle the whole group; individual categories can be changed below." ),
                          .members = members } );
        for( const auto &id : members ) {
            rows.push_back( { .label = "  " + id->name(), .description = id->desc(), .members = { id } } );
        }
    }
    for( const auto &id : available ) {
        if( !std::ranges::contains( grouped, id ) ) {
            rows.push_back( { .label = id->name(), .description = id->desc(), .members = { id } } );
        }
    }
    if( !categories_only ) {
        for( const auto &id : zone_creation_rules() ) {
            if( id.is_valid() ) {
                rows.push_back( { .label = id->name(), .description = id->desc(),
                                  .members = { id }, .rule = true } );
            }
        }
    }
    auto selection = initial;
    auto cursor = 0;
    while( true ) {
        auto menu = uilist();
        menu.text = _( "Create zone: categories and area rules" );
        menu.desc_enabled = true;
        menu.addentry_desc( 0, true, MENU_AUTOASSIGN, _( "Confirm selection" ),
                            _( "Selected categories share one storage zone. Area rules are created separately on the same rectangle. Specialized storage takes priority." ) );
        menu.addentry( 1, true, MENU_AUTOASSIGN, _( "Clear selection" ) );
        if( !categories_only ) {
            menu.addentry_desc( 2, true, MENU_AUTOASSIGN, _( "Manual text filter..." ),
                                _( "Use the original item filter syntax, without category checkboxes." ) );
            menu.addentry_desc( 3, true, MENU_AUTOASSIGN, _( "Standard zone type..." ),
                                _( "Create any standard zone, including construction, farming and vehicle zones." ) );
        }
        const auto offset = categories_only ? 2 : 4;
        auto rules_heading = false;
        for( auto i = 0; i < static_cast<int>( rows.size() ); ++i ) {
            const auto &row = rows[i];
            if( row.rule && !rules_heading ) {
                menu.addentry( -1, false, 0, _( "Area rules (separate zones on the same rectangle)" ) );
                rules_heading = true;
            }
            const auto &checked = row.rule ? selection.rules : selection.categories;
            const auto count = std::ranges::count_if( row.members, [&]( const auto & id ) {
                return std::ranges::contains( checked, id );
            } );
            const auto mark = count == 0 ? " " : count == static_cast<int>( row.members.size() ) ? "x" : "-";
            menu.addentry_desc( i + offset, true, MENU_AUTOASSIGN,
                                string_format( "[%s] %s", mark, row.label ), row.description );
            if( row.members.size() > 1 ) {
                menu.entries.back().text_color = c_light_cyan;
            }
        }
        menu.selected = cursor;
        menu.query();
        cursor = menu.selected;
        if( menu.ret < 0 ) {
            return std::nullopt;
        }
        if( menu.ret == 0 ) {
            if( !selection.categories.empty() || !selection.rules.empty() ) {
                return selection;
            }
            popup( _( "Select at least one category or area rule." ) );
        } else if( menu.ret == 1 ) {
            selection.categories.clear();
            selection.rules.clear();
        } else if( !categories_only && menu.ret == 2 ) {
            return zone_creation_selection{ .single_type = zone_LOOT_CUSTOM };
        } else if( !categories_only && menu.ret == 3 ) {
            const auto type = zone_manager::get_manager().query_type();
            if( type ) {
                return zone_creation_selection{ .single_type = type };
            }
        } else {
            const auto &row = rows.at( menu.ret - offset );
            toggle_zone_categories( row.rule ? selection.rules : selection.categories, row.members );
        }
    }
}

auto loot_options::query_categories() -> bool
{
    const auto selected = query_zone_creation( { .categories = categories }, true );
    if( !selected ) {
        return false;
    }
    categories = selected->categories;
    return true;
}

shared_ptr_fast<zone_options> zone_options::create( const zone_type_id &type )
{
    if( type == zone_FARM_PLOT ) {
        return make_shared_fast<plot_options>();
    } else if( type == zone_CONSTRUCTION_BLUEPRINT ) {
        return make_shared_fast<blueprint_options>();
    } else if( type == zone_LOOT_CUSTOM ) {
        return make_shared_fast<loot_options>();
    }

    return make_shared_fast<zone_options>();
}

bool zone_options::is_valid( const zone_type_id &type, const zone_options &options )
{
    if( type == zone_FARM_PLOT ) {
        return dynamic_cast<const plot_options *>( &options ) != nullptr;
    } else if( type == zone_CONSTRUCTION_BLUEPRINT ) {
        return dynamic_cast<const blueprint_options *>( &options ) != nullptr;
    } else if( type == zone_LOOT_CUSTOM ) {
        return dynamic_cast<const loot_options *>( &options ) != nullptr;
    }

    // ensure options is not derived class for the rest of zone types
    return !options.has_options();
}

namespace
{
auto blueprint_layout_to_string( const blueprint_options::blueprint_layout layout ) -> std::string
{
    switch( layout ) {
    case blueprint_options::blueprint_layout::rectangle_fill:
        return "rectangle_fill";
    case blueprint_options::blueprint_layout::rectangle_border:
        return "rectangle_border";
    case blueprint_options::blueprint_layout::circle_fill:
        return "circle_fill";
    case blueprint_options::blueprint_layout::circle_border:
        return "circle_border";
}
debugmsg( "Unhandled blueprint layout" );
return "rectangle_fill";
}

auto blueprint_layout_from_string( const std::string &value ) -> blueprint_options::blueprint_layout
{
    if( value == "rectangle_border" ) {
        return blueprint_options::blueprint_layout::rectangle_border;
    }
    if( value == "circle_fill" ) {
        return blueprint_options::blueprint_layout::circle_fill;
    }
    if( value == "circle_border" ) {
        return blueprint_options::blueprint_layout::circle_border;
    }
    return blueprint_options::blueprint_layout::rectangle_fill;
}

auto blueprint_layout_description( const blueprint_options::blueprint_layout layout ) -> std::string
{
    switch( layout ) {
    case blueprint_options::blueprint_layout::rectangle_fill:
        return _( "Filled rectangle" );
        case blueprint_options::blueprint_layout::rectangle_border:
            return _( "Rectangle border" );
        case blueprint_options::blueprint_layout::circle_fill:
            return _( "Filled circle" );
        case blueprint_options::blueprint_layout::circle_border:
            return _( "Circle border" );
    }
    debugmsg( "Unhandled blueprint layout in description" );
    return _( "Filled rectangle" );
}

struct circle_shape {
    double center_x = 0.0;
    double center_y = 0.0;
    double outer_sq = 0.0;
    double inner_sq = 0.0;
};

auto make_circle_shape( const zone_bounds &bounds, const bool border_only ) -> circle_shape
{
    const double center_x = static_cast<double>( bounds.min.x() + bounds.max.x() ) / 2.0;
    const double center_y = static_cast<double>( bounds.min.y() + bounds.max.y() ) / 2.0;
    const double radius = static_cast<double>( std::max( bounds.max.x() - bounds.min.x(),
        bounds.max.y() - bounds.min.y() ) ) / 2.0;
    const double outer_radius = radius + 1.0;
    const double inner_radius = border_only ? std::max( radius - 1.0, 0.0 ) : 0.0;

    return circle_shape{
        center_x, center_y, outer_radius * outer_radius, inner_radius *inner_radius
    };
}

auto point_in_circle( const circle_shape &shape, const tripoint_abs_ms &candidate ) -> bool
{
    const double dx = static_cast<double>( candidate.x() ) - shape.center_x;
    const double dy = static_cast<double>( candidate.y() ) - shape.center_y;
    const double dist_sq = dx * dx + dy * dy;
    return dist_sq <= shape.outer_sq && dist_sq >= shape.inner_sq;
}

} // namespace

construction_id blueprint_options::get_final_construction(
    const std::vector<construction_id> &list_constructions,
    const construction_id &id,
    std::set<construction_id> &skip_index
)
{
    if( id->post_terrain.is_empty() && id->post_furniture.is_empty() ) {
        return id;
    }

    for( const construction_id &c_id : list_constructions ) {
        if( c_id == id || skip_index.contains( c_id ) ) {
            continue;
        }
        const construction &c = *c_id;
        if( id->group == c.group &&
            ( id->post_terrain == c.pre_terrain || id->post_furniture == c.pre_furniture ) ) {
            skip_index.insert( id );
            return get_final_construction( list_constructions, c_id, skip_index );
        }
    }

    return id;
}

auto blueprint_options::get_covered_points( const tripoint_abs_ms &start,
        const tripoint_abs_ms &end ) const -> std::vector<tripoint_abs_ms>
{
    const auto bounds = make_zone_bounds( start, end );
    const auto circle_layout = layout == blueprint_layout::circle_fill ||
                               layout == blueprint_layout::circle_border;
    const auto border_only = layout == blueprint_layout::rectangle_border ||
                             layout == blueprint_layout::circle_border;

    if( !circle_layout && !border_only ) {
        return rectangle_points( bounds );
    }

    if( !circle_layout && border_only ) {
        auto points = std::vector<tripoint_abs_ms>();
        const auto x_span = bounds.max.x() - bounds.min.x() + 1;
        const auto y_span = bounds.max.y() - bounds.min.y() + 1;
        const auto z_span = bounds.max.z() - bounds.min.z() + 1;
        points.reserve( static_cast<size_t>( x_span * y_span * z_span ) );
        for( auto z = bounds.min.z(); z <= bounds.max.z(); ++z ) {
            for( auto y = bounds.min.y(); y <= bounds.max.y(); ++y ) {
                for( auto x = bounds.min.x(); x <= bounds.max.x(); ++x ) {
                    if( x == bounds.min.x() || x == bounds.max.x() ||
                        y == bounds.min.y() || y == bounds.max.y() ) {
                        points.emplace_back( x, y, z );
                    }
                }
            }
        }
        return points;
    }

    const auto shape = make_circle_shape( bounds, border_only );
    if( circle_layout && border_only ) {
        auto fill_set = std::unordered_set<tripoint_abs_ms>();
        const auto circle_fill = make_circle_shape( bounds, false );
        for( auto z = bounds.min.z(); z <= bounds.max.z(); ++z ) {
            for( auto y = bounds.min.y(); y <= bounds.max.y(); ++y ) {
                for( auto x = bounds.min.x(); x <= bounds.max.x(); ++x ) {
                    const tripoint_abs_ms pt( x, y, z );
                    if( point_in_circle( circle_fill, pt ) ) {
                        fill_set.insert( pt );
                    }
                }
            }
        }
        auto border_set = std::unordered_set<tripoint_abs_ms>();
        const auto neighbors = std::array<point, 8> {
            point_east, point_west, point_north, point_south,
            point_east + point_north, point_east + point_south,
            point_west + point_north, point_west + point_south
        };
        std::ranges::for_each( fill_set, [&]( const tripoint_abs_ms & pt ) {
            const bool at_edge = std::ranges::any_of( neighbors, [&]( const point & dir ) {
                const tripoint_abs_ms neigh( pt.xy() + dir, pt.z() );
                return !fill_set.contains( neigh );
            } );
            if( at_edge ) {
                border_set.insert( pt );
            }
        } );
        return std::vector<tripoint_abs_ms>( border_set.begin(), border_set.end() );
    }

    auto points = std::vector<tripoint_abs_ms>();
    const auto x_span = bounds.max.x() - bounds.min.x() + 1;
    const auto y_span = bounds.max.y() - bounds.min.y() + 1;
    const auto z_span = bounds.max.z() - bounds.min.z() + 1;
    points.reserve( static_cast<size_t>( x_span * y_span * z_span ) );
    for( auto z = bounds.min.z(); z <= bounds.max.z(); ++z ) {
        for( auto y = bounds.min.y(); y <= bounds.max.y(); ++y ) {
            for( auto x = bounds.min.x(); x <= bounds.max.x(); ++x ) {
                const tripoint_abs_ms point( x, y, z );
                if( point_in_circle( shape, point ) ) {
                    points.push_back( point );
                }
            }
        }
    }
    return points;
}

auto blueprint_options::has_inside( const tripoint_abs_ms &start, const tripoint_abs_ms &end,
                                    const tripoint_abs_ms &candidate ) const -> bool
{
    const auto bounds = make_zone_bounds( start, end );
    if( candidate.z() < bounds.min.z() || candidate.z() > bounds.max.z() ) {
        return false;
    }
    const auto circle_layout = layout == blueprint_layout::circle_fill ||
                               layout == blueprint_layout::circle_border;
    if( circle_layout && layout == blueprint_layout::circle_border ) {
        const auto shape = make_circle_shape( bounds, false );
        if( !point_in_circle( shape, candidate ) ) {
            return false;
        }
        const auto neighbors = std::array<point, 8> {
            point_east, point_west, point_north, point_south,
            point_east + point_north, point_east + point_south,
            point_west + point_north, point_west + point_south
        };
        return std::ranges::any_of( neighbors, [&]( const point & dir ) {
            const tripoint_abs_ms neigh( candidate.xy() + dir, candidate.z() );
            return !point_in_circle( shape, neigh );
        } );
    }
    const auto border_only = layout == blueprint_layout::rectangle_border ||
                             layout == blueprint_layout::circle_border;
    if( !circle_layout ) {
        const bool in_bounds = candidate.x() >= bounds.min.x() && candidate.x() <= bounds.max.x() &&
                               candidate.y() >= bounds.min.y() && candidate.y() <= bounds.max.y();
        if( !in_bounds ) {
            return false;
        }
        if( !border_only ) {
            return true;
        }
        return candidate.x() == bounds.min.x() || candidate.x() == bounds.max.x() ||
               candidate.y() == bounds.min.y() || candidate.y() == bounds.max.y();
    }
    if( candidate.x() < bounds.min.x() || candidate.x() > bounds.max.x() ||
        candidate.y() < bounds.min.y() || candidate.y() > bounds.max.y() ) {
        return false;
    }
    const auto shape = make_circle_shape( bounds, border_only );
    return point_in_circle( shape, candidate );
}

auto blueprint_options::query_con() -> query_con_result
{
    std::optional<construction_id> con_index = construction_menu( true );
    if( !con_index ) {
        return canceled;
    }
    const std::vector<construction_id> &list_constructions = constructions::get_all_sorted();
    std::set<construction_id> skip_index;
    con_index = get_final_construction( list_constructions, *con_index, skip_index );

    const construction &chosen = con_index->obj();

    const construction_group_str_id &chosen_group = chosen.group;
    const std::string &chosen_mark = chosen.post_terrain.is_empty() ?
                                     chosen.post_furniture.str() : chosen.post_terrain.str();

    if( *con_index != index || chosen_group != group || chosen_mark != mark ) {
        group = chosen_group;
        mark = chosen_mark;
        index = *con_index;
        return changed;
    } else {
        return successful;
    }
}

auto blueprint_options::query_layout() -> query_layout_result
{
    static const auto layouts = std::to_array<blueprint_layout>( {
        blueprint_layout::rectangle_fill, blueprint_layout::rectangle_border,
        blueprint_layout::circle_fill, blueprint_layout::circle_border
    } );

    uilist layout_menu;
    layout_menu.text = _( "Select placement layout" );
    layout_menu.desc_enabled = false;
    auto index = 0;
    std::ranges::for_each( layouts, [&]( const blueprint_layout & candidate ) {
        layout_menu.addentry( index, true, MENU_AUTOASSIGN,
                              blueprint_layout_description( candidate ) );
        if( candidate == layout ) {
            layout_menu.selected = index;
        }
        index++;
    } );

    layout_menu.query();
    if( layout_menu.ret < 0 || layout_menu.ret >= static_cast<int>( layouts.size() ) ) {
        return query_layout_result::canceled;
    }

    const blueprint_layout chosen = layouts[layout_menu.ret];
    if( chosen == layout ) {
        return query_layout_result::successful;
    }

    layout = chosen;
    return query_layout_result::changed;
}

loot_options::query_loot_result loot_options::query_loot()
{
    // Show the filter syntax inside the popup itself, so it stays visible while typing.
    string_input_popup()
    .title( _( "Filter:" ) )
    .description( item_filter_rules_text( item_filter_type::FILTER ) )
    .width( 55 )
    .identifier( "item_filter" )
    .max_length( 256 )
    .edit( mark );
    return changed;
}

plot_options::query_seed_result plot_options::query_seed()
{
    player &p = g->u;
    map &here = get_map();

    std::vector<item *> seed_inv = p.items_with( []( const item & itm ) {
        return itm.is_seed();
    } );
    auto &mgr = zone_manager::get_manager();
    const std::unordered_set<tripoint_abs_ms> &zone_src_set = mgr.get_near( zone_LOOT_SEEDS,
        p.abs_pos(), 60 );
    for( const tripoint_abs_ms &elem : zone_src_set ) {
        auto elem_loc = abs_to_bub( elem );
        for( item * const &it : here.i_at( elem_loc ) ) {
            if( it->is_seed() ) {
                seed_inv.push_back( it );
            }
        }
    }
    std::vector<seed_tuple> seed_entries = iexamine::get_seed_entries( seed_inv );
    seed_entries.emplace( seed_entries.begin(), itype_id( "null" ), _( "No seed" ), 0 );

    int seed_index = iexamine::query_seed( seed_entries, 0 );

    if( seed_index > 0 && seed_index < static_cast<int>( seed_entries.size() ) ) {
        const auto &seed_entry = seed_entries[seed_index];
        const itype_id &new_seed = std::get<0>( seed_entry );
        itype_id new_mark;

        if( new_seed->is_seed() ) {
            new_mark = new_seed->seed->fruit_id;
        } else {
            new_mark = seed;
        }

        if( new_seed != seed || new_mark != mark ) {
            seed = new_seed;
            mark = new_mark;
            return changed;
        } else {
            return successful;
        }
    } else if( seed_index == 0 ) {
        // No seeds
        if( !seed.is_empty() || !mark.is_empty() ) {
            seed = itype_id();
            mark = itype_id();
            return changed;
        } else {
            return successful;
        }
    } else {
        return canceled;
    }
}

bool loot_options::query_at_creation()
{
    return is_superzone() ? query_categories() : query_loot() != canceled;
}

bool loot_options::query()
{
    if( is_superzone() ) {
        auto menu = uilist();
        menu.text = _( "Edit category storage" );
        menu.addentry( 0, true, MENU_AUTOASSIGN, _( "Categories" ) );
        menu.addentry( 1, true, MENU_AUTOASSIGN, _( "Additional filter" ) );
        menu.query();
        return menu.ret == 0 ? query_categories() : menu.ret == 1 && query_loot() == changed;
    }
    return query_loot() == changed;
}

std::string loot_options::get_zone_name_suggestion() const
{
    if( superzone == "categories" ) {
        return _( "Category storage" );
    }
    const auto found = std::ranges::find( loot_zone_presets(), superzone, &loot_zone_preset::id );
    return found == loot_zone_presets().end() ? std::string() : _( found->name );
}

std::vector<std::pair<std::string, std::string>> loot_options::get_descriptions() const
{
    std::vector<std::pair<std::string, std::string>> options;
    if( is_superzone() ) {
        auto labels = std::string();
        for( const auto &id : categories ) {
            if( !labels.empty() ) {
                labels += ", ";
            }
            labels += id.is_valid() ? id->name() : id.str();
        }
        options.emplace_back( _( "Categories: " ), labels );
    }
    if( !mark.empty() || !is_superzone() ) {
        options.emplace_back( "", !mark.empty() ? mark : _( "No filter" ) );
    }

    return options;
}

void loot_options::serialize( JsonOut &json ) const
{
    json.member( "mark", mark );
    if( is_superzone() ) {
        json.member( "superzone", superzone );
        json.member( "categories", categories );
    }
}

void loot_options::deserialize( const JsonObject &jo_zone )
{
    jo_zone.read( "mark", mark );
    jo_zone.read( "superzone", superzone );
    jo_zone.read( "categories", categories );
}

bool blueprint_options::query_at_creation()
{
    if( query_con() == canceled ) {
        return false;
    }
    return query_layout() != query_layout_result::canceled;
}

bool plot_options::query_at_creation()
{
    return query_seed() != canceled;
}

bool blueprint_options::query()
{
    uilist menu;
    menu.text = _( "Edit blueprint options" );
    menu.addentry( 0, true, 'c', _( "Change construction" ) );
    menu.addentry( 1, true, 'l', _( "Change placement layout" ) );
    menu.query();

    switch( menu.ret ) {
        case 0:
            return query_con() == changed;
        case 1:
            return query_layout() == query_layout_result::changed;
        default:
            return false;
    }
}

bool plot_options::query()
{
    return query_seed() == changed;
}

std::string blueprint_options::get_zone_name_suggestion() const
{
    if( group ) {
        return group->name();
    }

    return _( "No construction" );
}

std::string plot_options::get_zone_name_suggestion() const
{
    if( !seed.is_empty() ) {
        auto type = itype_id( seed );
        if( seed->is_seed() ) {
            return seed->seed->plant_name.translated();
        } else {
            return item::nname( type );
        }
    }

    return _( "No seed" );
}

std::vector<std::pair<std::string, std::string>> blueprint_options::get_descriptions() const
{
    auto options = std::vector<std::pair<std::string, std::string>>();
    options.emplace_back( _( "Construct: " ), group ? group->name() : _( "No Construction" ) );
    options.emplace_back( _( "Layout: " ), blueprint_layout_description( layout ) );

    return options;
}

std::vector<std::pair<std::string, std::string>> plot_options::get_descriptions() const
{
    auto options = std::vector<std::pair<std::string, std::string>>();
    options.emplace_back(
        _( "Plant seed: " ),
        !seed.is_empty() ? item::nname( itype_id( seed ) ) : _( "No seed" ) );

    return options;
}

void blueprint_options::serialize( JsonOut &json ) const
{
    json.member( "mark", mark );
    json.member( "group", group );
    json.member( "index", index.id() );
    json.member( "layout", blueprint_layout_to_string( layout ) );
}

void blueprint_options::deserialize( const JsonObject &jo_zone )
{
    jo_zone.read( "mark", mark );
    jo_zone.read( "group", group );
    if( jo_zone.has_int( "index" ) ) {
        // Oops, saved incorrectly as an int id by legacy code. Just load it and hope for the best
        index = construction_id( jo_zone.get_int( "index" ) );
    } else {
        index = construction_str_id( jo_zone.get_string( "index" ) ).id();
    }
    auto layout_name = std::string();
    if( jo_zone.read( "layout", layout_name ) ) {
        layout = blueprint_layout_from_string( layout_name );
    } else {
        layout = blueprint_layout::rectangle_fill;
    }
}

void plot_options::serialize( JsonOut &json ) const
{
    json.member( "mark", mark );
    json.member( "seed", seed );
}

void plot_options::deserialize( const JsonObject &jo_zone )
{
    jo_zone.read( "mark", mark );
    jo_zone.read( "seed", seed );
}

auto get_zone_covered_points( const zone_data &zone ) -> std::vector<tripoint_abs_ms>
{
    const auto bounds = make_zone_bounds( zone.get_start_point(), zone.get_end_point() );
    if( zone.get_type() == zone_CONSTRUCTION_BLUEPRINT ) {
        const auto *bp_options = dynamic_cast<const blueprint_options *>( &zone.get_options() );
        if( bp_options != nullptr ) {
            return bp_options->get_covered_points( bounds.min, bounds.max );
        }
    }
    return rectangle_points( bounds );
}

std::optional<std::string> zone_manager::query_name( const std::string &default_name ) const
{
    string_input_popup popup;
    popup
    .title( _( "Zone name:" ) )
    .width( 55 )
    .text( default_name )
    .query();
    if( popup.canceled() ) {
        return {};
    } else {
        return popup.text();
    }
}

std::optional<zone_type_id> zone_manager::query_type( loot_options * /*preset*/ ) const
{
    const auto &types = get_manager().get_types();
    std::vector<std::pair<zone_type_id, zone_type>> types_vec;
    std::copy( types.begin(), types.end(),
               std::back_inserter<std::vector<std::pair<zone_type_id, zone_type>>>( types_vec ) );
    std::sort( types_vec.begin(), types_vec.end(),
    []( const std::pair<zone_type_id, zone_type> &lhs, const std::pair<zone_type_id, zone_type> &rhs ) {
        const auto left = lhs.second.display_name();
        const auto right = rhs.second.display_name();
        const auto rank = []( const std::string & text ) {
            const auto codepoints = utf8_to_utf32( text );
            return zone_icon_order( codepoints.empty() ? 0 : codepoints.front() );
        };
        const auto left_rank = rank( left );
        const auto right_rank = rank( right );
        return left_rank == right_rank ? localized_compare( left, right ) : left_rank < right_rank;
    } );

    uilist as_m;
    as_m.desc_enabled = true;
    as_m.text = _( "Select zone type:" );

    for( const auto i : std::views::iota( 0, static_cast<int>( types_vec.size() ) ) ) {
        const auto &type = types_vec[i].second;
        as_m.addentry_desc( i, true, MENU_AUTOASSIGN, type.display_name(), type.desc() );
    }

    as_m.query();
    if( as_m.ret < 0 ) {
        return {};
    }
    const auto index = static_cast<size_t>( as_m.ret );
    return types_vec.at( index ).first;
}

bool zone_data::set_name()
{
    const auto maybe_name = zone_manager::get_manager().query_name( name );
    if( maybe_name.has_value() ) {
        auto new_name = maybe_name.value();
        if( new_name.empty() ) {
            new_name = _( "<no name>" );
        }
        if( name != new_name ) {
            zone_manager::get_manager().zone_edited( *this );
            name = new_name;
            return true;
        }
    }
    return false;
}

bool zone_data::set_type()
{
    const auto maybe_type = zone_manager::get_manager().query_type();
    if( maybe_type.has_value() && maybe_type.value() != type ) {
        auto new_options = zone_options::create( maybe_type.value() );
        if( new_options->query_at_creation() ) {
            zone_manager::get_manager().zone_edited( *this );
            type = maybe_type.value();
            options = new_options;
            zone_manager::get_manager().cache_data();
            return true;
        }
    }
    return false;
}

void zone_data::set_position( const std::pair<tripoint_abs_ms, tripoint_abs_ms> &position,
                              const bool manual )
{
    if( is_vehicle && manual ) {
        debugmsg( "Tried moving a lootzone bound to a vehicle part" );
        return;
    }
    start = position.first;
    end = position.second;

    zone_manager::get_manager().cache_data();
}

void zone_data::set_enabled( const bool enabled_arg )
{
    zone_manager::get_manager().zone_edited( *this );
    enabled = enabled_arg;
}

void zone_data::set_is_vehicle( const bool is_vehicle_arg )
{
    is_vehicle = is_vehicle_arg;
}

tripoint_abs_ms zone_data::get_center_point() const
{
    return tripoint_abs_ms( ( start.x() + end.x() ) / 2, ( start.y() + end.y() ) / 2,
                            ( start.z() + end.z() ) / 2 );
}

auto zone_data::has_inside( const tripoint_abs_ms &p ) const -> bool
{
    const zone_bounds bounds = make_zone_bounds( start, end );
    if( p.x() < bounds.min.x() || p.x() > bounds.max.x() ||
        p.y() < bounds.min.y() || p.y() > bounds.max.y() ||
        p.z() < bounds.min.z() || p.z() > bounds.max.z() ) {
        return false;
    }

    if( type == zone_CONSTRUCTION_BLUEPRINT ) {
        const auto *bp_options = dynamic_cast<const blueprint_options *>( options.get() );
        if( bp_options != nullptr ) {
            return bp_options->has_inside( bounds.min, bounds.max, p );
        }
    }

    return true;
}

std::string zone_manager::get_name_from_type( const zone_type_id &type ) const
{
    const auto &iter = types.find( type );
    if( iter != types.end() ) {
        return iter->second.display_name();
    }

    return "Unknown Type";
}

bool zone_manager::has_type( const zone_type_id &type ) const
{
    return types.contains( type );
}

bool zone_manager::has_defined( const zone_type_id &type, const faction_id &fac ) const
{
    const auto &type_iter = area_cache.find( current_dimension_zone_hash( type, fac ) );
    return type_iter != area_cache.end();
}

void zone_manager::cache_data()
{
    area_cache.clear();

    std::ranges::for_each( zones, [&]( zone_data & elem ) {
        if( !elem.get_enabled() ) {
            return;
        }

        auto &cache = area_cache[elem.get_type_hash()];
        const auto points = get_zone_covered_points( elem );
        std::ranges::for_each( points, [&]( const tripoint_abs_ms & point ) {
            cache.insert( point );
        } );
    } );
}

void zone_manager::cache_vzones()
{
    vzone_cache.clear();
    auto vzones = get_map().get_vehicle_zones( g->get_levz() );
    std::ranges::for_each( vzones, [&]( zone_data * elem ) {
        if( elem == nullptr || !elem->get_enabled() ) {
            return;
        }

        auto &cache = vzone_cache[current_dimension_zone_hash( elem->get_type(), elem->get_faction() )];
        const auto points = get_zone_covered_points( *elem );
        std::ranges::for_each( points, [&]( const tripoint_abs_ms & point ) {
            cache.insert( point );
        } );
    } );
}

std::unordered_set<tripoint_abs_ms> zone_manager::get_point_set( const zone_type_id &type,
        const faction_id &fac ) const
{
    const auto &type_iter = area_cache.find( current_dimension_zone_hash( type, fac ) );
    if( type_iter == area_cache.end() ) {
        return std::unordered_set<tripoint_abs_ms>();
    }

    return type_iter->second;
}

std::unordered_set<tripoint_abs_ms> zone_manager::get_point_set_loot( const tripoint_abs_ms &where,
        int radius, const faction_id &fac ) const
{
    return get_point_set_loot( where, radius, false, fac );
}

std::unordered_set<tripoint_abs_ms> zone_manager::get_point_set_loot( const tripoint_abs_ms &where,
        int radius, bool npc_search, const faction_id &/*fac*/ ) const
{
    std::unordered_set<tripoint_abs_ms> res;
    map &here = get_map();
    for( const tripoint_bub_ms elem : here.points_in_radius( abs_to_bub( where ), radius ) ) {
        const auto abs_pos = bub_to_abs( elem );
        const zone_data *zone = get_zone_at( abs_pos );
        // if not a LOOT zone
        if( ( !zone ) || ( zone->get_type().str().substr( 0, 4 ) != "LOOT" ) ) {
            continue;
        }
        if( npc_search && ( has( zone_NO_NPC_PICKUP, abs_pos ) ) ) {
            continue;
        }
        res.insert( abs_pos );
    }
    return res;
}

std::unordered_set<tripoint_abs_ms> zone_manager::get_vzone_set( const zone_type_id &type,
        const faction_id &fac ) const
{
    //Only regenerate the vehicle zone cache if any vehicles have moved
    const auto &type_iter = vzone_cache.find( current_dimension_zone_hash( type, fac ) );
    if( type_iter == vzone_cache.end() ) {
        return std::unordered_set<tripoint_abs_ms>();
    }

    return type_iter->second;
}

bool zone_manager::has( const zone_type_id &type, const tripoint_abs_ms &where,
                        const faction_id &fac ) const
{
    const auto &point_set = get_point_set( type, fac );
    const auto &vzone_set = get_vzone_set( type, fac );
    return point_set.contains( where ) || vzone_set.contains( where );
}

bool zone_manager::has_near( const zone_type_id &type, const tripoint_abs_ms &where, int range,
                             const faction_id &fac ) const
{
    const auto &point_set = get_point_set( type, fac );
    for( auto &point : point_set ) {
        if( point.z() == where.z() ) {
            if( square_dist( point, where ) <= range ) {
                return true;
            }
        }
    }

    const auto &vzone_set = get_vzone_set( type, fac );
    for( auto &point : vzone_set ) {
        if( point.z() == where.z() ) {
            if( square_dist( point, where ) <= range ) {
                return true;
            }
        }
    }

    return false;
}

bool zone_manager::has_loot_dest_near( const tripoint_abs_ms &where ) const
{
    for( const auto &ztype : get_manager().get_types() ) {
        const zone_type_id &type = ztype.first;
        if( type == zone_FARM_PLOT ||
            type == zone_LOOT_UNSORTED || type == zone_LOOT_IGNORE ||
            type == zone_CONSTRUCTION_BLUEPRINT ||
            type == zone_NO_AUTO_PICKUP || type == zone_NO_NPC_PICKUP ) {
            continue;
        }
        if( has_near( type, where ) ) {
            return true;
        }
    }
    return false;
}

const zone_data *zone_manager::get_zone_at( const tripoint_abs_ms &where,
        const zone_type_id &type ) const
{
    for( const zone_data &zone : zones ) {
        if( is_in_current_dimension( zone ) && zone.has_inside( where ) && zone.get_type() == type ) {
            return &zone;
        }
    }
    auto vzones = get_map().get_vehicle_zones( g->get_levz() );
    for( const zone_data *zone : vzones ) {
        if( zone->has_inside( where ) && zone->get_type() == type ) {
            return zone;
        }
    }
    return nullptr;
}

bool zone_manager::custom_loot_has( const tripoint_abs_ms &where, const item *it,
                                    const bool regular_only ) const
{
    if( !it ) {
        return false;
    }
    const auto matches = [&]( const zone_data & zone ) {
        if( zone.get_type() != zone_LOOT_CUSTOM || !zone.get_enabled() ||
            zone.get_faction() != your_fac || !zone.has_inside( where ) || !is_in_current_dimension( zone ) ) {
            return false;
        }
        const auto &options = dynamic_cast<const loot_options &>( zone.get_options() );
        return ( !regular_only || !options.is_superzone() ) && options.matches( *it );
    };
    if( std::ranges::any_of( zones, matches ) ) {
        return true;
    }
    const auto vehicle_zones = get_map().get_vehicle_zones( g->get_levz() );
    return std::ranges::any_of( vehicle_zones, [&]( const zone_data * zone ) { return matches( *zone ); } );
}

std::unordered_set<tripoint_abs_ms> zone_manager::get_near( const zone_type_id &type,
        const tripoint_abs_ms &where, int range, const item *it, const faction_id &fac ) const
{
    const auto &point_set = get_point_set( type, fac );
    auto near_point_set = std::unordered_set<tripoint_abs_ms>();

    for( auto &point : point_set ) {
        if( point.z() == where.z() ) {
            if( square_dist( point, where ) <= range ) {
                if( it && type == zone_LOOT_CUSTOM ) {
                    if( custom_loot_has( point, it ) ) {
                        near_point_set.insert( point );
                    }
                } else {
                    near_point_set.insert( point );
                }
            }
        }
    }

    const auto &vzone_set = get_vzone_set( type, fac );
    for( auto &point : vzone_set ) {
        if( point.z() == where.z() ) {
            if( square_dist( point, where ) <= range ) {
                if( it && type == zone_LOOT_CUSTOM ) {
                    if( custom_loot_has( point, it ) ) {
                        near_point_set.insert( point );
                    }
                } else {
                    near_point_set.insert( point );
                }
            }
        }
    }

    // Ordinary explicit filters retain precedence over category presets.
    if( type == zone_LOOT_CUSTOM && it &&
    std::ranges::any_of( near_point_set, [&]( const auto & point ) {
    return custom_loot_has( point, it, true ) && zone_destination_accepts( *it, point );
    } ) ) {
        std::erase_if( near_point_set, [&]( const auto & point ) { return !custom_loot_has( point, it, true ); } );
    }
    return near_point_set;
}

std::optional<tripoint_abs_ms> zone_manager::get_nearest( const zone_type_id &type,
        const tripoint_abs_ms &where,
        int range, const faction_id &fac ) const
{
    if( range < 0 ) {
        return std::nullopt;
    }

    tripoint_abs_ms nearest_pos = tripoint_abs_ms( INT_MIN, INT_MIN, INT_MIN );
    int nearest_dist = range + 1;
    const std::unordered_set<tripoint_abs_ms> &point_set = get_point_set( type, fac );
    for( const tripoint_abs_ms &p : point_set ) {
        int cur_dist = square_dist( p, where );
        if( cur_dist < nearest_dist ) {
            nearest_dist = cur_dist;
            nearest_pos = p;
            if( nearest_dist == 0 ) {
                return nearest_pos;
            }
        }
    }

    const std::unordered_set<tripoint_abs_ms> &vzone_set = get_vzone_set( type, fac );
    for( const tripoint_abs_ms &p : vzone_set ) {
        int cur_dist = square_dist( p, where );
        if( cur_dist < nearest_dist ) {
            nearest_dist = cur_dist;
            nearest_pos = p;
            if( nearest_dist == 0 ) {
                return nearest_pos;
            }
        }
    }
    if( nearest_dist > range ) {
        return std::nullopt;
    }
    return nearest_pos;
}

zone_type_id zone_manager::get_near_zone_type_for_item( const item &it,
        const tripoint_abs_ms &where, int range ) const
{
    const item_category &cat = it.get_category();

    const auto usable = [&]( const zone_type_id & type, const bool regular_only = false ) {
        for( const auto &dest : get_near( type, where, range, &it ) ) {
            if( ( !regular_only || custom_loot_has( dest, &it, true ) ) &&
                zone_destination_accepts( it, dest ) ) {
                return true;
            }
        }
        return false;
    };
    // Keep explicit text filters' priority; broad category presets are fallback destinations.
    if( usable( zone_LOOT_CUSTOM, true ) ) {
        return zone_LOOT_CUSTOM;
    }
    if( it.has_flag( STATIC( flag_id( "FIREWOOD" ) ) ) ) {
        if( usable( zone_LOOT_WOOD ) ) {
            return zone_LOOT_WOOD;
        }
    }
    if( it.is_corpse() ) {
        if( usable( zone_LOOT_CORPSE ) ) {
            return zone_LOOT_CORPSE;
        }
    }

    std::optional<zone_type_id> zone_check_first = cat.priority_zone( it );
    if( zone_check_first && usable( *zone_check_first ) ) {
        return *zone_check_first;
    }

    if( cat.zone() && usable( *cat.zone() ) ) {
        return *cat.zone();
    }

    if( cat.get_id() == itcat_food ) {
        const bool preserves = it.is_food_container() && it.type->container->preserves;

        // skip food without comestible, like MREs
        if( const item *it_food = it.get_food() ) {
            if( it_food->get_comestible()->comesttype == "DRINK" ) {
                if( !preserves && it_food->goes_bad() && usable( zone_LOOT_PDRINK ) ) {
                    return zone_LOOT_PDRINK;
                } else if( usable( zone_LOOT_DRINK ) ) {
                    return zone_LOOT_DRINK;
                }
            }

            if( !preserves && it_food->goes_bad() && usable( zone_LOOT_PFOOD ) ) {
                return zone_LOOT_PFOOD;
            }
        }
        if( usable( zone_LOOT_FOOD ) ) {
            return zone_LOOT_FOOD;
        }
    }

    if( usable( zone_LOOT_CUSTOM ) ) {
        return zone_LOOT_CUSTOM;
    }
    if( usable( zone_LOOT_DUMP ) ) {
        return zone_LOOT_DUMP;
    }


    return zone_type_id();
}

std::vector<zone_data> zone_manager::get_zones( const zone_type_id &type,
        const tripoint_abs_ms &where, const faction_id &fac ) const
{
    auto zones = std::vector<zone_data>();

    for( const auto &zone : this->zones ) {
        if( is_in_current_dimension( zone ) && zone.get_type() == type && zone.get_faction() == fac ) {
            if( zone.has_inside( where ) ) {
                zones.emplace_back( zone );
            }
        }
    }

    return zones;
}

const zone_data *zone_manager::get_zone_at( const tripoint_abs_ms &where ) const
{
    for( auto it = zones.rbegin(); it != zones.rend(); ++it ) {
        const auto &zone = *it;

        if( is_in_current_dimension( zone ) && zone.has_inside( where ) ) {
            return &zone;
        }
    }
    return nullptr;
}

const zone_data *zone_manager::get_bottom_zone( const tripoint_abs_ms &where,
        const faction_id &fac ) const
{
    for( auto it = zones.rbegin(); it != zones.rend(); ++it ) {
        const auto &zone = *it;
        if( zone.get_faction() != fac || !is_in_current_dimension( zone ) ) {
            continue;
        }

        if( zone.has_inside( where ) ) {
            return &zone;
        }
    }
    auto vzones = get_map().get_vehicle_zones( g->get_levz() );
    for( auto it = vzones.rbegin(); it != vzones.rend(); ++it ) {
        const auto zone = *it;
        if( zone->get_faction() != fac ) {
            continue;
        }

        if( zone->has_inside( where ) ) {
            return zone;
        }
    }

    return nullptr;
}

// CAREFUL: This function has the ability to move the passed in zone reference depending on
// which constructor of the key-value pair we use which depends on new_zone being an rvalue or lvalue and constness.
// If you are passing new_zone from a non-const iterator, be prepared for a move! This
// may break some iterators like map iterators if you are less specific!
void zone_manager::create_vehicle_loot_zone( vehicle &vehicle, tripoint_mnt_veh mount_point,
        zone_data &new_zone )
{
    //create a vehicle loot zone
    new_zone.set_is_vehicle( true );
    auto nz = vehicle.loot_zones.emplace( mount_point, new_zone );
    get_map().register_vehicle_zone( &vehicle, g->get_levz() );
    vehicle.zones_dirty = false;
    added_vzones.push_back( &nz->second );
    cache_vzones();
}

namespace
{
struct deferred_zone {
    std::string         name;
    zone_type_id        type;
    faction_id          fac;
    bool                invert;
    bool                enabled;
    tripoint_abs_ms     start;
    tripoint_abs_ms     end;
};
std::mutex                 g_deferred_zones_mutex;
std::vector<deferred_zone> g_deferred_zones;
} // namespace

void defer_zone_add( const std::string &name, const zone_type_id &type,
                     const faction_id &fac, bool invert, bool enabled,
                     const tripoint_abs_ms &start, const tripoint_abs_ms &end )
{
    auto lock = std::lock_guard( g_deferred_zones_mutex );
    g_deferred_zones.push_back( { name, type, fac, invert, enabled, start, end } );
}

void flush_deferred_zones()
{
    std::vector<deferred_zone> pending;
    {
        auto lock = std::lock_guard( g_deferred_zones_mutex );
        pending.swap( g_deferred_zones );
    }
    auto &mgr = zone_manager::get_manager();
    std::ranges::for_each( pending, [&]( const deferred_zone & z ) {
        mgr.add( z.name, z.type, z.fac, z.invert, z.enabled, z.start, z.end );
    } );
}

void zone_manager::add( const std::string &name, const zone_type_id &type, const faction_id &fac,
                        const bool invert, const bool enabled, const tripoint_abs_ms &start,
                        const tripoint_abs_ms &end, shared_ptr_fast<zone_options> options )
{
    zone_data new_zone = zone_data( name, type, fac, invert, enabled, start, end,
                                    std::move( options ) );
    //the start is a vehicle tile with cargo space
    map &here = get_map();
    new_zone.set_dimension( here.get_bound_dimension() );
    if( const std::optional<vpart_reference> vp = here.veh_at( abs_to_bub(
            start ) ).part_with_feature( "CARGO", false ) ) {
        // TODO:Allow for loot zones on vehicles to be larger than 1x1
        if( start == end && query_yn( _( "Bind this zone to the cargo part here?" ) ) ) {
            // TODO: refactor zone options for proper validation code
            if( type == zone_FARM_PLOT || type == zone_CONSTRUCTION_BLUEPRINT ) {
                popup( _( "You cannot add that type of zone to a vehicle." ), PF_NONE );
                return;
            }

            create_vehicle_loot_zone( vp->vehicle(), vp->mount(), new_zone );
            return;
        }
    }

    //Create a regular zone
    zones.push_back( new_zone );
    cache_data();
}

bool zone_manager::remove( zone_data &zone )
{
    for( auto it = zones.begin(); it != zones.end(); ++it ) {
        if( &zone == &*it ) {
            zones.erase( it );
            return true;
        }
    }
    zone_data old_zone = zone_data( zone );
    //If the zone was previously edited this session
    //Move original data out of changed
    for( auto it = changed_vzones.begin(); it != changed_vzones.end(); ++it ) {
        if( it->second == &zone ) {
            old_zone = zone_data( it->first );
            changed_vzones.erase( it );
            break;
        }
    }
    bool added = false;
    //If the zone was added this session
    //remove from added, and don't add to removed
    for( auto it = added_vzones.begin(); it != added_vzones.end(); ++it ) {
        if( *it == &zone ) {
            added = true;
            added_vzones.erase( it );
            break;
        }
    }
    if( !added ) {
        removed_vzones.push_back( old_zone );
    }

    if( !get_map().deregister_vehicle_zone( zone ) ) {
        debugmsg( "Tried to remove a zone from an unloaded vehicle" );
        return false;
    }
    cache_vzones();
    return true;
}

auto zone_manager::remove_dimension_zones( const dimension_id &dim_id ) -> bool
{
    if( dim_id.is_empty() ) {
    return false;
}

namespace ranges = std::ranges;
    using namespace std::views;
        auto remaining = zones |
        filter( [&]( const auto & zone ) { return zone.get_dimension() != dim_id; } ) |
    ranges::to<std::vector>();
    if( remaining.size() == zones.size() ) {
    return true;
}

auto restore = on_out_of_scope( [&]() { zones.swap( remaining ); } );
    zones.swap( remaining );
    if( !save_zones() ) {
    return false;
}
restore.cancel();
cache_data();
return true;
}

void zone_manager::swap( zone_data &a, zone_data &b )
{
    if( a.get_is_vehicle() || b.get_is_vehicle() ) {
        //Current swap mechanic will change which vehicle the zone is on
        // TODO: track and update vehicle zone priorities?
        popup( _( "You cannot change the order of vehicle loot zones." ), PF_NONE );
        return;
    }
    std::swap( a, b );
}

void zone_manager::rotate_zones( map &target_map, const int turns )
{
    if( turns == 0 ) {
        return;
    }
    const auto origin = project_to<coords::ms>( target_map.get_abs_sub() );
    const auto a_start = map_local_to_abs( target_map, tripoint_bub_ms( 0, 0, 0 ) );
    const auto a_end = map_local_to_abs( target_map, tripoint_bub_ms( 23, 23, 0 ) );
    const point dim( 24, 24 );
    for( zone_data &zone : zones ) {
        if( zone.get_dimension() != target_map.get_bound_dimension() ) {
            continue;
        }
        const auto z_start = zone.get_start_point();
        const auto z_end = zone.get_end_point();
        if( ( a_start.x() <= z_start.x() && a_start.y() <= z_start.y() ) &&
            ( a_end.x() > z_start.x() && a_end.y() >= z_start.y() ) &&
            ( a_start.x() <= z_end.x() && a_start.y() <= z_end.y() ) &&
            ( a_end.x() >= z_end.x() && a_end.y() >= z_end.y() ) &&
            ( a_start.z() == z_start.z() )
          ) {
            const auto z_l_start3 = abs_to_map_local( target_map, z_start );
            const auto z_l_end3 = abs_to_map_local( target_map, z_end );
            // don't rotate centered squares
            if( z_l_start3.x() == z_l_start3.y() && z_l_end3.x() == z_l_end3.y() &&
                z_l_start3.x() + z_l_end3.x() == 23 ) {
                continue;
            }
            auto z_l_start = z_l_start3.xy().rotate( turns, dim );
            auto z_l_end = z_l_end3.xy().rotate( turns, dim );
            auto new_z_start = origin + z_l_start.reinterpret_as<point_rel_ms>();
            auto new_z_end = origin + z_l_end.reinterpret_as<point_rel_ms>();
            auto first = tripoint_abs_ms( std::min( new_z_start.x(), new_z_end.x() ),
                                          std::min( new_z_start.y(), new_z_end.y() ), a_start.z() );
            auto second = tripoint_abs_ms( std::max( new_z_start.x(), new_z_end.x() ),
                                           std::max( new_z_start.y(), new_z_end.y() ), a_end.z() );
            zone.set_position( std::make_pair( first, second ), false );
        }
    }
}

std::vector<zone_manager::ref_zone_data> zone_manager::get_zones( const faction_id &fac )
{
    auto zones = std::vector<ref_zone_data>();

    for( auto &zone : this->zones ) {
        if( is_in_current_dimension( zone ) && zone.get_faction() == fac ) {
            zones.emplace_back( zone );
        }
    }

    auto vzones = get_map().get_vehicle_zones( g->get_levz() );

    for( auto zone : vzones ) {
        if( zone->get_faction() == fac ) {
            zones.emplace_back( *zone );
        }
    }

    return zones;
}

std::vector<zone_manager::ref_const_zone_data> zone_manager::get_zones(
    const faction_id &fac ) const
{
    auto zones = std::vector<ref_const_zone_data>();

    for( auto &zone : this->zones ) {
        if( is_in_current_dimension( zone ) && zone.get_faction() == fac ) {
            zones.emplace_back( zone );
        }
    }

    auto vzones = get_map().get_vehicle_zones( g->get_levz() );

    for( auto zone : vzones ) {
        if( zone->get_faction() == fac ) {
            zones.emplace_back( *zone );
        }
    }

    return zones;
}

void zone_manager::serialize( JsonOut &json ) const
{
    json.write( zones );
}

void zone_manager::deserialize( JsonIn &jsin )
{
    jsin.read( zones );
    zones.erase( std::remove_if( zones.begin(), zones.end(),
    [this]( const zone_data & it ) -> bool {
        const zone_type_id zone_type = it.get_type();
        const bool is_valid = has_type( zone_type );

        if( !is_valid ) debugmsg( "Invalid zone type: %s", zone_type.c_str() );

        return !is_valid;
    } ),
    zones.end() );
}

void zone_data::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "name", name );
    json.member( "type", type );
    json.member( "faction", faction );
    json.member( "invert", invert );
    json.member( "enabled", enabled );
    json.member( "is_vehicle", is_vehicle );
    json.member( "dimension", dim_id.str() );
    json.member( "start", start );
    json.member( "end", end );
    get_options().serialize( json );
    json.end_object();
}

void zone_data::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.allow_omitted_members();
    data.read( "name", name );
    data.read( "type", type );
    if( data.has_member( "faction" ) ) {
        data.read( "faction", faction );
    } else {
        faction = your_fac;
    }
    data.read( "invert", invert );
    data.read( "enabled", enabled );
    auto raw_dim_id = std::string{};
    data.read( "dimension", raw_dim_id );
    dim_id = dimension_id( raw_dim_id );
    //Legacy support
    if( data.has_member( "is_vehicle" ) ) {
        data.read( "is_vehicle", is_vehicle );
    } else {
        is_vehicle = false;
    }
    //Legacy support
    if( data.has_member( "start_x" ) ) {
        tripoint_abs_ms s;
        tripoint_abs_ms e;
        data.read( "start_x", s.x() );
        data.read( "start_y", s.y() );
        data.read( "start_z", s.z() );
        data.read( "end_x", e.x() );
        data.read( "end_y", e.y() );
        data.read( "end_z", e.z() );
        start = s;
        end = e;
    } else {
        data.read( "start", start );
        data.read( "end", end );
    }
    auto new_options = zone_options::create( type );
    new_options->deserialize( data );
    options = new_options;
}

bool zone_manager::save_zones()
{
    const auto saved = g->get_active_world()->write_to_player_file( ".zones.json",
    [&]( std::ostream & fout ) {
        auto jsout = JsonOut( fout );
        serialize( jsout );
    }, _( "zones date" ) );
    if( !saved ) {
        return false;
    }
    added_vzones.clear();
    changed_vzones.clear();
    removed_vzones.clear();
    return true;
}

void zone_manager::load_zones()
{
    g->get_active_world()->read_from_player_file( ".zones.json", [&]( std::istream & fin ) {
        JsonIn jsin( fin );
        deserialize( jsin );
    }, true );
    revert_vzones();
    added_vzones.clear();
    changed_vzones.clear();
    removed_vzones.clear();

    cache_data();
    cache_vzones();
}

void zone_manager::zone_edited( zone_data &zone )
{
    if( zone.get_is_vehicle() ) {
        //Check if this zone has already been stored
        for( auto &changed_vzone : changed_vzones ) {
            if( &zone == changed_vzone.second ) {
                return;
            }
        }
        //Add it to the list of changed zones
        changed_vzones.emplace_back( zone_data( zone ), &zone );
    }
}

void zone_manager::revert_vzones()
{
    map &here = get_map();
    for( auto zone : removed_vzones ) {
        //Code is copied from add() to avoid yn query
        if( const std::optional<vpart_reference> vp = here.veh_at( abs_to_bub(
                zone.get_start_point() ) ).part_with_feature( "CARGO", false ) ) {
            zone.set_is_vehicle( true );
            vp->vehicle().loot_zones.emplace( vp->mount(), zone );
            vp->vehicle().zones_dirty = false;
            here.register_vehicle_zone( &vp->vehicle(), g->get_levz() );
            cache_vzones();
        }
    }
    for( const auto &zpair : changed_vzones ) {
        *( zpair.second ) = zpair.first;
    }
    for( auto zone : added_vzones ) {
        remove( *zone );
    }
}
