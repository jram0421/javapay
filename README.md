# JavaPay — Pebble Time 2 (Emery) Fork

Fork of [a2/javapay](https://github.com/a2/javapay) adding native support for the **Pebble Time 2** (platform codename: `emery`, 200×228 px color display).

## What changed from v1.3

### `appinfo.json`
- Added `"emery"` to `targetPlatforms`
- Version bumped to `2.0`

### `src/barcode_window.c`
Added an `#if defined(PBL_PLATFORM_EMERY)` layout branch for the 200×228 screen:
- `barcode_frame`: `GRect(0, 75, 200, 80)` — full width, taller barcode area
- `app_icon_frame`: `GRect(40, 30, 119, 25)` — centered in the wider display
- `card_number_frame`: `GRect(4, 165, 192, 20)` — near-full-width at bottom

### `src/card_window.c`
Added `PBL_PLATFORM_EMERY` layout macros (`PROMPT_FRAME`, `CARD_NUM_FRAME`, `SEL_START`) for the card entry UI, using the extra vertical space (228px vs 168px) to give the prompt and digit selector more room to breathe.

### `src/settings_window.c`
Changed `#if PBL_PLATFORM_APLITE` → `#if PBL_BW` for the text color guard. Emery is a color display, so it was already unaffected, but `PBL_BW` is the semantically correct check for "monochrome display."

## Building

```sh
pebble build
pebble install --emulator emery
```
