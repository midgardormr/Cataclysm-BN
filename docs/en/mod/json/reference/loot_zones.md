---
title: Loot zones
---

`LOOT_ZONE` definitions describe zone types. Their translated `name` is used in
the type picker, selected-zone details, and the suggested name for a new zone.
Each zone instance has its own editable name. `name` accepts a translation object
with `str` and `ctxt`.

## Category storage

The creation checklist offers groups and individual storage categories. Selected
categories create one `LOOT_CUSTOM` zone. Area rules create separate native zones
on the same rectangle. Native functions can be checked alongside storage where appropriate. Construction,
tree cutting, mining, fishing and farming open separately. Each created zone asks for a name. The manual custom filter remains a separate
text-only option using the original item filter syntax.

Custom options save `superzone` (selection mode), `categories` (stable zone IDs),
or `mark` (the text filter for manual zones). Category storage cannot add a text filter;
editing it opens the category checklist directly.
Legacy custom zones without category fields keep their text-filter behavior.
The ordered groups are loaded from `data/json/loot_zone_groups.json`.
Unknown storage types are retained as individual entries.

Sorting checks explicit text filters, available specialized zones, category
storage, then dump zones. Among category zones, the smaller category selection
wins; equal selections prefer the current storage location, then the closest
available tile. Only the first reachable destination is used. Full or blocked specialized storage allows a general
fallback. Sorting checks the actor's route to a tile adjacent to the destination,
using the existing movement restrictions. An unreachable specialized zone or
manual filter does not suppress an accessible fallback. Cargo uses its own item
count and volume limits, independently of items on the ground beneath it.
Overlapping custom zones accept the union of their matching filters; an item at
a custom source zone is retained only if that zone accepts it.

The details panel shows selected category names directly under "Zone type:".
Other zones on the same rectangle are not included in its description.
Area rules remain independent zones and apply to their own tiles.
Manual custom zones show their type followed by a blank line and the raw
filter. Comma-separated conditions move intact to the next line whenever they
fit the panel width. Conditions wider than the entire panel must still wrap.
Details grow upwards, retaining the bottom shortcuts and at least one list row;
PgUp/PgDn scroll content that exceeds the available height.

## Configuring the creation checklist

`data/json/loot_zone_groups.json` defines `LOOT_ZONE_GROUP` objects:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "name": "Tools",
  "kind": "storage",
  "sort_order": 20,
  "categories": ["LOOT_WORKSHOP_TOOLS", "LOOT_ENTRY_TOOLS", "LOOT_TOOLS"]
}
```

`id` is stable and untranslated. `name` is an English source string (or a normal
translation object), extracted for translation alongside other JSON names.
Changing the game language changes group and type labels. Instance names already
saved by the player remain literal editable names.

`kind` is `storage` (combined category checkbox), `activity` (native functions, with individual setup when needed), or `rules` (separate native zones on the chosen rectangle). Storage groups
come first, followed by activity groups and then area rules. Within each section `sort_order`
controls the order, with `id` as a deterministic tie breaker. Members follow the
order of `categories`. Duplicate members appear only in their first group;
references to zone types not installed in the current world are skipped.

Mods can add groups or update an existing group by ID. Omitted fields retain the
existing values. `categories` replaces membership; `extend`/`delete` update it
without copying the original definition:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "extend": { "categories": ["MY_MOD_TOOL_STORAGE"] }
}
```

New storage zone types remain selectable even before they are grouped: types
with a `LOOT_` ID, types referenced by an item category's `zone`, and explicitly
configured storage members are discovered from the loaded world data. Ungrouped
storage appears after configured storage groups, sorted by stable zone ID.
Ungrouped native types appear under "Other zone types" and open individually.
The enabled manager never opens the legacy type picker.
A new item category still needs its usual zone mapping for category sorting;
this UI does not invent sorting behavior for categories without a destination.

Suggested names use the selected group names, in checklist order; mixed groups
are joined with commas. Partial selections still use their parent group name.
Unknown/unassigned storage types use their translated type name instead.


The Zone Manager returns to its creation checklist if the manual filter dialog
is canceled; pending checkboxes are preserved and no name/area dialog opens.
Recognized filter prefixes (`c:`, `m:`, `q:`, etc.) are highlighted only for display.
The raw filter remains unchanged. Selected zones use the normal color overlay;
zone option text is never interpreted as a tile ID.

The Interface option **Use zone manager** is enabled by default. Disabling it
restores the standard zone-type picker for new zones without changing saved zones
or sorting behavior. In the checkbox manager, Left and Right both toggle the
highlighted checkbox; Enter on a category confirms the current selection. Service
entries and native types requiring setup retain their normal Enter action. There is no separate confirm row.
Unassigned categories use stable zone IDs for ordering, independent of language.

Selection markers are gray `−` (unselected), green `+` (selected), and yellow `~`
(partially selected group). Descriptions follow the highlighted entry, with current
key bindings shown underneath after a blank line.

Window sizing uses the same automatic `uilist` layout as the original zone picker:
width follows the contents and is capped to the screen; height is capped and the
list scrolls. Control hints use two lines below the selected entry description.
All checklist labels share one left alignment, with a single space after their
selection marker.

Editing an existing zone type also uses the manager when enabled. One existing zone
can become category storage or one native type; extra area rules must be created
as separate zones. Its name, bounds, enabled state, faction and vehicle binding
are retained, and cancellation leaves it unchanged.

The edit menu shows **Edit type** for category/native zones and **Edit filter**
for manual text filters, without duplicate actions. Native setup dialogs, such as
farming and construction options, remain available separately.
