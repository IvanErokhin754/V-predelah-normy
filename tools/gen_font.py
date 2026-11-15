#!/usr/bin/env python3
"""Запекает пиксельный моноширинный шрифт с кириллицей в C++-таблицу.

Источник глифов — GNU Unifont (8x16, лицензии SIL OFL 1.1 / GPLv2+ с исключением
для встраивания шрифта). Результат — src/engine/font_data.inc: отсортированный
список кодов символов и по 16 байт на глиф (байт на строку, старший бит слева).

Запуск нужен только если хочется поменять набор символов или сам шрифт:

    python3 tools/gen_font.py [путь к unifont.otf]

Требуется Pillow (pip install pillow). Игре во время сборки и запуска Python не нужен:
готовый font_data.inc уже лежит в репозитории.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

DEFAULT_FONT = "/usr/share/fonts/opentype/unifont/unifont.otf"
CELL_W, CELL_H = 8, 16

CHARS = (
    [chr(c) for c in range(32, 127)]            # ASCII
    + ["Ё", "ё"]
    + [chr(c) for c in range(0x410, 0x450)]     # А–я
    + list("«»—–…№·°±×“”„’")
    + list("█░▒▓▌▐▀▄")
    + list("←↑→↓►◄▲▼●○■□")
    + list("─│┌┐└┘├┤┬┴┼═║╔╗╚╝")
)


def main() -> int:
    font_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_FONT
    font = ImageFont.truetype(font_path, CELL_H)
    codes = sorted({ord(c) for c in CHARS})

    rows_out = []
    for code in codes:
        ch = chr(code)
        if round(font.getlength(ch)) != CELL_W:
            raise SystemExit(f"символ U+{code:04X} не шириной {CELL_W} px — не моноширинный глиф")
        img = Image.new("1", (CELL_W, CELL_H), 0)
        draw = ImageDraw.Draw(img)
        draw.fontmode = "1"  # без сглаживания: чистые пиксели
        draw.text((0, 0), ch, font=font, fill=1)
        rows = []
        for y in range(CELL_H):
            byte = 0
            for x in range(CELL_W):
                if img.getpixel((x, y)):
                    byte |= 0x80 >> x
            rows.append(byte)
        if code != 32 and not any(rows):
            raise SystemExit(f"в шрифте нет глифа для U+{code:04X}")
        rows_out.append(rows)

    out = Path(__file__).resolve().parent.parent / "src" / "engine" / "font_data.inc"
    with out.open("w", encoding="utf-8") as f:
        f.write("// Сгенерировано tools/gen_font.py — руками не править.\n")
        f.write("// Глифы GNU Unifont 8x16 (SIL OFL 1.1 / GPLv2+ with font embedding exception).\n")
        f.write(f"static const int kFontGlyphCount = {len(codes)};\n")
        f.write("static const uint32_t kFontCodes[] = {\n")
        for i in range(0, len(codes), 12):
            f.write("    " + ", ".join(f"0x{c:04X}" for c in codes[i:i + 12]) + ",\n")
        f.write("};\n")
        f.write(f"static const uint8_t kFontRows[][{CELL_H}] = {{\n")
        for code, rows in zip(codes, rows_out):
            f.write("    {" + ",".join(f"0x{b:02X}" for b in rows) + f"}}, // U+{code:04X}\n")
        f.write("};\n")
    print(f"{len(codes)} глифов -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
