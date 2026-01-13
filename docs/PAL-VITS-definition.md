# PAL VITS (Vertical Interval Test Signal) Definition

## Overview

VITS (Vertical Interval Test Signal) are standardized test signals inserted in the vertical blanking interval (VBI) of composite video to allow for objective measurement and verification of video signal quality and characteristics. Two primary standards define PAL VITS:

1. **IEC 60856-1986**: "Guide to the Use of the LaserDisc System" - defines LaserDisc-specific VITS requirements
2. **ITU-T J.63**: "General characteristics of test signals for the measurement of the performance of video telephone systems" - defines generic PAL VITS requirements

The main differences between these standards are:
- **LaserDisc (IEC 60856-1986)**: More comprehensive test signals optimized for LaserDisc mastering and quality control
- **ITU-T J.63**: Simpler, more commonly used signals for general video transmission testing

---

## PAL Video Standards Reference

### PAL Field Structure
```
Field (313 lines total):
  Lines 1-5:        Vertical Sync
  Lines 6-22:       Vertical Blanking Interval (VBI) / VITS area
  Lines 23-310:     Active Video
  Line 311-313:     Field blanking (second field only in interlaced video)
```

### PAL Timing Parameters
- **Subcarrier Frequency**: 4.433618.75 MHz
- **Sample Rate**: 17.734475 MHz (4× subcarrier)
- **Field Width**: 1135 samples (at 4× subcarrier)
- **Active Video Region**: Samples 185-1107 (923 pixels)
- **Horizontal Blanking**: 184 samples (leading), 28 samples (trailing)
- **Color Burst**: Samples 98-138 (41 samples, 10 cycles)
- **Sync Pulse**: ~74 samples at 0x0000 (start of line)

### PAL Signal Levels (16-bit composite)
```
Sync Level:     0x0000  (-300mV in analog)
Blanking Level: 0x4000  (  0mV in analog)
Black Level:    0x4000  (  0mV in analog)
White Level:    0xD300  (700mV in analog)
Maximum Peak:   0xE000 (~812mV in analog)
```

---

## VITS Test Signals - Definition and Technical Details

### Signal Category: Reference Signals (Used in both standards)

#### 1. White Reference (100 IRE)
- **Purpose**: Measures white level, luminance response, and signal linearity
- **Composition**: Pure white (no chroma) at 100 IRE (700mV)
- **Y Level**: 1.0 (maximum luminance)
- **U/V Level**: 0.0 (no chroma, centered on 0V)
- **Duration**: Typically ~1.0 µs active duration (line dependent)
- **Appearance**: Flat white level that should measure consistently across frequency domain

#### 2. 75% Gray Reference (75 IRE)
- **Purpose**: Luminance linearity reference point
- **Composition**: Gray at 75 IRE (~525mV)
- **Y Level**: 0.75
- **U/V Level**: 0.0
- **Application**: Mid-scale reference for linearity testing
- **IEC 60856-1986**: Included on specific lines for LaserDisc compliance
- **ITU-T J.63**: Optional, may be included as part of signal sequences

#### 3. 50% Gray Reference (50 IRE)
- **Purpose**: Mid-gray reference, baseline for noise measurements
- **Composition**: Gray at 50 IRE (~350mV)
- **Y Level**: 0.5
- **U/V Level**: 0.0
- **Noise Floor Reference**: Used to establish signal-to-noise baseline

#### 4. Black Reference (0 IRE)
- **Purpose**: Black level verification, noise floor reference
- **Composition**: Pure black at 0 IRE (blanking level)
- **Y Level**: 0.0
- **U/V Level**: 0.0 (no chroma)
- **Note**: Same level as blanking (0V), useful for noise measurements

---

### Signal Category: Color References (Both standards)

#### 5. Color Burst (Reference Burst)
- **Purpose**: Chroma reference for color demodulation and phase/amplitude measurement
- **Composition**: ±300mV peak chroma at reference phase
- **Frequency**: 4.433618.75 MHz (PAL subcarrier)
- **Phase**: 135° reference phase (standard PAL burst phase)
- **Amplitude**: Approximately ±300mV (peak-to-peak ~600mV)
- **Duration**: 10 cycles of subcarrier (~2.26 µs)
- **V-Switch**: Polarity may be inverted based on PAL V-switch pattern
- **Standard Position**: Multiple lines in VBI contain burst signal
- **Function**: Allows decoders to lock phase and measure chroma signal amplitude

