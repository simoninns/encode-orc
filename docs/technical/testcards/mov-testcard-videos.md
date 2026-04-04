# Testcard MOV Video Assets

The MOV test-card collections live in `assets/720x576/video/mov_25_00/` (PAL) and `assets/704x480/video/mov_29_97/` (NTSC). Each file is a short clip containing a moving or animated test pattern encoded in **v210** (10-bit 4:2:2 Y'CbCr) with uncompressed 24-bit PCM audio.

## Format Specification

| Property | PAL | NTSC |
|---|---|---|
| Container | QuickTime MOV | QuickTime MOV |
| Video codec | v210 (10-bit 4:2:2 YCbCr) | v210 (10-bit 4:2:2 YCbCr) |
| Active picture size | 720 × 576 | 704 × 480 |
| Frame rate | 25 fps | 29.97 fps |
| Audio | PCM S24LE, 48 kHz | – |
| Colour space | ITU-R BT.601 studio range | ITU-R BT.601 studio range |
| Pixel aspect ratio | 16:15 (SAR → DAR 4:3) | 8:9 (SAR → DAR 4:3) |

!!! note "PAL-M source material"
    PAL-M output formats (`palm-composite`, `palm-yc`) use NTSC-sized **720 × 480** source material. The MOV assets in this collection are **704 × 480** (standard NTSC active area). Verify your project dimensions match before using these files with PAL-M output.

## Complete File Inventory

### PAL — `assets/720x576/video/mov_25_00/`

| File | Frames | Duration | Audio tracks | Description |
|---|---|---|---|---|
| `Moving-Zone-2H.mov` | 43 | 1.72 s | 1 × stereo | Moving two-harmonic zone plate |
| `PLUGE.mov` | 43 | 1.72 s | 1 × stereo | PLUGE picture line-up signal |
| `pt5300.mov` | 75 | 3.00 s | 4 × stereo | Multi-channel audio/video test signal |

### NTSC — `assets/704x480/video/mov_29_97/`

| File | Frames | Duration | Audio tracks | Description |
|---|---|---|---|---|
| `MOVING_ZONE_2H.mov` | 50 | 1.67 s | – | Moving two-harmonic zone plate |

## Video Reference

The animated previews below are sampled from the source files at reduced resolution for web display. Zone-plate animations are downscaled to 180 px wide to keep file sizes manageable; other clips are shown at 240 px wide.

---

### Moving Zone Plate — Two Harmonics (`Moving-Zone-2H` / `MOVING_ZONE_2H`)

A two-harmonic zone plate that produces concentric ring interference patterns which scroll and rotate over time. The spatial frequency content sweeps across the entire active picture, exercising chroma and luma bandwidth simultaneously. This is one of the most demanding test signals for a video encoder because every pixel in every frame is different — there is no static region to compress against.

The PAL version (`Moving-Zone-2H.mov`) is in `assets/720x576/video/mov_25_00/`. The NTSC version (`MOVING_ZONE_2H.mov`) is in `assets/704x480/video/mov_29_97/`.

| PAL 720×576 @ 25 fps | NTSC 704×480 @ 29.97 fps |
|:---:|:---:|
| ![Moving Zone 2H PAL](images/pal/Moving-Zone-2H.apng){ width="340" } | ![Moving Zone 2H NTSC](images/ntsc/MOVING_ZONE_2H.apng){ width="340" } |

---

### PLUGE (`PLUGE.mov`)

A PAL-only PLUGE (Picture Line-Up Generation Equipment) signal. The clip contains the standard PLUGE trio of below-black, nominal black, and just-above-black bars alongside a mid-grey reference field. The signal includes a stereo audio track. The PLUGE content is essentially static between frames; the MOV wrapper allows the signal to carry audio and to interleave with other sections in a YAML project.

| PAL 720×576 @ 25 fps |
|:---:|
| ![PLUGE PAL](images/pal/PLUGE.apng){ width="340" } |

---

### Multi-Channel Audio/Video Test Signal (`pt5300.mov`)

A PAL test signal spanning 75 frames (3 seconds) that includes both video and four independent stereo audio tracks (48 kHz, 24-bit PCM). It is used for end-to-end audio/video alignment and multi-channel audio verification. The video component contains an animated pattern; the four audio tracks can carry different test tones or timing references simultaneously.

| PAL 720×576 @ 25 fps |
|:---:|
| ![pt5300 PAL](images/pal/pt5300.apng){ width="340" } |

---

## YAML Usage

### PAL — Moving Zone Plate

```yaml
sections:
  - name: "Moving Zone 2H"
    duration: 43
    source:
      type: "mov-file"
      file: "${ENCODE_ORC_ASSETS}/720x576/video/mov_25_00/Moving-Zone-2H.mov"
```

### PAL — PLUGE

```yaml
sections:
  - name: "PLUGE"
    duration: 43
    source:
      type: "mov-file"
      file: "${ENCODE_ORC_ASSETS}/720x576/video/mov_25_00/PLUGE.mov"
```

### PAL — Multi-Channel Audio/Video

```yaml
sections:
  - name: "pt5300"
    duration: 75
    source:
      type: "mov-file"
      file: "${ENCODE_ORC_ASSETS}/720x576/video/mov_25_00/pt5300.mov"
```

### NTSC — Moving Zone Plate

```yaml
sections:
  - name: "Moving Zone 2H"
    duration: 50
    source:
      type: "mov-file"
      file: "${ENCODE_ORC_ASSETS}/704x480/video/mov_29_97/MOVING_ZONE_2H.mov"
```

## Regenerating Preview Animations

The APNG previews in `docs/technical/testcards/images/` were generated from the MOV files using the helper script `scripts/mov_to_apng.py`. To regenerate them:

```sh
nix develop
python3 scripts/mov_to_apng.py
```

Zone-plate animations are sampled at every 6th–8th source frame and scaled to 180 px wide. Other clips use continuous fps down-sampling to 240 px wide. All previews loop indefinitely.
