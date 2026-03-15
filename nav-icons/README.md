# Navigation and status icons (FIS display)

This folder holds SVG sources for the 64x64 pixel 1-bit icons used by the FIS bridge firmware. The firmware uses the converted C arrays from `firmware/fis_nav_icons.h`.

## Icon format (firmware)

- **Size:** 64 x 64 pixels.
- **Bit depth:** 1-bit (black = 1, white = 0).
- **Layout:** Row-major; each row is 8 bytes (64 bits); MSB first (first pixel of row = bit 7 of first byte).
- **Total per icon:** 512 bytes.

## Adding a new icon

### 1. Create or add the SVG

- Place a new SVG in this folder (e.g. `nav_my_icon_bk.svg`). [Inkscape](https://inkscape.org/) is a good tool for creating and editing SVG files.
- The image will be scaled to 64x64; design for that size (simple shapes and strokes work best).
- **No background in the SVG:** use a transparent background (no opaque background rect or fill). The converter renders with a white background; transparent areas become white (0) and black strokes/fills become the icon (1). If the SVG has a dark or opaque background, the whole 64x64 will turn black in the 1-bit output.
- Use black strokes/fill for the icon shape.

### 2. Convert SVG to C array

From the project root, run the converter (requires Python 3 and one of: cairosvg, rsvg-convert, or ImageMagick):

```bash
python3 tools/svg_to_fis_icon.py nav-icons/nav_my_icon_bk.svg
```

To write to a file:

```bash
python3 tools/svg_to_fis_icon.py nav-icons/nav_my_icon_bk.svg -o icon_fragment.c
```

The script prints a C array `ICON_NAV_MY_ICON_BK` and a suggested `FIS_ICON_TABLE` line. If you need a different C symbol:

```bash
python3 tools/svg_to_fis_icon.py nav-icons/nav_my_icon_bk.svg --name MY_CUSTOM_NAME
```

**Dependencies:** For SVG rendering the script tries, in order: `cairosvg` (pip install cairosvg), `rsvg-convert` (librsvg), then ImageMagick `convert`. For 1-bit conversion it needs `Pillow` (pip install Pillow).

### 3. Add the array to the firmware

1. Open `firmware/fis_nav_icons.h`.
2. Paste the generated `static const uint8_t ICON_...[] PROGMEM = { ... };` block **before** the `FIS_ICON_TABLE` definition (e.g. before the "Icon lookup table" comment).
3. Add one entry to `FIS_ICON_TABLE`: use the commented line printed by the script, e.g.  
   `{"nav_my_icon_bk", ICON_NAV_MY_ICON_BK, 64, 64},`  
   Insert it **before** the sentinel `{NULL, NULL, 0, 0}`.
4. Increment `FIS_ICON_COUNT` by 1 (at the bottom of the file).

### 4. Using the new icon

- **By index:** Call `fis_display_inject_icon(index)` with the new icon’s index (same order as in `FIS_ICON_TABLE`; the new icon is at `FIS_ICON_COUNT - 1`).
- **By maneuver (navigation):** If the icon should be shown for a specific Navit maneuver, add a case in `fis_maneuver_to_icon_index()` in `firmware/fis_display.c` that maps the maneuver string to this index.

## Regenerating all icon data

If the icon arrays in `firmware/fis_nav_icons.h` are placeholders (512 bytes of zeros each), the conversion step was never applied. From the project root you can regenerate all arrays from the SVGs in this folder in one go:

```bash
python3 tools/regenerate_nav_icons.py
```

This runs `svg_to_fis_icon.py` for each icon in `FIS_ICON_TABLE` order and rewrites the icon data section of the header. The table and `FIS_ICON_COUNT` are left unchanged.

## Existing icons

The repo ships with 38 icons: navigation maneuvers (left, right, roundabout, exit, merge, etc.) and status (routing, recalculating, no_route, etc.). Their names and order are defined in `FIS_ICON_TABLE` in `firmware/fis_nav_icons.h`.
