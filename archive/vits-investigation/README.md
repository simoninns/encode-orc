# VITS Investigation Archive

This directory contains all VITS (Vertical Interval Test Signal) related development work that has been parked to focus on Phase 2.1 implementation (EBU/EIA color bars).

## Contents

### Source Code
- `pal_vits_signal_generator.cpp/h` - PAL VITS signal generation
- `vits_composer.cpp/h` - VITS composition and timing
- `vits_line_allocator.h` - Line allocation for VITS data
- `pal_laserdisc_line_allocator.cpp` - PAL LaserDisc line allocation

### Test Files
- `test_vits_pal.cpp` - PAL VITS generation tests
- `test_vits_debug.cpp` - VITS debugging utilities
- `debug_vits_burst.cpp` - Burst signal debugging
- `verify_vits.cpp` - VITS verification utility
- `generate-iec60856-vits-test.sh` - Shell script for IEC 60856 VITS test generation

### Analysis & Documentation
- `VITS_ISSUES.md` - Issues and problems found in VITS implementation
- `analyze_vits_timing.py` - Python script for VITS timing analysis
- `verify_vits_lines.py` - Python script for VITS line verification
- `analyze_line19.py` - Analysis of specific VITS line 19

## Why Archived?

This work was put on hold to prioritize the new implementation plan which focuses on:
1. **Phase 2.1**: EBU color bars for PAL and EIA color bars for NTSC
2. **Phase 2.3**: 24-bit biphase encoding for frame numbers and timecode
3. **Phase 3**: VITS implementation (PAL then NTSC)

VITS development will resume in Phase 3 with the insights gained from the previous investigation preserved here.

## How to Restore

To reintegrate any of these components, copy them back to their original locations:
- `.cpp`/`.h` files from this directory to `../src/` and `../include/`
- Test files from this directory to `../tests/`
- Update `tests/CMakeLists.txt` to reference the restored files
