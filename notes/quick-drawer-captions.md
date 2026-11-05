# Quick drawer captions: why this build uses the theme size

Upstream pins the quick drawer's toggle captions and state labels to a fixed
12 px font (`fallback_font_ui_12()`), and the comment there explains why: the
captions sit in an 84 px cell under their icon, and at the theme size the word
`Bluetooth` no longer fits on one line.

That is a good decision for an English interface. It is the wrong one for this
build, which ships Simplified Chinese by default:

| Caption | English | Chinese | Width at 20 px |
|---|---|---|---|
| Wi-Fi | `Wi-Fi` | `无线网络` | fits |
| Bluetooth | `Bluetooth` | `蓝牙` | fits |
| Sleep | `Sleep` | `睡眠` | fits |
| Crossfade | `Crossfade` | `淡入淡出` | fits |
| Gapless | `Gapless` | `无缝播放` | fits |

So the captions and the state labels below them use
`gui_theme_font(GUI_FONT_ROLE_SUBTEXT)` instead of the fixed 12 px font.

## What size that actually is

The role maps to `app_font_16`, but the slot name is not the pixel size: the
slot is built from the interface's font-size tier (`tier_pixel_sizes()` in
`fallback_font.c`), and the default tier is **Medium (1)**:

| Settings → Font Size | `SUBTEXT` renders at |
|---|---|
| Small (0) | 16 px |
| **Medium (1) — default** | **20 px** |
| Large / BlindMF (2) | 24 px |

The R1 does not scale these further: `board_font_px()` only rescales on the
compact boards, so on this device the numbers above are the pixel heights.

Two consequences, both intended:

- The drawer's text matches every other label on screen, and follows
  **Settings → Font Size** like the rest of the interface.
- If the interface language is switched back to English, `Bluetooth` wraps to a
  second line inside its cell. Nothing is clipped — the label is sized to its
  content — it is just two lines where it used to be one.

## What is *not* changed

- The brightness and volume read-outs stay at their own fixed face: they render
  numbers and a percent sign, never a glyph that needs a fallback.
- The TempoTec V1 board builds keep their own line-height maths; those paths are
  inside `#if defined(BOARD_TEMPOTEC_V1)` and the R1 never takes them.
