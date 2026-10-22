---
title: Loot zones
---

`LOOT_ZONE` definitions describe zone types. Their `name` is displayed in the
type picker and in the selected zone's details panel. A zone instance has its
own editable name.

The optional `default_name` string supplies the translated name suggested when
creating a zone, independently of its displayed type:

```json
{
  "type": "LOOT_ZONE",
  "id": "LOOT_FOOD",
  "name": "Food storage",
  "default_name": "Pantry",
  "description": "Destination for comestibles.",
  "color": "green"
}
```

When omitted or empty, the translated type name is used, including any icon
prefix. Existing zone names are preserved. Construction blueprint and seed suggestions retain precedence. Custom loot
filters use the type default name; the filter expression appears only in details.

The optional `icon` is a non-translatable glyph, drawn before the localized
name in the type picker, selected type details, and default suggested name.
`name` accepts a translation object with `str` and `ctxt`. Built-in zone glyphs
use the bundled Symbols Nerd Font and occupy two terminal cells. See
`data/font/SymbolsNerdFont-NOTICE.md` for the selected code points and provenance.

The prototype's Russian zone-name catalogue is in `lang/zone_ui/ru.po`.
Compile it with `msgfmt -o lang/zone_ui/ru.mo lang/zone_ui/ru.po`; it is loaded
alongside the base translation catalogue.


## Superzone templates (prototype)

The creation picker inserts cyan, icon-free templates before the food, weapon,
tool and material groups. Existing ordinary types remain available. Templates
create `LOOT_CUSTOM`, not new zone IDs. Creation and category editing use a
checkbox list. At least one category is required; cancelling restores the prior
selection. An optional additional text filter is combined with categories using
AND. Selected categories use stable zone IDs and do not depend on translated
labels; food and drinks are split into perishable/non-perishable groups.

Custom options save `superzone` (template ID), `categories` (selected category
zone IDs), and the existing `mark` (additional filter). Legacy custom zones
without these fields keep their text-filter behavior. Templates currently live
in `loot_zone_presets()` in `clzones.cpp`.

Sorting checks existing explicit text filters, then available specialized
zones, then a matching superzone, then the dump zone. Destination availability
includes map bounds, item placement permissions, item count and cargo/tile volume.
Full or blocked specialized zones therefore allow a general fallback. Pathfinding
and NPC hauling behavior are not changed by this prototype.
