# PAL Virtual Test Discs

This page documents the PAL-format virtual fixture projects in `ggv-tests/`.

## GGV1066 PAL CAV Composite

**Format:** PAL CAV Composite  
**Project File:** `ggv-tests/ggv1066-pal-cav-composite.yaml`  
**Output:** `ggv-output/ggv1066-pal-cav-composite.tbc`  
**User Code:** 1066

A comprehensive PAL CAV test disc featuring picture-number VBI encoding, VITS signals, static and moving test material, and PCM audio exercises.

### Key Features

- **VBI Picture Numbers:** Lines 16, 17, 18, 328, 329, 330
- **VITS Signals:**
  - Line 13: `multiburst`
  - Line 19: `uk-national`
  - Line 325: `itu-combination`
  - Line 331: `itu-composite`
- **Color Burst:** Enabled
- **Audio:** PCM (tones, sweeps, noise, WAV playback, source-audio passthrough)
- **Structure:** Lead-in, 7 content chapters, lead-out

### Disc Structure Summary

- **Lead-in:** 2 frames of `75_BARS`
- **Chapter 1:** 19 PAL raw patterns, 50 frames each, each with a different audio stimulus
- **Chapter 2:** Organ WAV playback over PAL bars
- **Chapter 3:** Repeated MOV playback with alternating up/down log sweeps
- **Chapter 4:** SMPTE bars with stepped SNR noise levels
- **Chapter 5:** SMPTE bars with stepped dropout densities
- **Chapter 6:** SMPTE bars with combined noise + dropout stress profiles
- **Chapter 7:** Ice skating sequence with source audio
- **Lead-out:** 2 frames of `75_BARS`

## GGV1066 PAL CLV Composite

**Format:** PAL CLV Composite  
**Project File:** `ggv-tests/ggv1066-pal-clv-composite.yaml`  
**Output:** `ggv-output/ggv1066-pal-clv-composite.tbc`  
**User Code:** 1066

PAL CLV variant of the same fixture content.

### Key Differences From PAL CAV

- Uses CLV mode
- Uses biphase VBI timecode (same PAL line placement as CAV metadata block)
- Timecode starts at `00:00:00.00`
- Keeps the same content sequence and audio program as the PAL CAV fixture

## GGV1066 PAL VITC YC

**Format:** PAL Y/C (separate luma/chroma)  
**Project File:** `ggv-tests/ggv1066-pal-vitc-yc.yaml`  
**Output:** `ggv-output/ggv1066-pal-vitc-yc.tbcy` and `ggv-output/ggv1066-pal-vitc-yc.tbcc`  
**Timecode Start:** `00:00:00.00`

PAL tape-style fixture using VITC and split Y/C output.

### Key Features

- **VITC Timecode:** PAL VITC line placement per fixture
- **Color Burst:** Enabled
- **Audio:** PCM (same chapter-level content plan)
- **Output Files:** `.tbcy` (Y) and `.tbcc` (C)
- **No VITS:** VITS signals are not included in this VITC Y/C variant
- **No Disc-Area/Chapter Biphase Metadata:** Uses VITC workflow semantics instead