#### 6. Primary Color Bars
- **Purpose**: Full color gamut verification
- **Composition**: SMPTE color bars (white, yellow, cyan, green, magenta, red, blue, black)
- **Each Bar Width**: Typically ~100-150 mV for modulation
- **U/V Modulation**: Full ±300mV chroma for each color
- **Application**: ITU-T J.63 includes color bars as standard test pattern
- **Measurement**: Color accuracy, hue error, saturation
- **Note**: May be optionally included in certain lines

---

### Signal Category: Frequency Response and Bandwidth

#### 7. Multiburst Signal
- **Purpose**: Frequency response measurement across multiple frequencies
- **Composition**: Series of short bursts at different frequencies:
  - 0.5 MHz
  - 1.0 MHz
  - 2.0 MHz
  - 3.0 MHz (often 3.58 MHz for NTSC equivalent)
  - 4.2 MHz
  - Higher frequencies as applicable
- **Amplitude**: Typically 50 IRE (half amplitude of full signal)
- **Duration**: ~0.2-0.5 µs per burst segment
- **Measurement**: Channel bandwidth, group delay, phase linearity
- **IEC 60856-1986**: Included on specific VBI line (e.g., Line 20)
- **ITU-T J.63**: May be optional or included for comprehensive testing

#### 8. Staircase Signal
- **Purpose**: Differential linearity and luminance step response
- **Composition**: Multiple discrete luminance levels in ascending order
- **Levels**: Typically 6-8 steps (0 IRE, ~14 IRE, ~28 IRE, ... 100 IRE)
- **Amplitude per Step**: Even increments to verify linearity
- **Rise Time**: Measures transient response and ringing
- **Application**: Detects differential non-linearity, gain compression
- **IEC 60856-1986**: Common on LaserDisc VITS lines
- **ITU-T J.63**: Often included as standard reference

---

### Signal Category: Chroma-Related Tests

#### 9. Chroma Modulation Reference
- **Purpose**: Chroma amplitude and phase response measurement
- **Composition**: Modulated chroma at specified levels (typically 50% or 100%)
- **Amplitude**: ±150 mV or ±300 mV depending on level
- **Phase**: Variable or at reference phase
- **Measurement**: Chroma-to-Luma delay, chroma phase response
- **IEC 60856-1986**: Specific modulation sequences defined
- **ITU-T J.63**: Simpler modulation schemes

#### 10. PAL V-Switch Verification Signal
- **Purpose**: Verify proper PAL V-switch (line alternation) functionality
- **Composition**: Two lines with opposite chroma phase due to V-switch
- **Expected Difference**: 180° phase shift between consecutive lines
- **Application**: Confirms V-switch implementation in encoder
- **Measurement**: Automatic V-switch detection and verification
- **Note**: Critical for PAL color demodulation accuracy

---

### Signal Category: Timing and Sync

#### 11. Sync Pulse Reference
- **Purpose**: Verify horizontal sync pulse timing and amplitude
- **Composition**: Standard H-sync pulse at ~4.7 µs duration
- **Amplitude**: 300mV (from blanking to sync level, 0x4000 to 0x0000)
- **Timing**: Standard PAL horizontal timing
- **Measurement**: Sync pulse duration, rise time, over/undershoot
- **Location**: Present on all VITS lines

#### 12. Vertical Sync Pattern
- **Purpose**: Verify field synchronization timing
- **Composition**: Standard PAL vertical sync pulses (broad + narrow pattern)
- **Duration**: Lines 1-5 of field
- **Pattern**: Specific broad pulse and narrow pulse timing for field identification
- **Measurement**: Field lock, vertical timing accuracy

---

### Signal Category: Specialized Tests (LaserDisc Specific - IEC 60856-1986)

#### 13. Insertion Test Signal (ITS)
- **Purpose**: LaserDisc playback head stability and timing verification
- **Composition**: Specialized timing reference for optical media
- **Application**: LD player servo system validation
- **IEC 60856-1986**: Pages 20-21 detail ITS requirements
- **ITU-T J.63**: N/A (not applicable to transmission media)

