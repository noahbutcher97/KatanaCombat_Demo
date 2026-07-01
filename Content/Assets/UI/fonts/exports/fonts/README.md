# KatanaCombat — Fonts for Unreal Engine 5.6

Unreal imports **`.ttf` / `.otf`** directly: drag a font file into the Content Browser and UE creates a **Font Face** asset, then you reference it from a **Font** asset (the *Runtime* cache type is the one UMG uses). This folder is the drop-in home for the three families this design system uses.

## What's bundled

All three are **free, open-source (SIL OFL 1.1)** — redistributable and shippable in a commercial game. Two are already included as `.ttf`; the third is too large to bundle (see note).

```
fonts/
  shippori-mincho/    ← display / titles / wordmark   ⚠️ ADD MANUALLY (see below)
  zen-kaku-gothic-new/ ← UI / body                    ✅ included (Regular/Medium/Bold/Black)
  jetbrains-mono/      ← frame data / code / numerics  ✅ included (Regular/Medium/Bold)
```

**Shippori Mincho is not bundled** — each weight is ~18 MB (a full Japanese serif with kanji), over the import limit. Grab the static TTFs from the official repo **github.com/fontdasu/ShipporiMincho** (`fonts/ttf/` → Medium, SemiBold, Bold, ExtraBold) or "Download family" on Google Fonts, and drop them into `shippori-mincho/`. The auto-importer picks them up on the next launch.

### Where to get them (Google Fonts → "Download family" gives static TTFs)

| Family | Source | Weights this system uses → files to keep |
|---|---|---|
| **Shippori Mincho** | fonts.google.com/specimen/Shippori+Mincho | 500 `…-Medium`, 600 `…-SemiBold`, 700 `…-Bold`, 800 `…-ExtraBold` |
| **Zen Kaku Gothic New** | fonts.google.com/specimen/Zen+Kaku+Gothic+New | 400 `…-Regular`, 500 `…-Medium`, 700 `…-Bold`, 900 `…-Black` |
| **JetBrains Mono** | fonts.google.com/specimen/JetBrains+Mono | 400 `…-Regular`, 500 `…-Medium`, 700 `…-Bold` |

(GitHub mirrors with OFL licenses: `googlefonts/shippori-mincho`, `googlefonts/zen-kakugothic`, `JetBrains/JetBrainsMono`.)

Keep each family's `OFL.txt` next to its files — UE projects should ship the license with the font.

## Importing into UE 5.6 (the right way for UMG)

UE has two font caching modes; **for screen/UMG UI you want Runtime**:

- **Runtime (cached) font** — glyphs rasterized on demand into a font atlas, any size, crisp. **Use this for all UMG widgets.** This is the default when you import a TTF/OTF.
- **Offline (distance-field) font** — pre-baked at a fixed size; only for in-world `TextRender` / 3D text. Don't use for HUD/UMG.

Steps:
1. Drag the `.ttf` files for one family into the Content Browser → UE makes a **Font Face** per file.
2. Create one **Font** asset per family (right-click → Font, or it's auto-made on first import). Set **Font Cache Type = Runtime**.
3. In the Font asset, add a **Typeface entry** per weight (name them `Regular`, `Medium`, `Bold`, …) pointing at the matching Font Face. UMG's Font picker then exposes those names.
4. Reference the Font asset in a **Text** widget or in your style assets (see `../UE5_GUIDE.md` for button/badge styles).

## Composite fonts (mixing Latin + Japanese)

A **Composite Font** lets one logical font serve different character ranges from different files — essential when Latin and Japanese (居合, 緋, move names) appear together.

- **Shippori Mincho** and **Zen Kaku Gothic New** are *Japanese* fonts: each already contains Latin + kana + kanji, so a single Font asset covers both scripts — no sub-family needed.
- **JetBrains Mono is Latin-only.** If you ever set Japanese text in the mono style, add a **Sub-Typeface** in the Font asset with a CJK fallback (Zen Kaku Gothic New) and a character range covering the kana/kanji blocks. In this system, mono is used only for numerics/Latin (frame data, code), so this is usually unnecessary.

## Mapping to the design-system roles

| Token (see `../../tokens/typography.css`) | Family | UMG Font / Typeface |
|---|---|---|
| `--font-display` | Shippori Mincho | Font_Display → Bold / ExtraBold |
| `--font-sans` | Zen Kaku Gothic New | Font_Sans → Regular / Medium / Bold / Black |
| `--font-mono` | JetBrains Mono | Font_Mono → Regular / Medium / Bold |

Sizes are in `../../tokens/typography.css` (a 1.250 scale). UMG font sizes are in points at DPI 96 — multiply the px value in the tokens by 0.75 for the point size, or just author your widgets at the 4K canvas size and use the px values directly.
