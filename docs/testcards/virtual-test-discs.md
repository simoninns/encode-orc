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
  - Chapter 1: Various test tones (sine waves, square, sawtooth, noise, and log sweeps)
  - Chapter 2: Organ WAV file (342 frames)
  - Chapter 3: Alternating log sweeps (ascending/descending 20 Hz - 20 kHz)
  - Chapter 4: 1 kHz sine tone with varying noise levels (SNR 30/25/20/15 dB)
  - Chapter 5: 500 Hz sine tone with varying dropout densities
  - Chapter 6: 750 Hz sine tone with combined noise and dropouts
  - Chapter 7: Ice skating video with original audio
- **Picture Numbering:** Starts at frame 1 (after lead-in)

### Disc Structure

#### Lead-in (2 frames)
- 625_50_75_BARS test pattern
- User code: 1066
- Disc area: Lead-in

#### Chapter 1: PAL Raw Test Patterns with Audio Test Tones (frames 1-950)
50 frames each (2 seconds @ 25fps) of 19 different YUV422 raw test patterns with various audio test signals:

1. **625_50_100_BARS** (frames 1-50) - **50 Hz sine tone**
2. **625_50_75_BARS** (frames 51-100) - **100 Hz sine tone**
3. **625_50_75_BARS_RED** (frames 101-150) - **315 Hz sine tone** (classic alignment)
4. **625_50_CHROMA_RAMP** (frames 151-200) - **400 Hz sine tone** (broadcast reference)
5. **625_50_FULL_RAMP** (frames 201-250) - **1 kHz sine tone** (primary reference)
6. **625_50_GREY_10H_STEP** (frames 251-300) - **2 kHz sine tone**
7. **625_50_GREY_10V_STEP** (frames 301-350) - **3.15 kHz sine tone** (IEC/CCIR alignment)
8. **625_50_GREY_5H_STEP** (frames 351-400) - **5 kHz sine tone**
9. **625_50_GREY_5V_STEP** (frames 401-450) - **8 kHz sine tone**
10. **625_50_LEGAL_RAMP** (frames 451-500) - **10 kHz sine tone**
11. **625_50_LUMA_RAMP** (frames 501-550) - **12.5 kHz sine tone**
12. **625_50_LUMA_RAMP_DOWN** (frames 551-600) - **15 kHz sine tone**
13. **625_50_MULTIBURST** (frames 601-650) - **20 Hz → 20 kHz log sweep** (2 seconds)
14. **625_50_PLUGE** (frames 651-700) - **440 Hz square wave**
15. **625_50_SMPTE_BARS** (frames 701-750) - **220 Hz sawtooth wave**
16. **625_50_TARTAN** (frames 751-800) - **pink noise** (75% amplitude, seed 42)
17. **625_50_VALID_RAMPS** (frames 801-850) - **white noise** (50% amplitude, seed 123)
18. **625_50_VERT_LUMA_RAMP** (frames 851-900) - **brown noise** (75% amplitude, seed 456)
19. **625_50_Y_CB_CR_RAMPS** (frames 901-950) - **100 Hz → 10 kHz log sweep** (2 seconds)

#### Chapter 2: Audio Test - Organ WAV (frames 951-1292)
342 frames (13.68 seconds @ 25fps):

- **Video:** 625_50_SMPTE_BARS test pattern
- **Audio:** `organ.wav` file

#### Chapter 3: MOV Video Files (frames 1293+)
One MOV file repeated 10 times (full duration each time) with alternating log sweeps:

- **625-50-i_Moving-Zone-2H.mov** × 10
  - Odd repetitions (1, 3, 5, 7, 9): **20 Hz → 20 kHz log sweep**
  - Even repetitions (2, 4, 6, 8, 10): **20 kHz → 20 Hz log sweep**

#### Chapter 4: SMPTE Bars with Noise - SNR Tests (125 frames each, 500 frames total)
Four sections testing different noise levels on SMPTE color bars:

1. **SMPTE_BARS_SNR_30dB** (125 frames) - **1 kHz sine tone**
   - Noise: SNR 30 dB (light noise)
2. **SMPTE_BARS_SNR_25dB** (125 frames) - **1 kHz sine tone**
   - Noise: SNR 25 dB (moderate noise)
3. **SMPTE_BARS_SNR_20dB** (125 frames) - **1 kHz sine tone**
   - Noise: SNR 20 dB (heavy noise)
4. **SMPTE_BARS_SNR_15dB** (125 frames) - **1 kHz sine tone**
   - Noise: SNR 15 dB (severe noise)

#### Chapter 5: SMPTE Bars with Dropouts - Density Tests (125 frames each, 500 frames total)
Four sections testing different dropout densities on SMPTE color bars:

1. **SMPTE_BARS_DROPOUT_0.001** (125 frames) - **500 Hz sine tone**
   - Dropout density: 0.001 (light dropouts)
   - Multi-field: 30%, Single-field: 70%
2. **SMPTE_BARS_DROPOUT_0.005** (125 frames) - **500 Hz sine tone**
   - Dropout density: 0.005 (moderate dropouts)
   - Multi-field: 30%, Single-field: 70%
3. **SMPTE_BARS_DROPOUT_0.01** (125 frames) - **500 Hz sine tone**
   - Dropout density: 0.01 (heavy dropouts)
   - Multi-field: 30%, Single-field: 70%
4. **SMPTE_BARS_DROPOUT_0.02** (125 frames) - **500 Hz sine tone**
   - Dropout density: 0.02 (severe dropouts)
   - Multi-field: 30%, Single-field: 70%

#### Chapter 6: SMPTE Bars with Combined Effects (125 frames each, 500 frames total)
Four sections testing combined noise and dropouts on SMPTE color bars:

1. **SMPTE_BARS_NOISE_DROPOUT_LIGHT** (125 frames) - **750 Hz sine tone**
   - Noise: SNR 35 dB
   - Dropout density: 0.001, Multi-field: 30%, Single-field: 70%
2. **SMPTE_BARS_NOISE_DROPOUT_MODERATE** (125 frames) - **750 Hz sine tone**
   - Noise: SNR 28 dB
   - Dropout density: 0.005, Multi-field: 30%, Single-field: 70%
3. **SMPTE_BARS_NOISE_DROPOUT_HEAVY** (125 frames) - **750 Hz sine tone**
   - Noise: SNR 22 dB
   - Dropout density: 0.01, Multi-field: 30%, Single-field: 70%
4. **SMPTE_BARS_NOISE_DROPOUT_SEVERE** (125 frames) - **750 Hz sine tone**
   - Noise: SNR 18 dB
   - Dropout density: 0.02, Multi-field: 30%, Single-field: 70%

#### Chapter 7: Ice Skating
- **Video:** `pal-ice-skating.mp4` (full duration, with original audio)
- **Audio:** PCM format (from source)

#### Lead-out (2 frames)
- 625_50_75_BARS test pattern
- User code: 1066
- Disc area: Lead-out

