# Symbols Nerd Font: zone icons

The unmodified SymbolsNerdFont-Regular.ttf is bundled from Nerd Fonts 3.5.1.
Upstream revision: `f034eeafc0ccf79afa8dd952feb86b18e5d0469e`.
Source: https://github.com/ryanoasis/nerd-fonts/blob/f034eeafc0ccf79afa8dd952feb86b18e5d0469e/patched-fonts/NerdFontsSymbolsOnly/SymbolsNerdFont-Regular.ttf
SHA-256: `2839f0a572d4559f3f17a6fb74b8772e183f0c0a47150998ab194932cad55829`.

The upstream distribution license is preserved in SymbolsNerdFont-LICENSE.txt.
SymbolsNerdFont-UPSTREAM.md preserves the upstream component attribution/license table;
additional upstream license texts are in SymbolsNerdFont-Licenses/.
All 18 selected icons are from the Material Design set (Pictogrammers), identified
by the `md-` names below. Its upstream license text is included as materialdesign.txt.
The full upstream font is retained, without subsetting, renaming or changing glyphs.

## Integration

- data/json/loot_zones.json uses the original Nerd Fonts Unicode code points.
- font_loader.cpp prepends the font resolved through PATH_INFO::fontdir(), also
  for existing user font settings. Text falls through to the configured fonts.
- zone_icons.h records category order independently of Unicode numeric order.
- wcwidth.cpp reserves two cells for exactly these 18 selected symbols; no entire
  private-use block is changed. No split-half glyphs or custom font are used.
- Existing core data/font packaging includes the font, notices and licenses in
  macOS resources and Android data assets. An Android device run is still required.
- Existing saved names are literal strings. Old prototype icon prefixes need
  migration in test saves; arbitrary user names must not be rewritten silently.
- No changes to the separate translation catalogue packaging are included here.
- External terminal renderers require a compatible font and cell-width handling;
  the bundled font integration applies to SDL graphics builds.

## Selected categories

| Category | Nerd Fonts name | Unicode |
|---|---|---|
| Сортировка | `md-swap_horizontal` | U+F04E1 |
| Еда и напитки | `md-food_apple` | U+F025B |
| Оружие | `md-sword_cross` | U+F0787 |
| Инструменты | `md-tools` | U+F1064 |
| Быт и вещи | `md-tshirt_crew` | U+F0A7B |
| Медицина и улучшения | `md-plus_thick` | U+F11EC |
| Книги и карты | `md-book_open_variant` | U+F14F7 |
| Особые предметы | `md-star_four_points` | U+F0AE2 |
| Техника | `md-cog` | U+F0493 |
| Сырьё и припасы | `md-leaf` | U+F032A |
| Материалы | `md-layers_triple` | U+F0F58 |
| Останки | `md-skull` | U+F068C |
| Работы | `md-shovel` | U+F0710 |
| Строительство | `md-hammer` | U+F08EA |
| Транспорт | `md-car` | U+F010B |
| Автопитание и огонь | `md-autorenew` | U+F006A |
| Подбор предметов | `md-cancel` | U+F073A |
| Поведение НПС | `md-account` | U+F0004 |
