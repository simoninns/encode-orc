# NTSC Virtual Test Discs

This page documents the NTSC-format virtual fixture projects in `ggv-tests/`.

## GGV1986 NTSC CAV Composite

**Format:** NTSC CAV Composite  
**Project File:** `ggv-tests/ggv1986-ntsc-cav-composite.yaml`  
**Output:** `ggv-output/ggv1986-ntsc-cav-composite.tbc`  
**User Code:** 1986

A comprehensive NTSC CAV test disc featuring picture-number VBI encoding, VITS signals, static and moving test material, and PCM audio exercises.

### Key Features

- **VBI Picture Numbers:** Lines 16, 17, 18, 278, 279, 280
- **VITS Signals:**
  - Line 13: `ntc7-composite`
  - Line 19: `vir`
  - Line 275: `ntc7-combination`
  - Line 281: `vir`
- **Color Burst:** Enabled
- **Audio:** PCM (tones, sweeps, noise, WAV playback, source-audio passthrough)
- **Structure:** Lead-in, 7 content chapters, lead-out

### Disc Structure Summary

- **Lead-in:** 2 frames of `75_BARS`
- **Chapter 1:** 19 NTSC raw patterns, 50 frames each, each with a different audio stimulus
- **Chapter 2:** Organ WAV playback over NTSC bars
- **Chapter 3:** Repeated MOV playback with alternating up/down log sweeps
- **Chapter 4:** SMPTE bars with stepped SNR noise levels
- **Chapter 5:** SMPTE bars with stepped dropout densities
- **Chapter 6:** SMPTE bars with combined noise + dropout stress profiles
- **Chapter 7:** Ice skating sequence with source audio
- **Lead-out:** 2 frames of `75_BARS`

## GGV1986 NTSC CLV Composite

**Format:** NTSC CLV Composite  
**Project File:** `ggv-tests/ggv1986-ntsc-clv-composite.yaml`  
**Output:** `ggv-output/ggv1986-ntsc-clv-composite.tbc`  
**User Code:** 1986

NTSC CLV variant of the same fixture content.

### Key Differences From NTSC CAV

- Uses CLV mode
- Uses biphase VBI timecode on lines 16, 17, 18, 278, 279, 280
- Timecode starts at `00:00:00.00`
- Keeps the same content sequence and audio program as the NTSC CAV fixture

## GGV1986 NTSC VITC YC

**Format:** NTSC Y/C (separate luma/chroma)  
**Project File:** `ggv-tests/ggv1986-ntsc-vitc-yc.yaml`  
**Output:** `ggv-output/ggv1986-ntsc-vitc-yc.tbcy` and `ggv-output/ggv1986-ntsc-vitc-yc.tbcc`  
**Timecode Start:** `00:00:00.00`

NTSC tape-style fixture using VITC and split Y/C output.

### Key Features

- **VITC Timecode:** Lines 12, 13, 14, 275, 276, 277
- **Color Burst:** Enabled
- **Audio:** PCM (same chapter-level content plan)
- **Output Files:** `.tbcy` (Y) and `.tbcc` (C)
- **No VITS:** VITS signals are not included in this VITC Y/C variant
- **No Disc-Area/Chapter Biphase Metadata:** Uses VITC workflow semantics instead