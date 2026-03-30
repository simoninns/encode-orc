# Testcard Raw Y'CbCr 4:2:2 Assets

The raw frame collections live in `assets/pal/raw/` and `assets/ntsc/raw/`. They contain single-frame **Y'CbCr 4:2:2** test material sized to the active picture area only.

## Format Specification

- **Sampling**: Y'CbCr 4:2:2
- **Storage**: 10-bit component values packed into 16-bit little-endian words
- **Packing order**: Y0, Cb, Y1, Cr for each pixel pair
- **Color space**: ITU-R BT.601-derived component video
- **Range**: Studio range with support for sub-black and super-white test content

## Active Picture Dimensions

- **PAL**: 720 × 576 @ 50i
- **NTSC**: 720 × 480 @ 59.94i
- **PAL-M**: Uses the NTSC-sized 720 × 480 assets when sourcing raw frames

## Representative Files

### PAL (`assets/pal/raw/`)
- `625_50_75_BARS.raw`
- `625_50_100_BARS.raw`
- `625_50_PLUGE.raw`
- `625_50_MULTIBURST.raw`
- `625_50_Y_CB_CR_RAMPS.raw`

### NTSC (`assets/ntsc/raw/`)
- `525_5994_75_BARS.raw`
- `525_5994_100_BARS.raw`
- `525_5994_PLUGE.raw`
- `525_5994_MULTIBURST.raw`
- `525_5994_Y_CB_CR_RAMPS.raw`

The folders also contain ramps, greyscale steps, tartan patterns, legal/full-range ramps, and additional diagnostic images.

## File Sizes

- **PAL**: 1,658,880 bytes for 720 × 576 material
- **NTSC / PAL-M source assets**: 1,382,400 bytes for 720 × 480 material

## Usage Notes

- Files contain the active picture area only; encode-orc adds blanking, sync, and VBI content during encoding.
- These files are the preferred deterministic inputs for test signals because they preserve studio-range values accurately.
- MOV and raw YUV422 assets are the best choice when you need precise PLUGE, ramp, or super-white/sub-black behavior.