#### 14. Cross-Color Distortion Measurement
- **Purpose**: Detect cross-color (chroma-to-luma) artifacts
- **Composition**: Chroma signal overlaid on luminance steps
- **Measurement**: Cross-color suppression effectiveness
- **IEC 60856-1986**: Included for mastering quality control

#### 15. Differential Gain and Phase Measurement
- **Purpose**: Monitor chroma gain and phase errors at different luminance levels
- **Composition**: Chroma signal at multiple luminance backgrounds (0%, 25%, 50%, 75%, 100%)
- **Measurement**: Gain variation ≤ ±20% (typical spec), phase variation ≤ ±5°
- **Application**: Quality control for playback and transmission

---

## VITS Line Allocation Standards

### LaserDisc (IEC 60856-1986) VITS Line Allocation
**Reference: IEC 60856-1986, Pages 20-21**

```
Field 1 (Odd Field):
  Line 6  : [Color Burst + Reference Signals]
  Line 7  : [Reserved/Test Signal 1]
  Line 8  : [Reserved/Test Signal 2]
  Line 9  : [Color Burst + Insertion Test Signal (ITS)]
  Line 10 : [Multiburst or Frequency Response]
  Line 11 : [Staircase Signal]
  Line 12 : [White Reference/100 IRE]
  Line 13 : [Color Bars or Reference Chroma]
  Line 14 : [Reserved]
  Line 15 : [Reserved]
  Line 16 : [Differential Gain/Phase]
  Line 17 : [Cross-Color Reference]
  Line 18 : [Vertical Sync]
  Line 19 : [Vertical Sync]
  Line 20 : [Multiburst Alternative/Blanking]
  Line 21 : [Blanking]
  Line 22 : [Blanking/Burst]

Field 2 (Even Field):
  Lines 6-22: Mirror of Field 1 (with V-switch applied to chroma)
```

**Key Characteristics:**
- Comprehensive test signal set for mastering and quality control
- Multiple reference signals on adjacent lines for comparison
- Specific allocation for LaserDisc-specific tests (ITS)
- Includes differential measurements (gain/phase at different luminance levels)
- Both fields contain VITS (important for field-based quality assessment)

---

### Generic PAL (ITU-T J.63) VITS Line Allocation
**Reference: ITU-T J.63**

```
Field 1 (Odd Field):
  Line 6  : [Blanking/Color Burst]
  Line 7  : [Staircase Signal or Gray Reference]
  Line 8  : [Multiburst Signal]
  Line 9  : [Blanking/Color Burst]
  Line 10 : [Optional: Color Reference/Bars]
  Line 11 : [Optional: Reserved]
  ...
  Line 22 : [Blanking/Color Burst]

Field 2 (Even Field):
  Line 6  : [Blanking/Color Burst]
  Line 7  : [Staircase Signal or Gray Reference]
  Line 8  : [Multiburst Signal]
  ...
  Line 22 : [Blanking/Color Burst]
```

**Key Characteristics:**
- Simpler, more flexible allocation than LaserDisc
- Core signals: Color burst, staircase, multiburst
- Allows optional signals depending on transmission requirements
- Better suited for general video transmission (broadcast, cable, etc.)
- Color burst appears on multiple lines for phase stability
- Typically one field contains primary VITS (field 1 or specific odd field)

---

## Compliance Requirements Summary

### LaserDisc (IEC 60856-1986) Compliance

**Mandatory VITS Signals:**
| Signal | Lines (Field 1) | Lines (Field 2) | Purpose |
|--------|-----------------|-----------------|---------|
| Color Burst | 6, 9, 22 | 6, 9, 22 | Phase/amplitude reference |
| Staircase | 11 | 11 | Linearity verification |
| White Reference | 12 | 12 | White level/frequency response |
| Multiburst | 10, 20 | 10, 20 | Frequency response |
| ITS | 9 | 9 | LaserDisc playback timing |

**Optional VITS Signals:**
- Color bars (line 13)
- Differential gain/phase (line 16)
- Cross-color reference (line 17)

**Total VITS Lines per Field:** 17 lines (6-22)
**Quality Control Focus:** Mastering and optical media playback verification

