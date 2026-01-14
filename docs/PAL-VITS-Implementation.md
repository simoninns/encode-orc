# PAL VITS Implementation Summary (Phase 3.1)

## Overview
Successfully implemented Phase 3.1: PAL VITS (Vertical Interval Test Signals) for encode-orc.

## Implementation Details

### Files Created
1. **include/pal_vits_generator.h** - PAL VITS signal generator header
2. **src/pal_vits_generator.cpp** - PAL VITS signal generator implementation

### Files Modified
1. **include/pal_encoder.h** - Added VITS enable/disable methods
2. **src/pal_encoder.cpp** - Integrated VITS generator into field encoding
3. **src/video_encoder.cpp** - Enabled VITS for PAL test card generation
4. **CMakeLists.txt** - Added pal_vits_generator.cpp to build
5. **tests/CMakeLists.txt** - Added pal_vits_generator.cpp to test builds

## VITS Signals Implemented

Based on **Video Demystified, 5th Edition** (ISBN 978-0-750-68395-1):

### First Field (Lines in VBI)
1. **Line 19**: ITU Composite Test Signal (Figure 8.41, BT.628/BT.473)
   - White flag: 100 IRE, 10 µs (12-22 µs)
   - 2T pulse: 100 IRE, centered at 26 µs
   - 5-step modulated staircase: 42.86 IRE subcarrier at 60° phase
   - Steps at 0, 20, 40, 60, 80, 100 IRE (30-60 µs)

2. **Line 20**: ITU Combination ITS Test Signal (Figure 8.45)
   - 3-step modulated pedestal: 20, 60, 100 IRE peak-to-peak
   - Extended subcarrier packet: 60 IRE peak-to-peak
   - Phase: 60° relative to U axis

### Second Field (Lines in VBI)
3. **Line 332**: UK PAL National Test Signal #1 (Figure 8.42)
   - White flag: 100 IRE, 10 µs (12-22 µs)
   - 2T pulse: 100 IRE, centered at 26 µs
   - 10T chrominance pulse: 100 IRE, centered at 30 µs
   - 5-step modulated staircase: 21.43 IRE subcarrier at 60° phase

4. **Line 333**: ITU Multiburst Test Signal (Figure 8.38)
   - White flag: 80 IRE, 4 µs
   - Six frequency packets: 0.5, 1.0, 2.0, 4.0, 4.8, 5.8 MHz
   - 50 IRE pedestal with 60 IRE peak-to-peak

## Technical Features

### Signal Generation Functions
- `ire_to_sample()` - Convert IRE levels to 16-bit samples
- `calculate_phase()` - Phase calculation for PAL 8-field sequence
- `get_v_switch()` - PAL V-switch calculation
- `generate_sync_pulse()` - Horizontal sync generation
- `generate_color_burst()` - Color burst with PAL swinging burst
- `generate_flat_level()` - Flat luminance levels
- `generate_2t_pulse()` - 2T pulse (200ns half-amplitude)
- `generate_10t_pulse()` - 10T chrominance pulse
- `generate_modulated_staircase()` - Modulated staircase with chroma
- `generate_modulated_pedestal()` - Modulated pedestal
- `generate_multiburst_packet()` - Multiburst frequency packets

### PAL-Specific Implementation
- Correct PAL V-switch handling for all signals
- Phase calculation using PAL 8-field sequence
- 283.7516 subcarrier cycles per line
- Swinging color burst (±135° phase)
- Proper chroma modulation at 60° to U axis

## Verification

Test file generated: `test-output/pal-vits-test.tbc`

Verification results (using verify_vits.py):
- **Field 0, Line 18 (Line 19)**: Max 753mV ✓ (contains 100 IRE white reference)
- **Field 0, Line 19 (Line 20)**: Max 713mV ✓ (contains modulated pedestals)
- **Field 1, Line 18 (Line 332)**: Max 753mV ✓ (contains white reference and 10T pulse)
- **Field 1, Line 19 (Line 333)**: Max 602mV ✓ (contains multiburst packets)
- **Blanking line**: Max 126mV ✓ (only sync and burst, no test signals)

All VITS signals are correctly generated with appropriate signal levels.

## Build Status
✅ Compiles cleanly with no warnings
✅ All tests pass
✅ Integration with PAL encoder successful

## API Usage

```cpp
// Enable VITS in PAL encoder
PALEncoder encoder(params);
encoder.enable_vits();

// Disable VITS
encoder.disable_vits();

// Check if enabled
bool enabled = encoder.is_vits_enabled();
```

## Next Steps (Future Phases)

- Phase 3.2: NTSC VITS implementation (VIR, NTC-7, etc.)
- CLI option to enable/disable VITS
- Additional VITS standards (IEC 60856-1986 variations)
- VITS signal validation tools

## References

- Video Demystified, 5th Edition - Keith Jack (ISBN 978-0-750-68395-1)
- ITU-R BT.628 - Composite Test Signal for PAL
- ITU-R BT.473 - ITS for PAL Systems
- IEC 60856-1986 - LaserDisc Standard

---

**Date**: 14 January 2026
**Phase**: 3.1 - PAL VITS
**Status**: ✅ Complete
