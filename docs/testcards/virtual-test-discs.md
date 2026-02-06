---
title: Virtual Test Discs
layout: default
parent: Testcards
nav_order: 3
---

# Virtual Test Discs

This page documents comprehensive virtual LaserDisc test projects that combine multiple test patterns, video sources, and metadata encoding features into complete disc images.

## GGV1066

**Format:** PAL CAV  
**Project File:** `test-projects/ggv1066.yaml`  
**Output:** `test-output/ggv1066.tbc`  
**User Code:** 1066

A comprehensive PAL CAV test disc featuring picture number VBI encoding, VITS signals, and a wide variety of test content including static patterns, video sequences, and audio testing.

### Features

- **VBI Picture Numbers:** Lines 16, 17, 18, 328, 329, 330
- **VITS Signals:**
  - Line 13: Multiburst
  - Line 19: UK National
  - Line 325: ITU Combination
  - Line 331: ITU Composite
- **Color Burst:** Enabled
- **Audio:** PCM format
  - Chapter 1: Audio test tones (50 Hz - 15 kHz) and log sweep (20 Hz - 20 kHz)
  - Chapter 2: Alternating log sweeps (ascending/descending 20 Hz - 20 kHz)
  - Chapter 5: Ice skating video with original audio
- **Picture Numbering:** Starts at frame 1 (after lead-in)

### Disc Structure

#### Lead-in (2 frames)
- Geometry-Checkers-32 test pattern
- User code: 1066
- Disc area: Lead-in

#### Chapter 1: PAL Raw Test Patterns with Audio Test Tones (frames 1-950)
50 frames each (2 seconds @ 25fps) of 19 different YUV422 raw test patterns. First 13 sections include audio test tones:

1. **625_50_100_BARS** (frames 1-50) - **50 Hz tone**
2. **625_50_75_BARS** (frames 51-100) - **100 Hz tone**
3. **625_50_75_BARS_RED** (frames 101-150) - **315 Hz tone** (classic alignment)
4. **625_50_CHROMA_RAMP** (frames 151-200) - **400 Hz tone** (broadcast reference)
5. **625_50_FULL_RAMP** (frames 201-250) - **1 kHz tone** (primary reference)
6. **625_50_GREY_10H_STEP** (frames 251-300) - **2 kHz tone**
7. **625_50_GREY_10V_STEP** (frames 301-350) - **3.15 kHz tone** (IEC/CCIR alignment)
8. **625_50_GREY_5H_STEP** (frames 351-400) - **5 kHz tone**
9. **625_50_GREY_5V_STEP** (frames 401-450) - **8 kHz tone**
10. **625_50_LEGAL_RAMP** (frames 451-500) - **10 kHz tone**
11. **625_50_LUMA_RAMP** (frames 501-550) - **12.5 kHz tone**
12. **625_50_LUMA_RAMP_DOWN** (frames 551-600) - **15 kHz tone**
13. **625_50_MULTIBURST** (frames 601-650) - **20 Hz → 20 kHz log sweep** (2 seconds)
14. **625_50_PLUGE** (frames 651-700) - silent
15. **625_50_SMPTE_BARS** (frames 701-750) - silent
16. **625_50_TARTAN** (frames 751-800) - silent
17. **625_50_VALID_RAMPS** (frames 801-850) - silent
18. **625_50_VERT_LUMA_RAMP** (frames 851-900) - silent
19. **625_50_Y_CB_CR_RAMPS** (frames 901-950) - silent

#### Chapter 2: MOV Video Files (frames 951+)
One MOV file repeated 10 times (full duration each time) with alternating log sweeps:

- **625-50-i_Moving-Zone-2H.mov** × 10
  - Odd repetitions (1, 3, 5, 7, 9): **20 Hz → 20 kHz log sweep**
  - Even repetitions (2, 4, 6, 8, 10): **20 kHz → 20 Hz log sweep**

#### Chapter 3: PNG Test Cards (frames following Chapter 2)
205 PNG test patterns from the Bill Wetzel collection, 1 frame each (silent):

- Clipping tests (broadcast/full range, high/low, RGB channels)
- Color bars (horizontal/vertical, various spacings)
- Color composition tests (step wipes, solid colors)
- Color patches and random patterns
- Color space tests (HSL, HSV, RGB swatches and wipes)
- Gamma tests (checker patterns, line patterns)
- Geometry tests (bars, checkers, circles, grids, lines, points, squares)
- Resolution tests (horizontal/vertical wedges, star patterns)
- Distortion and convergence tests

#### Chapter 4: Nynashamn
- **Video:** `pal-nynashamn.mp4` (full duration, silent)

#### Chapter 5: Ice Skating
- **Video:** `pal-ice-skating.mp4` (full duration, with original audio)
- **Audio:** PCM format

#### Lead-out (2 frames)
- Geometry-Checkers-32 test pattern
- User code: 1066
- Disc area: Lead-out

