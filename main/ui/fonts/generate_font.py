#!/usr/bin/env python3
"""Generate subset LVGL C fonts: Montserrat Latin + SimHei CJK."""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CHARSET = os.path.join(HERE, "charset.txt")
MONTSERRAT = os.path.join(
    ROOT, "managed_components", "lvgl__lvgl", "scripts", "built_in_font", "Montserrat-Medium.ttf"
)
SIMHEI = r"C:\Windows\Fonts\simhei.ttf"


def unique_symbols():
    raw = open(CHARSET, encoding="utf-8").read()
    seen = set()
    out = []
    for ch in raw:
        if ch.isspace():
            continue
        if "\u4e00" <= ch <= "\u9fff" or ch in "（）【】、。：；？！—…℃°":
            if ch not in seen:
                seen.add(ch)
                out.append(ch)
    return "".join(out)


def gen(size, name):
    symbols = unique_symbols()
    out_c = os.path.join(HERE, f"{name}.c")
    cmd = [
        "npx", "--yes", "lv_font_conv",
        "--font", MONTSERRAT, "-r", "0x20-0x7F,0xB0",
        "--font", SIMHEI, "--symbols", symbols,
        "--size", str(size),
        "--bpp", "4",
        "--format", "lvgl",
        "--lv-include", "lvgl.h",
        "--lv-font-name", name,
        "--no-compress",
        "--no-kerning",
        "-o", out_c,
    ]
    print(f"Generating {name} size={size} glyphs={len(symbols)} CJK/punct")
    subprocess.check_call(cmd, cwd=HERE, shell=(os.name == "nt"))
    print(f"  wrote {out_c} ({os.path.getsize(out_c)} bytes)")


def main():
    if not os.path.isfile(MONTSERRAT):
        sys.exit(f"missing {MONTSERRAT}")
    if not os.path.isfile(SIMHEI):
        sys.exit(f"missing {SIMHEI}")
    gen(22, "font_cn_22")
    gen(28, "font_cn_28")


if __name__ == "__main__":
    main()
