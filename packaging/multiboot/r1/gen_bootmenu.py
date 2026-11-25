#!/usr/bin/env python3
"""Render the boot menu frames for the R1 (480x800) as raw RGB24.

One frame per option, plus a small strip per countdown value ("10秒" .. "1秒").
The strips cover the exact background colour, so the on-device program can blit
them without any alpha blending, and blank the whole line by painting the same
colour back over it once a key is pressed.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

W, H = 480, 800
CJK = "C:/Users/Vanger/Documents/deepseek-harness/default-workspace/compas-zh/Chinese.ttf"
MISANS = "D:/r1fw/work/fonts/misans/MiSans Latin/otf/MiSansLatin-Demibold.otf"
NOTO = "D:/r1fw/out/NotoSansChinese-Plus.ttf"
ASSETS = "D:/r1fw/out/bootmenu"
OUT = sys.argv[1] if len(sys.argv) > 1 else "."

BG = (12, 14, 18)
FG = (238, 238, 244)
MUTED = (150, 155, 165)
SEL_FILL = (36, 58, 92)
SEL_EDGE = (124, 176, 236)
ROW_FILL = (22, 25, 31)
ROW_EDGE = (44, 48, 56)

ROWS = [
    ("icon", "hiby.png", "HiByOS", "noto"),
    ("logo", "compas.png", None, None),
    ("icon", "sonix.png", "SonixOS", "misans"),
    ("logo", "rockbox.png", None, None),
]

MARGIN = 28
ROW_H = 96
ROW_GAP = 16
RADIUS = 12
TOP = 170
ICON = 56
COUNTDOWN_Y = 742
COUNTDOWN_X = 156


def font(path, size):
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def label_font(style, size):
    return font(NOTO if style == "noto" else MISANS, size)


def load(path):
    return Image.open(path).convert("RGBA")


def fit(img, height):
    scale = height / img.height
    return img.resize((max(1, int(img.width * scale)), height), Image.LANCZOS)


def frame(selected):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    d.text((MARGIN, 84), "\u9009\u62e9\u7cfb\u7edf", font=font(CJK, 34), fill=FG, anchor="lm")

    for i, (kind, asset, label, style) in enumerate(ROWS):
        y = TOP + i * (ROW_H + ROW_GAP)
        active = i == selected
        d.rounded_rectangle([MARGIN, y, W - MARGIN, y + ROW_H], radius=RADIUS,
                            fill=SEL_FILL if active else ROW_FILL,
                            outline=SEL_EDGE if active else ROW_EDGE,
                            width=2)
        art = load(f"{ASSETS}/{asset}")
        mid = y + ROW_H // 2
        if kind == "icon":
            icon = fit(art, ICON)
            img.paste(icon, (MARGIN + 20, mid - ICON // 2), icon)
            d.text((MARGIN + 20 + ICON + 20, mid), label, font=label_font(style, 38),
                   fill=FG if active else (206, 206, 214), anchor="lm")
        else:
            logo = fit(art, 34 if asset == "compas.png" else 30)
            img.paste(logo, (MARGIN + 20, mid - logo.height // 2), logo)

    d.text((MARGIN, COUNTDOWN_Y), "\u81ea\u52a8\u542f\u52a8", font=font(CJK, 24), fill=MUTED, anchor="lm")
    return img


def countdown_strip(seconds):
    """A strip that sits right of the label, painted on the flat background."""
    width, height = 120, 44
    img = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(img)
    d.text((0, height // 2), f"{seconds}\u79d2", font=font(CJK, 26), fill=(196, 202, 214), anchor="lm")
    return img


def main():
    for i in range(len(ROWS)):
        path = f"{OUT}/bootmenu_{i}.raw"
        with open(path, "wb") as f:
            f.write(frame(i).convert("RGB").tobytes())
        print(f"{path}: {W}x{H} RGB24")
    for seconds in range(1, 11):
        path = f"{OUT}/count_{seconds}.raw"
        with open(path, "wb") as f:
            f.write(countdown_strip(seconds).tobytes())
        print(f"{path}")


if __name__ == "__main__":
    main()
