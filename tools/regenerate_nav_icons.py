#!/usr/bin/env python3
"""
Regenerate all icon C arrays in firmware/fis_nav_icons.h from nav-icons/*.svg.

Use when the header has placeholder (all-zero) arrays: the arrays were declared
and FIS_ICON_TABLE was filled, but the output of svg_to_fis_icon.py was never
pasted. This script runs the converter for each icon in table order and
replaces the icon data section in the header.

Run from project root:
  python3 tools/regenerate_nav_icons.py

Requires: svg_to_fis_icon.py (and its deps: Pillow, cairosvg or rsvg/ImageMagick).
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
NAV_ICONS_DIR = PROJECT_ROOT / "nav-icons"
HEADER_PATH = PROJECT_ROOT / "firmware" / "fis_nav_icons.h"
CONVERTER = SCRIPT_DIR / "svg_to_fis_icon.py"


def extract_icon_names_from_header(content: str) -> list[str]:
    """Parse FIS_ICON_TABLE and return icon names in order (excluding sentinel)."""
    # Match lines like:   {"nav_destination_bk", ICON_NAV_DESTINATION_BK, 64, 64},
    names: list[str] = []
    for m in re.finditer(r'\{\s*"([^"]+)"\s*,', content):
        name = m.group(1)
        if name == "NULL":
            break
        names.append(name)
    return names


def run_converter(svg_path: Path) -> str:
    """Run svg_to_fis_icon.py on svg_path; return stdout (C array block)."""
    result = subprocess.run(
        [sys.executable, str(CONVERTER), str(svg_path)],
        cwd=str(PROJECT_ROOT),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Converter failed for {svg_path}: {result.stderr or result.stdout}")
    return result.stdout


def extract_array_block(stdout: str) -> str:
    """From converter output take only the comment + array (exclude FIS_ICON_TABLE line)."""
    lines = stdout.strip().split("\n")
    block_lines: list[str] = []
    for line in lines:
        if line.strip().startswith("// FIS_ICON_TABLE entry:"):
            break
        block_lines.append(line)
    return "\n".join(block_lines).strip()


def main() -> None:
    if not HEADER_PATH.exists():
        sys.exit(f"Header not found: {HEADER_PATH}")
    if not CONVERTER.exists():
        sys.exit(f"Converter not found: {CONVERTER}")

    content = HEADER_PATH.read_text(encoding="utf-8")
    names = extract_icon_names_from_header(content)
    if not names:
        sys.exit("Could not parse any icon names from FIS_ICON_TABLE.")

    # Build new icon section by running converter for each SVG
    blocks: list[str] = []
    for name in names:
        svg_path = NAV_ICONS_DIR / f"{name}.svg"
        if not svg_path.exists():
            sys.exit(f"SVG not found for table name '{name}': {svg_path}")
        raw = run_converter(svg_path)
        blocks.append(extract_array_block(raw))

    # Replace the icon arrays section: from first icon comment to line before "/* Lookup:"
    icon_section = "\n\n".join(blocks)

    start_m = re.search(r"\n// [a-z_]+_bk\.svg", content)
    end_m = re.search(r"\n/\* Lookup:", content)
    if not start_m or not end_m or start_m.start() >= end_m.start():
        sys.exit("Could not find icon section boundaries in header.")
    # From the newline before first icon comment through the newline before "/* Lookup:"
    new_content = (
        content[: start_m.start()] + "\n" + icon_section + "\n\n" + content[end_m.start() :]
    )

    HEADER_PATH.write_text(new_content, encoding="utf-8")
    print(f"Regenerated {len(blocks)} icon arrays in {HEADER_PATH}", file=sys.stderr)


if __name__ == "__main__":
    main()
