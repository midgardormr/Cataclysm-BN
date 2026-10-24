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
on the same rectangle. Construction, farming, vehicle and other activity zones
retain their individual setup flow. The manual custom filter remains a separate
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
