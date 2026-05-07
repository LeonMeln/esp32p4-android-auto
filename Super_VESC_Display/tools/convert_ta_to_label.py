#!/usr/bin/env python3
"""
One-shot conversion: every `type: "ta"` widget in Super_VESC_Display.guiguider
becomes `type: "label"`. GUI Guider then generates lv_label_create() instead of
lv_textarea_create() on Generate Code, which removes the need for the
post-processing patch (no blue cursor, no CLICKABLE/SCROLLABLE flags to clear).

Run once after pulling, then keep editing the project in GUI Guider as usual.
Idempotent: a second run is a no-op.

    cd Super_VESC_Display
    python3 tools/convert_ta_to_label.py

The label format produced here matches what GUI Guider 1.10.1 itself emits
when you add a label via the GUI: top-level fields `is_static`,
`custom_attribute`, `attribute_type`, and a `style` array with TWO entries
(LV_STATE_DEFAULT and LV_STATE_DISABLED). Without all of these, GUI Guider
silently fails to render the widget on the canvas.
"""

from __future__ import annotations

import copy
import json
import os
import shutil
import sys
from typing import Any

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(ROOT, "Super_VESC_Display.guiguider")
BACKUP = PROJECT + ".pre-label.bak"

# textarea-only fields — labels don't accept any of these
TA_FIELDS = (
    "password_mode",
    "one_line_mode",
    "placeholder",
    "accept_characters",
    "max_len",
    "password_bullet",
)

# Style fields a GUI-Guider-emitted label always carries. Used to fill gaps
# in TA styles that didn't have these set explicitly.
LABEL_STYLE_DEFAULTS = {
    "border_side": ["LV_BORDER_SIDE_FULL"],
    "border_width": 0,
    "border_opa": 255,
    "border_color": "#2195f6",
    "radius": 0,
    "text_color": "#000000",
    "text_opa": 255,
    "font": 16,
    "text_align": "LV_TEXT_ALIGN_LEFT",
    "font_family": "montserratMedium",
    "letter_space": 0,
    "line_space": 0,
    "bg_color": "#2195f6",
    "bg_grad_color": "#2195f6",
    "bg_opa": 255,
    "bg_grad_dir": "LV_GRAD_DIR_NONE",
    "bg_dither_mode": "LV_DITHER_NONE",
    "bg_main_stop": 0,
    "bg_grad_stop": 255,
    "padding_top": 0,
    "padding_right": 0,
    "padding_bottom": 0,
    "padding_left": 0,
    "bg_img_src": "",
    "bg_img_opa": 255,
    "bg_img_recolor": "#ffffff",
    "bg_img_recolor_opa": 0,
    "shadow_color": "#2195f6",
    "shadow_opa": 255,
    "shadow_width": 0,
    "shadow_spread": 0,
    "shadow_ofs_x": 0,
    "shadow_ofs_y": 0,
}


def make_default_style(src: dict | None) -> dict:
    """LV_PART_MAIN / LV_STATE_DEFAULT, merging the source TA style on top of
    the canonical label defaults. `src` is the original TA's MAIN/DEFAULT
    entry (may be None — then we use defaults only)."""
    s = dict(LABEL_STYLE_DEFAULTS)
    if src:
        for k, v in src.items():
            if k in ("part", "state"):
                continue
            s[k] = v
    s["part"] = "LV_PART_MAIN"
    s["state"] = "LV_STATE_DEFAULT"
    s["disable"] = False
    return s


def make_disabled_style(default_style: dict) -> dict:
    """LV_PART_MAIN / LV_STATE_DISABLED — copy of DEFAULT with disable:true
    and bg_opa:0 (matches what GUI Guider emits)."""
    s = copy.deepcopy(default_style)
    s["state"] = "LV_STATE_DISABLED"
    s["disable"] = True
    s["bg_opa"] = 0
    return s


def convert_widget(w: dict) -> bool:
    """Mutates w in place. Returns True if it was a TA and got converted."""
    if w.get("type") != "ta":
        return False
    w["type"] = "label"
    for k in TA_FIELDS:
        w.pop(k, None)
    # New top-level fields GUI Guider expects on labels
    w.setdefault("long_mode", "LV_LABEL_LONG_WRAP")
    w["is_static"] = False
    w["custom_attribute"] = []
    w["attribute_type"] = ""

    # Pick the source MAIN/DEFAULT style from the TA (it's the only one with
    # actual font/colour info). Drop everything else (SCROLLBAR, CURSOR,
    # FOCUSED — these don't apply to labels).
    src_default: dict | None = None
    for s in w.get("style", []):
        if (isinstance(s, dict)
                and s.get("part") == "LV_PART_MAIN"
                and s.get("state") == "LV_STATE_DEFAULT"):
            src_default = s
            break

    default_style = make_default_style(src_default)
    disabled_style = make_disabled_style(default_style)
    w["style"] = [default_style, disabled_style]
    return True


def walk(node: Any, n: list[int]) -> None:
    if isinstance(node, dict):
        if convert_widget(node):
            n[0] += 1
        for v in node.values():
            walk(v, n)
    elif isinstance(node, list):
        for x in node:
            walk(x, n)


def main() -> int:
    if not os.path.isfile(PROJECT):
        print(f"[err] {PROJECT} not found", file=sys.stderr)
        return 1
    with open(PROJECT, "r", encoding="utf-8") as f:
        proj = json.load(f)

    ta_count = [0]
    def count(x: Any) -> None:
        if isinstance(x, dict):
            if x.get("type") == "ta":
                ta_count[0] += 1
            for v in x.values():
                count(v)
        elif isinstance(x, list):
            for e in x:
                count(e)
    count(proj)
    if ta_count[0] == 0:
        print("[i] no `ta` widgets — already converted, nothing to do")
        return 0

    if not os.path.isfile(BACKUP):
        shutil.copy2(PROJECT, BACKUP)
        print(f"[i] backup -> {BACKUP}")
    else:
        print(f"[i] backup already exists: {BACKUP}")

    converted = [0]
    walk(proj, converted)

    with open(PROJECT, "w", encoding="utf-8") as f:
        json.dump(proj, f, ensure_ascii=False, indent="\t")
        f.write("\n")
    print(f"[ok] converted {converted[0]} ta -> label")
    print(f"[ok] {PROJECT}")
    print()
    print("next:")
    print("  1. open project in GUI Guider, hit Generate Code")
    print("  2. rebuild simulator: cd lvgl-simulator && make")
    return 0


if __name__ == "__main__":
    sys.exit(main())
