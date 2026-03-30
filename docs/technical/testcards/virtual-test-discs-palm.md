# PAL-M Virtual Test Discs

This page documents the PAL-M format virtual fixture projects in `ggv-tests/`.

PAL-M fixtures use PAL-family chroma behavior on 525-line timing. In practice, these projects use NTSC-sized source assets from `assets/720x480/`.

## GGV1958 PAL-M CAV Composite

**Format:** PAL-M CAV Composite  
**Project File:** `ggv-tests/ggv1958-palm-cav-composite.yaml`  
**Output:** `ggv-output/ggv1958-palm-cav-composite.tbc`  
**User Code:** 1958

A comprehensive PAL-M CAV test disc with CAV picture-number metadata, PAL-family VITS insertion, and full audio/test-content coverage.

### Key Features

- **VBI Picture Numbers:** PAL-M uses 525-line geometry for biphase placement
- **VITS Signals (`vits-pal`):**
  - Line 13: `multiburst`
  - Line 19: `uk-national`
  - Line 275: `itu-combination`
  - Line 281: `itu-composite`
- **Color Burst:** Enabled
- **Audio:** PCM (tones, sweeps, noise, WAV playback, source-audio passthrough)
- **Structure:** Lead-in, 7 content chapters, lead-out

### Disc Structure Summary

- **Lead-in:** 2 frames of `75_BARS`
- **Chapter 1:** 19 NTSC-sized raw patterns, 50 frames each, each with a different audio stimulus
- **Chapter 2:** Organ WAV playback over bars
- **Chapter 3:** Repeated MOV playback with alternating up/down log sweeps
- **Chapter 4:** SMPTE bars with stepped SNR noise levels
- **Chapter 5:** SMPTE bars with stepped dropout densities
- **Chapter 6:** SMPTE bars with combined noise + dropout stress profiles
- **Chapter 7:** Ice skating sequence with source audio
- **Lead-out:** 2 frames of `75_BARS`

## GGV1958 PAL-M CLV Composite

**Format:** PAL-M CLV Composite  
**Project File:** `ggv-tests/ggv1958-palm-clv-composite.yaml`  
**Output:** `ggv-output/ggv1958-palm-clv-composite.tbc`  
**User Code:** 1958

PAL-M CLV variant of the same fixture content.

### Key Differences From PAL-M CAV

- Uses CLV mode
- Uses biphase VBI timecode metadata for 525-line PAL-M geometry
- Timecode starts at `00:00:00.00`
- Keeps the same content sequence and audio program as the PAL-M CAV fixture

## GGV1958 PAL-M VITC YC

**Format:** PAL-M Y/C (separate luma/chroma)  
**Project File:** `ggv-tests/ggv1958-palm-vitc-yc.yaml`  
**Output:** `ggv-output/ggv1958-palm-vitc-yc.tbcy` and `ggv-output/ggv1958-palm-vitc-yc.tbcc`  
**Timecode Start:** `00:00:00.00`

PAL-M tape-style fixture using VITC and split Y/C output.

### Key Features

- **VITC Timecode:** Lines 12, 13, 14, 275, 276, 277
- **Color Burst:** Enabled
- **Audio:** PCM (same chapter-level content plan)
- **Output Files:** `.tbcy` (Y) and `.tbcc` (C)
- **No VITS:** VITS signals are not included in this VITC Y/C variant
- **No Disc-Area/Chapter Biphase Metadata:** Uses VITC workflow semantics instead