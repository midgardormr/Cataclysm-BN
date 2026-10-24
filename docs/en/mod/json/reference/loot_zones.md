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
on the same rectangle. Native functions without extra options can be checked alongside storage;
functions requiring a seed or construction project retain individual setup. The manual custom filter remains a separate
text-only option using the original item filter syntax.

Custom options save `superzone` (selection mode), `categories` (stable zone IDs),
and `mark` (optional additional filter, combined with categories using AND).
Legacy custom zones without category fields keep their text-filter behavior.
The ordered groups are defined in `loot_zone_presets()` in `clzones.cpp`.
Unknown storage types are retained as individual entries.

Sorting checks explicit text filters, available specialized zones, category
storage, then dump zones. Full or blocked specialized storage allows a general
fallback. Pathfinding and NPC hauling behavior are unchanged.

The details panel shows selected category names directly under "Zone type:".
Enabled area rules on the same rectangle are listed separately after a blank
line. Manual custom zones show their type followed by a blank line and the raw
filter. Comma-separated conditions move intact to the next line whenever they
fit the panel width. Conditions wider than the entire panel must still wrap.
Details grow upwards, retaining the bottom shortcuts and at least one list row;
PgUp/PgDn scroll content that exceeds the available height.

The prototype's Russian catalogue is `lang/zone_ui/ru.po`, compiled to `ru.mo`
and loaded alongside the base translation catalogue.

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
come first, activity groups next, rules last. Within each section `sort_order`
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
storage appears after configured storage groups, sorted by translated name.
All other native types remain accessible through the standard type picker.
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
