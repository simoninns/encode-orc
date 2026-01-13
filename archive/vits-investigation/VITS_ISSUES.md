# IEC 60856-1986 VITS Generation - Issues and Fixes Required

## Line 19 Analysis

### Specification vs Generated

| Element | Spec | Generated | Issue |
|---------|------|-----------|-------|
| B2 (White Ref) | 0.70V flat, rise/fall 100±10ns | Correct 0xE000 | ✓ CORRECT |
| B1 (2T Pulse) | 0.70V sine-squared, 200ns width | Low values 0x244a-0xE000 | ✗ WRONG |
| F (20T Pulse) | 0.70V carrier-modulated, 2000ns | Low values 0x244a-0x5bb6 | ✗ WRONG |
| D1 (Staircase) | 0.70V 6-level staircase | Values down to 12930 | ✗ WRONG |

### Problems Identified

1. **B1 and F signals are being generated with wrong amplitudes**
   - Should be centered around 0xE000 (white) with envelope modulation
   - Currently showing low values suggesting inverted or clipped calculations

2. **Staircase levels are incorrect**
   - Should be 6 evenly-spaced levels from 0x4000 (black) to 0xE000 (white)
   - Currently showing arbitrary low values

3. **Rise/Fall times may not match spec**
   - Using simplified linear interpolation instead of Thomson filter
   - Should match 100ns for B2, 235ns for D1, etc.

## Line 20 Analysis

### Specification vs Generated

| Element | Spec | Generated | Issue |
|---------|------|-----------|-------|
| C1 (White Ref) | 80% = 0x8000+ | 0x9021 | ✓ CLOSE |
| C2 (Black Ref) | 20% = 0x5000 | 0x9000 | ✗ WRONG (inverted!) |
| C3 (Multiburst) | 6 frequencies, 60% amp | Modulated but unclear | ? UNCLEAR |

### Problems Identified

1. **C2 is HIGHER than C1** - they should be reversed!
   - C2 should be 20% (dark) but it's showing higher than C1 which should be 80% (bright)

2. **Multiburst structure unclear**
   - Should have 6 distinct bursts at 0.5, 1.3, 2.3, 4.2, 4.8, 5.8 MHz
   - Currently appears to be continuous modulation

## Root Causes

1. Sine-squared envelope math is producing values outside expected range
2. Level calculations may have sign errors or be using wrong base levels
3. Staircase implementation not producing discrete levels properly
4. Spacing between elements not matching spec

## Required Fixes

1. **Rewrite B1, F, D1 generation with correct mathematical expressions**
2. **Fix staircase to produce exact 6 levels**
3. **Fix C2 implementation (currently inverted relative to C1)**
4. **Verify multiburst generates correct frequencies and amplitudes**
5. **Add proper rise/fall time filtering (Thomson filter simulation)**
6. **Verify all amplitude calculations match 0.70V spec (0xE000)**

