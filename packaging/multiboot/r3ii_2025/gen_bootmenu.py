#!/usr/bin/env python3
"""Render the R3II 2025 boot menu: logo/wordmark plus the system name only.

Each name uses the face that system ships as its own default: HiByOS is drawn in
Noto Sans, SonixOS in MiSans. CompasOS keeps its official wordmark.

    gen_menu_r3d.py <out-dir> <cjk.ttf> <noto.ttf> <misans.otf> <asset-dir>
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 320, 480
BG = (12, 14, 18)
CARD = (24, 28, 36)
CARD_SEL = (36, 42, 54)
ACCENT = (86, 170, 255)
TEXT = (238, 242, 248)
TEXT_DIM = (150, 160, 178)

# target, logo file, is wordmark, name, name font key
OPTIONS = [
    ("stock", "hiby.png", False, "HiByOS", "noto"),
    ("compas", "compas.png", True, None, None),
    ("sonix", "sonix.png", False, "SonixOS", "misans"),
]

TITLE = "\u9009\u62e9\u8981\u542f\u52a8\u7684\u7cfb\u7edf"
HINT = "\u5f00\u673a\u65f6\u6309\u4f4f \u64ad\u653e/\u6682\u505c \u952e\u663e\u793a\u6b64\u83dc\u5355"
FOOT = "\u6eda\u8f6e \u5207\u6362 \u00b7 \u64ad\u653e\u952e \u786e\u8ba4 \u00b7 10 \u79d2\u540e\u81ea\u52a8\u542f\u52a8"
SECONDS = "\u79d2"

FONTS = {}


def font(key, size):
    path = FONTS.get(key)
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()


def center(draw, text, fnt, y, fill):
    box = draw.textbbox((0, 0), text, font=fnt)
    draw.text(((W - (box[2] - box[0])) // 2 - box[0], y), text, font=fnt, fill=fill)


def load_logo(asset_dir, name, wordmark, box):
    path = os.path.join(asset_dir, name)
    if not os.path.exists(path):
        return None, 0, 0
    art = Image.open(path).convert("RGBA")
    if wordmark:
        w = box
        h = max(1, round(art.height * w / art.width))
    else:
        h = box
        w = max(1, round(art.width * h / art.height))
    return art.resize((w, h), Image.LANCZOS), w, h


def frame(out_dir, index, asset_dir):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    center(d, TITLE, font("cjk", 20), 40, TEXT)
    center(d, HINT, font("cjk", 12), 70, TEXT_DIM)

    top, row_h, gap = 118, 88, 16
    for i, (_, logo, wordmark, name, font_key) in enumerate(OPTIONS):
        y = top + i * (row_h + gap)
        sel = (i == index)
        d.rounded_rectangle([20, y, W - 20, y + row_h], radius=16,
                            fill=CARD_SEL if sel else CARD)
        if sel:
            d.rounded_rectangle([20, y, 26, y + row_h], radius=16, fill=ACCENT)

        if wordmark:
            art, lw, lh = load_logo(asset_dir, logo, True, 150)
            if art is not None:
                img.paste(art, (44, y + (row_h - lh) // 2), art)
        else:
            art, lw, lh = load_logo(asset_dir, logo, False, 52)
            if art is not None:
                img.paste(art, (44, y + (row_h - lh) // 2), art)
            text_x = 44 + (lw if art is not None else 60) + 18
            f = font(font_key, 26)
            box = d.textbbox((0, 0), name, font=f)
            th = box[3] - box[1]
            d.text((text_x, y + (row_h - th) // 2 - box[1]), name,
                   font=f, fill=TEXT if sel else TEXT_DIM)

    center(d, FOOT, font("cjk", 12), 448, TEXT_DIM)
    with open(os.path.join(out_dir, "bootmenu_%d.raw" % index), "wb") as fh:
        fh.write(img.tobytes())


def countdown(out_dir):
    for s in range(1, 11):
        img = Image.new("RGB", (96, 30), BG)
        d = ImageDraw.Draw(img)
        f = font("cjk", 16)
        text = "%d %s" % (s, SECONDS)
        box = d.textbbox((0, 0), text, font=f)
        d.text(((96 - (box[2] - box[0])) // 2 - box[0],
                (30 - (box[3] - box[1])) // 2 - box[1]), text, font=f, fill=TEXT_DIM)
        with open(os.path.join(out_dir, "count_%d.raw" % s), "wb") as fh:
            fh.write(img.tobytes())


def main():
    out_dir, FONTS["cjk"], FONTS["noto"], FONTS["misans"], asset_dir = sys.argv[1:6]
    os.makedirs(out_dir, exist_ok=True)
    for i in range(len(OPTIONS)):
        frame(out_dir, i, asset_dir)
    countdown(out_dir)
    for name in sorted(os.listdir(out_dir)):
        p = os.path.join(out_dir, name)
        print("%-18s %8d bytes" % (name, os.path.getsize(p)))


if __name__ == "__main__":
    main()
