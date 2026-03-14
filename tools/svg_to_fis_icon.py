#!/usr/bin/env python3
"""
Convert an SVG file to a C array in the format expected by firmware/fis_nav_icons.h.

Output format:
  - 64x64 pixels, 1-bit (black = 1, white = 0)
  - Row-major order: first 8 bytes = row 0 (64 pixels), etc.
  - MSB first: pixel (0,0) = bit 7 of byte 0; pixel (7,0) = bit 0 of byte 0
  - Total size: 64 * 8 = 512 bytes

Usage:
  python3 svg_to_fis_icon.py icon.svg
  python3 svg_to_fis_icon.py icon.svg -o output.c
  python3 svg_to_fis_icon.py icon.svg --name MY_ICON_BK

The SVG should have a transparent background (no opaque background). The script
renders with a white background so that transparent areas become 0 and black
icon shape becomes 1 in the 1-bit output.

The C symbol is derived from the SVG basename (e.g. my_icon_bk.svg -> ICON_MY_ICON_BK)
unless overridden with --name. You must manually add the array and a FIS_ICON_TABLE
entry to firmware/fis_nav_icons.h (see nav-icons/README.md).
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

# Target dimensions; must match FIS display icon size in firmware.
WIDTH = 64
HEIGHT = 64
BYTES_PER_ROW = WIDTH // 8
BYTES_TOTAL = BYTES_PER_ROW * HEIGHT  # 512


def _svg_to_png_cairosvg(svg_path: Path, png_path: Path) -> bool:
    """Render SVG to PNG using cairosvg. Returns True on success."""
    try:
        import cairosvg
        cairosvg.svg2png(
            url=str(svg_path),
            write_to=str(png_path),
            output_width=WIDTH,
            output_height=HEIGHT,
            background_color="white",
        )
        return True
    except Exception:
        return False


def _svg_to_png_rsvg(svg_path: Path, png_path: Path) -> bool:
    """Render SVG to PNG using rsvg-convert (librsvg). Returns True on success."""
    try:
        subprocess.run(
            [
                "rsvg-convert", "-w", str(WIDTH), "-h", str(HEIGHT),
                "--background-color", "white", "-o", str(png_path), str(svg_path),
            ],
            check=True,
            capture_output=True,
        )
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def _svg_to_png_convert(svg_path: Path, png_path: Path) -> bool:
    """Render SVG to PNG using ImageMagick convert. Returns True on success."""
    try:
        subprocess.run(
            ["convert", "-background", "white", "-resize", f"{WIDTH}x{HEIGHT}", str(svg_path), str(png_path)],
            check=True,
            capture_output=True,
        )
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def svg_to_png(svg_path: Path, png_path: Path) -> bool:
    """Render SVG to PNG at WIDTH x HEIGHT. Tries cairosvg, then rsvg-convert, then ImageMagick."""
    if _svg_to_png_cairosvg(svg_path, png_path):
        return True
    if _svg_to_png_rsvg(svg_path, png_path):
        return True
    if _svg_to_png_convert(svg_path, png_path):
        return True
    return False


def png_to_1bit_bytes(png_path: Path) -> list[int]:
    """
    Load PNG, convert to 1-bit (threshold: luminance >= 128 -> 0, else 1).
    Return list of bytes in row-major, MSB-first order (512 bytes).
    """
    try:
        from PIL import Image
    except ImportError:
        sys.exit("PIL/Pillow is required: pip install Pillow")

    img = Image.open(png_path).convert("L")  # grayscale
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    pixels = img.load()

    out: list[int] = []
    for y in range(HEIGHT):
        for byte_index in range(BYTES_PER_ROW):
            byte_val = 0
            for bit in range(8):
                x = byte_index * 8 + bit
                # Black (low luminance) -> 1; white -> 0 (FIS display convention).
                if x < WIDTH and pixels[x, y] < 128:
                    byte_val |= 1 << (7 - bit)
            out.append(byte_val)
    return out


def basename_to_c_name(basename: str) -> str:
    """Turn filename stem (e.g. my_icon_bk) into C symbol suffix MY_ICON_BK."""
    s = re.sub(r"[^a-zA-Z0-9_]", "_", basename).strip("_").upper()
    return s if s else "ICON"


def emit_c_array(symbol_suffix: str, bytes_data: list[int], source_comment: str) -> str:
    """Emit C source for the icon array and a comment with FIS_ICON_TABLE line."""
    lines = [
        f"// {source_comment} — {WIDTH}x{HEIGHT}px, {len(bytes_data)} bytes",
        f"static const uint8_t ICON_{symbol_suffix}[] PROGMEM = {{",
    ]
    for i in range(0, len(bytes_data), 16):
        chunk = bytes_data[i : i + 16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"  {hex_str},")
    lines.append("};")
    # Table entry for copy-paste into FIS_ICON_TABLE (name is lowercase with underscores).
    table_name = symbol_suffix.lower()
    lines.append("")
    lines.append(f"// FIS_ICON_TABLE entry: {{\"{table_name}\", ICON_{symbol_suffix}, {WIDTH}, {HEIGHT}}},")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Convert SVG to FIS icon C array (64x64 1-bit, row-major MSB first)."
    )
    ap.add_argument("svg", type=Path, help="Input SVG file")
    ap.add_argument("-o", "--output", type=Path, help="Output .c or .h file (default: stdout)")
    ap.add_argument("--name", type=str, help="C symbol suffix (default: from SVG basename)")
    args = ap.parse_args()

    if not args.svg.exists():
        sys.exit(f"File not found: {args.svg}")

    png_path = args.svg.with_suffix(".png")
    if not svg_to_png(args.svg, png_path):
        sys.exit(
            "Could not render SVG. Install one of: cairosvg (pip install cairosvg), "
            "librsvg (rsvg-convert), or ImageMagick (convert)."
        )

    bytes_data = png_to_1bit_bytes(png_path)
    png_path.unlink(missing_ok=True)

    symbol_suffix = args.name if args.name else basename_to_c_name(args.svg.stem)
    source_comment = args.svg.name
    c_source = emit_c_array(symbol_suffix, bytes_data, source_comment)

    if args.output:
        args.output.write_text(c_source, encoding="utf-8")
        print(f"Wrote {args.output}", file=sys.stderr)
    else:
        print(c_source)


if __name__ == "__main__":
    main()
