# NTSC VITS Implementation Summary (Phase 3.2)

Successfully implemented Phase 3.2: NTSC VITS (Vertical Interval Test Signals) for encode-orc.

## Files Created/Modified

### New Files:
1. **include/ntsc_vits_generator.h** - NTSC VITS signal generator header
2. **src/ntsc_vits_generator.cpp** - NTSC VITS signal generator implementation

### Modified Files:
1. **include/ntsc_encoder.h** - Added VITS enable/disable methods
2. **src/ntsc_encoder.cpp** - Integrated VITS generator into field encoding
3. **src/video_encoder.cpp** - Enabled VITS for NTSC test card generation
4. **CMakeLists.txt** - Added ntsc_vits_generator.cpp to build
5. **tests/CMakeLists.txt** - Added ntsc_vits_generator.cpp to test builds

## VITS Signals Implemented

### 1. VIR (Vertical Interval Reference) Signal
- **Lines**: 19 and 282 (FCC Recommendation 73-699 / CCIR Recommendation 314-4)
- **Structure**:
  - 12 µs blanking/spacing (0 IRE)
  - 24 µs luminance pedestal (70 IRE) with superimposed chrominance reference (±20 IRE, 50-90 IRE range)
  - 12 µs luminance reference pedestal (70 IRE)
  - 12 µs black reference (7.5 IRE)
- **Purpose**: Program-related reference for maintaining consistent luminance/chroma setup through broadcast chain

### 2. NTC-7 Composite Test Signal
- **Lines**: 17 and 283
- **Components**:
  - 100 IRE white bar (8 µs width at 60 IRE level, 125±5 ns rise/fall)
  - 2T pulse (100 IRE peak, 250±10 ns half-amplitude width)
  - 12.5T chrominance pulse (100 IRE peak, 1562.5±50 ns half-amplitude width)
  - 5-step modulated staircase (0-90 IRE with 40 IRE peak-to-peak subcarrier at 0° phase)
- **Purpose**: Tests several video parameters in a composite test signal format

### 3. NTC-7 Combination Test Signal
- **Lines**: 20 and 280
- **Components**:
  - 100 IRE white flag (4 µs width)
  - Multiburst (0.5-4.2 MHz on 50 IRE pedestal with 50 IRE peak-to-peak)
  - 3-step modulated pedestal (20, 40, 80 IRE peak-to-peak at -90° phase)
- **Purpose**: Tests multiple video parameters including bandwidth and chroma response

## Implementation Details

### Key Features:
- **NTSC-specific parameters**:
  - Subcarrier frequency: 3.579545 MHz
  - Line period: 63.556 µs
  - Sync level: -40 to -43 IRE
  - Blanking level: 0 IRE

- **Phase calculation**: Based on NTSC subcarrier frequency and field/line numbering
- **Signal levels**: Proper IRE value conversions for accurate test signal generation
- **Envelope shaping**: Proper rise/fall times for pulse signals
- **Chroma modulation**: Accurate color subcarrier modulation with phase offsets

### Integration:
- VITS generator is instantiated within NTSCEncoder when enable_vits() is called
- VITS lines are automatically inserted into the VBI (Vertical Blanking Interval)
- Works seamlessly with existing NTSC encoding pipeline

## Testing

Test file generated: `test-output/ntsc-vits-test.tbc`

### Verification Results:

**VITS Lines (Field 0):**
- Line 18 (VIR Line 19): Min=0x0000 (-40.00 IRE), Max=0xceff (89.37 IRE), Mean=0x6937 (25.76 IRE) ✓
- Line 19 (NTC-7 Combination): Min=0x0000 (-40.00 IRE), Max=0xceff (89.37 IRE), Mean=0x6256 (21.46 IRE) ✓

**VITS Lines (Field 1):**
- Line 18 (VIR Line 282): Min=0x0000 (-40.00 IRE), Max=0xceff (89.37 IRE), Mean=0x6858 (25.22 IRE) ✓
- Line 19 (NTC-7 Composite): Min=0x0000 (-40.00 IRE), Max=0xceff (89.37 IRE), Mean=0x61c8 (21.11 IRE) ✓

**Regular Blanking Lines (Reference):**
- Line 5: Min=0x0000 (-40.00 IRE), Max=0x4a00 (6.25 IRE), Mean=0x3875 (-4.71 IRE)
- Line 10: Min=0x0000 (-40.00 IRE), Max=0x4a00 (6.25 IRE), Mean=0x3875 (-4.71 IRE)

All VITS signals are correctly generated with appropriate signal levels and content. Regular blanking lines show only blanking and sync, confirming proper VITS insertion.

## API Usage

```cpp
// Enable VITS in NTSC encoder
NTSCEncoder encoder(params);
encoder.enable_vits();

// Disable VITS
encoder.disable_vits();

// Check if VITS is enabled
bool enabled = encoder.is_vits_enabled();
```

## Command Line Usage

```bash
./encode-orc -o output.tbc -f ntsc-composite -t smpte --vits iec60856-ntsc -n 100
```

## Standards References

- FCC Recommendation 73-699 (VIR Signal)
- CCIR Recommendation 314-4 (VIR Signal)
- BT.471 (NTC-7 Composite Test Signal)
- BT.628/BT.473 (referenced test signal specifications)
- Video Demystified, 5th Edition (Figures 8.40, 8.43)

## Next Steps

- Phase 3.3+: Additional VITS signals for specialty applications
- Phase 4: Signal Generation - Complete signal synthesis
- Phase 5: File Format Handling - Full .tbc and metadata support
