#!/usr/bin/env python3
"""
fontgen.py – tạo font bitmap tiếng Việt (anti-alias 2 bit/pixel) cho màn ILI9341.

Chạy từ thư mục gốc repo (cần Python 3 + Pillow: pip install pillow):
    python tools/fontgen/fontgen.py
→ ghi App/drivers/ili9341/fonts_vn.c

Font nguồn: Be Vietnam Pro (SIL Open Font License 1.1, xem tools/fontgen/OFL.txt).
Muốn đổi cỡ chữ / thêm ký tự: sửa bảng FONTS bên dưới rồi chạy lại.
Lưu ý Flash (64 KB): font_vn16 ~9.5 KB, font_num ~3.8 KB. Thêm 1 font chữ đầy đủ cỡ 20 px tốn ~16 KB.
"""
import os
import unicodedata
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT_C = os.path.join(ROOT, "App", "drivers", "ili9341", "fonts_vn.c")

# ---------- bộ ký tự ----------
def vietnamese_letters():
    vowels = "aăâeêioôơuưy"
    tones = ["", "\u0300", "\u0301", "\u0303", "\u0309", "\u0323"]   # huyền sắc ngã hỏi nặng
    out = set("đĐ")
    for v in vowels:
        for t in tones:
            for c in (v, v.upper()):
                out.add(unicodedata.normalize("NFC", c + t))
    return {c for c in out if ord(c) > 0x7E}

ASCII = {chr(c) for c in range(0x20, 0x7F)}
TEXT_SET = ASCII | vietnamese_letters() | set("°·•–…")
NUM_SET = set("0123456789.-:%°C ")


FONTS = [
    # tên C,      file TTF,                  cỡ px, bộ ký tự
    ("font_vn16", "BeVietnamPro-Medium.ttf",   15, TEXT_SET),
    ("font_num",  "BeVietnamPro-SemiBold.ttf", 46, NUM_SET),
]
BPP = 2


def render_font(cname, ttf, size, chars):
    path = os.path.join(HERE, ttf)
    if not os.path.exists(path):                       # Medium không có → dùng Regular
        path = os.path.join(HERE, "BeVietnamPro-Regular.ttf")
    font = ImageFont.truetype(path, size)
    chars = sorted(chars, key=ord)

    # khung dòng: đỉnh/đáy thực tế của mọi ký tự so với baseline
    top, bottom = 0, 0
    for ch in chars:
        l, t, r, b = font.getbbox(ch, anchor="ls")
        top, bottom = min(top, t), max(bottom, b)
    base = -top                     # baseline tính từ đỉnh dòng
    height = bottom - top

    glyphs, bits = [], bytearray()
    for ch in chars:
        adv = int(round(font.getlength(ch)))
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        entry = dict(cp=ord(ch), adv=adv, x=l, y=t + base, w=w, h=h, off=len(bits))
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
            px = img.load()
            acc, n = 0, 0
            for yy in range(h):
                for xx in range(w):
                    q = (px[xx, yy] * 3 + 127) // 255           # 0..3
                    acc = (acc << 2) | q
                    n += 1
                    if n == 4:
                        bits.append(acc); acc, n = 0, 0
            if n:
                bits.append(acc << (2 * (4 - n)))
        glyphs.append(entry)
    return dict(cname=cname, size=size, height=height, base=base, glyphs=glyphs, bits=bits,
                ttf=os.path.basename(path))


def emit(fonts):
    L = []
    L.append("/**")
    L.append(" * @file    fonts_vn.c")
    L.append(" * @brief   Font tiếng Việt – TỰ SINH bởi tools/fontgen/fontgen.py, KHÔNG sửa tay.")
    L.append(" *          Be Vietnam Pro © The Be Vietnam Pro Project Authors, SIL Open Font License 1.1")
    L.append(" */")
    L.append('#include "ili9341_text.h"')
    L.append("")
    total = 0
    for f in fonts:
        n = f["cname"]
        L.append(f"/* ---- {n}: {f['ttf']} {f['size']}px, cao dòng {f['height']}px, "
                 f"{len(f['glyphs'])} ký tự, {len(f['bits'])} byte bitmap ---- */")
        L.append(f"static const uint8_t {n}_bits[{len(f['bits'])}] = {{")
        b = f["bits"]
        for i in range(0, len(b), 20):
            L.append("    " + ",".join(f"0x{v:02X}" for v in b[i:i + 20]) + ",")
        L.append("};")
        L.append(f"static const vfont_glyph_t {n}_glyphs[{len(f['glyphs'])}] = {{")
        for g in f["glyphs"]:
            ch = chr(g["cp"])
            cm = ch if ch not in "\\*/" and g["cp"] >= 0x21 else ""
            L.append(f"    {{0x{g['cp']:04X},{g['off']:5d},{g['w']:2d},{g['h']:2d},{g['x']:3d},{g['y']:3d},{g['adv']:2d}}}, /* {cm} */")
        L.append("};")
        L.append(f"const vfont_t {n} = {{ {n}_bits, {n}_glyphs, {len(f['glyphs'])}, {f['height']}, {f['base']}, {BPP} }};")
        L.append("")
        total += len(f["bits"]) + len(f["glyphs"]) * 10
    open(OUT_C, "w", encoding="utf-8", newline="\n").write("\n".join(L))
    return total


if __name__ == "__main__":
    fonts = [render_font(*spec) for spec in FONTS]
    total = emit(fonts)
    for f in fonts:
        print(f"{f['cname']}: {f['ttf']} {f['size']}px -> cao {f['height']}px, "
              f"{len(f['glyphs'])} ky tu, {len(f['bits'])} B bitmap")
    print(f"Tong ~{total} B Flash -> {OUT_C}")
