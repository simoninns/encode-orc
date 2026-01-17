# Testcard Raw RGB30 Assets

The pal-raw and ntsc-raw folders contains raw **RGB30** (10-bit per channel) color bar frames stored in **RGB48 containers** (16-bit little-endian per channel, interleaved R,G,B) for PAL EBU and NTSC EIA at 75% and 100% saturation. Files are single-frame images sized to the active picture area only. All values use the **10-bit 64-940 standard** (ITU-R BT.601).

## Files
- `pal-ebu-colorbars-100.raw` — 720x576, PAL EBU bars, 100% saturation
- `pal-ebu-colorbars-75.raw` — 720x576, PAL EBU bars, 75% saturation
- `ntsc-eia-colorbars-100.raw` — 720x480, NTSC EIA/SMPTE bars, 100% saturation
- `ntsc-eia-colorbars-75.raw` — 720x480, NTSC EIA/SMPTE bars, 75% saturation

## Pixel format
- **RGB30**: Interleaved RGB, each channel 10-bit, values 0–1023.
- **Container**: Stored as 16-bit little-endian per channel (RGB48), with upper 6 bits unused.
- Row-major, no padding; total bytes = width × height × 3 × 2.

## 10-bit 64-940 Standard
- **Black**: 64
- **White**: 940
- **Range**: 0–1023 (fits in 10 bits)
- Color values interpolate linearly: `value = 64 + fraction × (940 − 64)`, where fraction is 0.0–1.0.
- Example: 100% white = (940, 940, 940); 75% white = (755, 755, 755); black = (64, 64, 64).

## Color ordering (left to right)
White, Yellow, Cyan, Green, Magenta, Red, Blue, Black.

## Sizes (bytes)
- PAL: 720 × 576 × 6 = 2,488,320 bytes
- NTSC: 720 × 480 × 6 = 2,073,600 bytes

## Notes
- Images contain only the active picture region; add your own blanking/VBI if needed.
- Use these as baseline deterministic inputs for encoder tests with well-defined digital levels.
