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