---

### ITU-T J.63 Compliance

**Mandatory Core VITS Signals:**
| Signal | Lines | Purpose |
|--------|-------|---------|
| Color Burst | 6, 22 | Phase/amplitude reference |
| Staircase or Gray Ref | 7 | Luminance linearity |
| Multiburst | 8 | Frequency response |

**Optional VITS Signals:**
- Color bars/references (lines 10-11)
- Additional reference signals
- Extended test sequences

**Total VITS Lines per Field:** 3-5 lines minimum (6-22 available)
**Quality Control Focus:** Video transmission quality and compatibility

---

## Implementation Architecture Recommendations

To support clean separation between test signal generation and line allocation:

### 1. Test Signal Generator Module
**Responsibility**: Generate individual VITS test signals

```
class VITSSignalGenerator {
  // Reference signals
  - generateWhiteReference() → composite_line
  - generate75GrayReference() → composite_line
  - generateBlackReference() → composite_line
  
  // Color references
  - generateColorBurst() → composite_line
  - generateColorBars() → composite_line
  
  // Frequency response
  - generateMultiburst(frequencies) → composite_line
  - generateStaircase(levels) → composite_line
  
  // Chroma-specific
  - generateChromaModulation() → composite_line
  - generateVSwitchPattern() → pair_of_lines
  
  // LaserDisc-specific
  - generateInsertionTestSignal() → composite_line
  - generateDifferentialGainPhase() → composite_line
}
```

### 2. VITS Line Allocator Module
**Responsibility**: Map VITS signals to specific lines based on standard

```
class VITSLineAllocator {
  // Standard definitions
  - getLineAllocation(standard: "IEC60856" | "ITUTN63") → allocation_map
  - getSignalsForLine(line_number, standard) → signal_list
  
  // Flexible configuration
  - customizeAllocation(line, signal, standard) → void
  - validateAllocation(allocation) → bool
  
  // Query interface
  - isVITSLine(line_number, standard) → bool
  - getSignalDescription(line_number) → string
}
```

### 3. VITS Composition Module
**Responsibility**: Combine signals and allocations into complete fields

```
class VITSComposer {
  - composeField(field_data, standard, line_allocator) → field
  - insertVITS(field, line_num, signal) → field
  - validateVITSCompliance(field, standard) → compliance_report
}
```

---

## Key Design Decisions

1. **Separation of Concerns**: Signal generation logic completely separate from line allocation
   - Allows easy addition of new test signals without affecting line allocation
   - Permits reuse of signals across different standards
   - Simplifies testing of individual signals

2. **Standard-Agnostic Signals**: Each signal definition is implementation-agnostic
   - Can be applied to any line number
   - Easy to create custom allocations for experimentation

3. **Configuration Over Hard-Coding**: Line allocation driven by standard definitions
   - Simple to add new standards or variations
   - User can specify compliance target (LaserDisc, J.63, custom)

4. **Parameter Flexibility**: Signals accept parameters for tuning
   - Amplitude levels configurable
   - Frequency selections variable
   - Timing adjustments supported

---

## References

1. **IEC 60856-1986**: "Guide to the Use of the LaserDisc System"
   - Pages 20-21: PAL LaserDisc VITS requirements
   - Contains detailed specifications for LaserDisc-specific test signals

2. **ITU-T J.63**: "General Characteristics of Test Signals for Measurement of Performance of Video Telephone Systems"
   - Defines generic PAL VITS for video transmission
   - Includes both mandatory and optional test signals

3. **ITU-R BT.601**: "Studio Encoding Parameters of Digital Television for Standard 4:3 and Wide-screen 16:9 Aspect Ratios"
   - Color encoding reference (already implemented in PAL encoder)

4. **IEC 61937**: "Digital audio interface - Transmission of non-PCM audio bitstreams"
   - Additional timing reference standards

---

## Next Steps

1. Implement `VITSSignalGenerator` with each test signal type
2. Implement `VITSLineAllocator` with both IEC 60856-1986 and ITU-T J.63 definitions
3. Integrate with existing PAL encoder (`pal_encoder.cpp`)
4. Create test suite validating each VITS signal generation
5. Validate compliance against specification documents
6. Add CLI options for VITS standard selection
