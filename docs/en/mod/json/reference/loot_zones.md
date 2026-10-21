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

When omitted or empty, the translated type name is used, with any leading
Unicode private-use glyphs and their following spaces removed. This supports
mods that prefix their type names with custom icon fonts. Existing zone names
are preserved. Zone-specific suggestions, such as a construction blueprint or
a custom loot filter, retain precedence.
