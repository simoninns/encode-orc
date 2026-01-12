# Reference Implementation Analysis

This document contains research findings from Phase 1.2 of the implementation plan, analyzing the ld-chroma-encoder source code and related file formats from the ld-decode project.

## ld-chroma-encoder Overview

**Source Location**: https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder/encoder

**License**: GPLv3+

**Purpose**: A simplistic PAL/NTSC encoder for decoder testing, prioritizing accuracy over performance.

**Authors**: Adam Sampson, Phillip Blucas (2019-2022)

### Architecture

The encoder uses an object-oriented design with:
- **Base Class**: `Encoder` - Common functionality for all encoding
- **PAL Implementation**: `PALEncoder` - PAL-specific encoding
- **NTSC Implementation**: `NTSCEncoder` - NTSC-specific encoding

### Key Components

1. **encoder.cpp/h** - Base encoder class with common functions
2. **palencoder.cpp/h** - PAL-specific encoding implementation
3. **ntscencoder.cpp/h** - NTSC-specific encoding implementation
4. **main.cpp** - Command-line interface and entry point
5. **firfilter.h** - FIR filter implementation for chroma filtering

## Video Signal Generation

### PAL Encoding

#### Subcarrier Frequency
- **fSC**: 4,433,618.75 Hz (PAL standard)
- **Sample Rate**: 4 × fSC = 17,734,475 Hz

#### Sampling Modes

**Subcarrier-Locked Sampling** (default for ld-chroma-encoder):
- Frame size: (1135 × 625) + 4 samples
- Field structure:
  - Field 1: 1135 × 313 lines + 2 samples
  - Field 2: 1135 × 312 lines + 2 samples
  - 1131 padding samples
- Each 64 μs line is 1135 + (4/625) samples long
- Everything shifts right by 4/625 samples per line
- Color burst sampled at 0°, 90°, 180°, 270° → [95.5, 64, 32.5, 64] × 0x100

**Line-Locked Sampling** (ld-decode typical output):
- Fixed samples per line
- Color burst start: sample 98
- Color burst end: sample 138
- Active video: samples 185-1107

#### Video Parameters
- Field width: 1135 pixels
- Field height: 313 lines
- Active width: 928 pixels (centered in active area)
- Active top: line 44
- Active height: 576 lines (620 - 44)
- White level (16-bit): 0xD300
- Black level (16-bit): 0x4000

#### Color Encoding
PAL uses Y'UV color space:

```
Y = 0.299*R + 0.587*G + 0.114*B
U = -0.147141*R - 0.288869*G + 0.436010*B
V = 0.614975*R - 0.514965*G - 0.100010*B
```

Chroma components are low-pass filtered to 1.3 MHz.

#### Subcarrier Modulation
The PAL chroma signal uses alternating V phase (V-switch):

```cpp
// V-switch state on each line
Vsw = (prevLines % 2) == 0 ? 1.0 : -1.0

// Subcarrier phase calculation
prevCycles = prevLines * 283.7516  // Cycles at 0H
a = 2π * ((fSC * t) + prevCycles)  // Current phase

// Color burst phase
burstOffset = Vsw * 135° * π/180

// Chroma encoding [Poynton p338]
chroma = U*sin(a) + V*cos(a)*Vsw
```

#### Sync and Blanking
- 0H (horizontal sync reference) shifts by (4/625) samples per line in subcarrier-locked mode
- Burst amplitude suppressed on certain lines (619-625, specific VBI lines)
- Uses raised cosine gates for burst and chroma to avoid sharp transitions

### NTSC Encoding

#### Subcarrier Frequency
- **fSC**: 315.0e6 / 88.0 Hz ≈ 3,579,545.45 Hz
- **Sample Rate**: 4 × fSC = 14,318,181.82 Hz

#### Frame Structure
- Frame size: 910 × 526 samples (last line ignored)
- Field width: 910 pixels
- Field height: 263 lines
- Line length: 63.555 μs = 910 samples
- Active width: 758 pixels
- Active top: line 39
- Active height: 486 lines (525 - 39)

#### Video Levels
- White level (16-bit): 0xC800
- Blanking level: 0x3C00
- Black level: blanking + setup (if enabled)
- Setup offset: 0x0A80 (10.5 IRE, optional for NTSC-J)

#### Color Encoding Modes

**Wideband Y'UV**:
```
Y = 0.299*R + 0.587*G + 0.114*B
U = -0.147141*R - 0.288869*G + 0.436010*B
V = 0.614975*R - 0.514965*G - 0.100010*B
```

**Wideband Y'IQ**:
```
Y = 0.299*R + 0.587*G + 0.114*B
I = 0.595901*R - 0.274557*G - 0.321344*B
Q = 0.211537*R - 0.522736*G + 0.311200*B
```

Y'IQ is rotated 33° from Y'UV:
```
I = -sin(33°)*U + cos(33°)*V
Q = cos(33°)*U + sin(33°)*V
```

**Narrowband Q**: Same as Y'IQ but Q component filtered to narrower bandwidth.

#### Subcarrier Modulation

**For Y'UV**:
```cpp
chroma = U*sin(a) + V*cos(a)
```

**For Y'IQ**:
```cpp
chroma = C2*sin(a + 33°) + C1*cos(a + 33°)
where C1=I, C2=Q
```

#### Color Burst
- Starts 19 cycles after 0H
- Duration: 9 cycles
- Sampled at -33°, 57°, 123°, 213° → [46, 83, 74, 37] × 0x100

#### 0H Reference
- 0H occurs 33/90 of the way between samples 784 and 785 on field 1, line 1
- First TBC sample is first blanking sample of field 1, line 1

### Common Encoding Pipeline

Both PAL and NTSC follow this general structure:

1. **Input Processing**
   - Read RGB48 or YUV444P16 frame data
   - Two fields per frame (interlaced)
   
2. **Color Space Conversion**
   - RGB → Y'UV (PAL) or Y'UV/Y'IQ (NTSC)
   - Normalize to 0.0-1.0 range

3. **Chroma Filtering**
   - Low-pass filter U and V to 1.3 MHz
   - Uses FIR filter implementation

4. **Line-by-Line Encoding**
   - Calculate subcarrier phase for current line
   - Generate color burst
   - Modulate chroma signal
   - Combine luma, chroma, burst, and sync
   - Apply raised cosine gates for smooth transitions

5. **Output Generation**
   - Composite output (VBS): luma + sync + blanking
   - Chroma output (C): chroma + burst
   - Or combined composite signal

6. **16-bit Quantization**
   - Scale signals to 16-bit range
   - Clamp to valid IRE levels
   - For separate chroma: center on 0x7FFF

## TBC File Format

### Structure

**.tbc files** contain raw field-based composite video data:
- One field per block
- 16-bit unsigned samples
- Little-endian byte order
- No header information (metadata in separate .tbc.db file)

### Field Organization

**PAL** (subcarrier-locked):
- Field 1: 1135 × 313 samples + 2 samples
- Field 2: 1135 × 312 samples + 2 samples
- Plus 1131 padding samples per frame

**NTSC**:
- Each field: 910 × 263 samples
- Frame: 910 × 526 samples (last line ignored)

### Sample Encoding

Each sample is a 16-bit unsigned integer representing the composite video signal level:
- Sync level: lowest values
- Blanking level: ~0x3C00 (NTSC) or ~0x4000 (PAL)
- Black level: blanking + setup (NTSC only)
- White level: 0xC800 (NTSC) or 0xD300 (PAL)

### Y/C (S-Video) Format

When encoding to separate Y/C:
- **.tbcy** (luma): Contains Y signal + sync + blanking
- **.tbcc** (chroma): Contains chroma + burst, centered on 0x7FFF
- Both use same 16-bit unsigned format

## SQLite Metadata Format (.tbc.db)

### Overview

The .tbc.db file is a SQLite database containing comprehensive metadata about the TBC video capture. It uses schema version 1 (PRAGMA user_version = 1).

### Database Schema

#### 1. capture table
Capture-level metadata (one record per TBC file):

**Key Fields**:
- `capture_id` (INTEGER, PK): Unique identifier
- `system` (TEXT): "PAL", "NTSC", or "PAL_M"
- `decoder` (TEXT): "ld-decode" or "vhs-decode"
- `git_branch`, `git_commit` (TEXT): Decoder version info
- `video_sample_rate` (REAL): Sample rate in Hz (typically 4 × fSC)
- `active_video_start`, `active_video_end` (INTEGER): Active region bounds in pixels
- `field_width`, `field_height` (INTEGER): Field dimensions
- `number_of_sequential_fields` (INTEGER): Total field count
- `colour_burst_start`, `colour_burst_end` (INTEGER): Burst position in pixels
- `is_mapped` (BOOLEAN): Whether processed by ld-discmap
- `is_subcarrier_locked` (BOOLEAN): Subcarrier vs line-locked sampling
- `is_widescreen` (BOOLEAN): 16:9 vs 4:3
- `white_16b_ire`, `black_16b_ire`, `blanking_16b_ire` (INTEGER): Signal levels

#### 2. pcm_audio_parameters table
Audio configuration (one record per capture, optional):

**Fields**:
- `capture_id` (INTEGER, FK): References capture
- `bits` (INTEGER): Bits per sample (typically 16)
- `is_signed`, `is_little_endian` (BOOLEAN): Format flags
- `sample_rate` (REAL): Audio sample rate in Hz

#### 3. field_record table
Field-level metadata (one record per field):

**Key Fields**:
- `capture_id` (INTEGER, FK): References capture
- `field_id` (INTEGER, PK): Zero-indexed field number (original JSON was 1-indexed)
- `audio_samples` (INTEGER): Audio samples for this field
- `decode_faults` (INTEGER): Bit flags (1=first field detect fail, 2=phase mismatch, 4=skip)
- `disk_loc` (REAL): File position in fields
- `field_phase_id` (INTEGER): Position in 4-field (NTSC) or 8-field (PAL) sequence
- `file_loc` (INTEGER): Sample number where field starts
- `is_first_field` (BOOLEAN): First or second field
- `median_burst_ire` (REAL): Median burst level
- `pad` (BOOLEAN): Field is padding (no valid video)
- `sync_conf` (INTEGER): Sync confidence 0-100

**NTSC-Specific Fields** (NULL for PAL):
- `ntsc_is_fm_code_data_valid` (BOOLEAN): FM code validity
- `ntsc_fm_code_data` (INTEGER): 20-bit FM code (X5-X1)
- `ntsc_field_flag` (BOOLEAN): First video field indicator
- `ntsc_is_video_id_data_valid` (BOOLEAN): Video ID validity
- `ntsc_video_id_data` (INTEGER): 14-bit IEC 61880 code
- `ntsc_white_flag` (BOOLEAN): White flag presence

#### 4. vits_metrics table (optional)
Video Insert Test Signal metrics:

**Fields**:
- `capture_id`, `field_id` (FK): References field_record
- `b_psnr` (REAL): Black line PSNR
- `w_snr` (REAL): White area SNR

#### 5. vbi table (optional)
Vertical Blanking Interval data:

**Fields**:
- `capture_id`, `field_id` (FK): References field_record
- `vbi0`, `vbi1`, `vbi2` (INTEGER): VBI lines 16-18 raw data (IEC 60857/60856)

#### 6. drop_outs table (optional)
RF dropout detection (multiple records per field):

**Fields**:
- `capture_id`, `field_id`, `field_line` (FK/PK): Location
- `startx`, `endx` (INTEGER, PK): Dropout pixel range

#### 7. vitc table (optional)
Vertical Interval Timecode:

**Fields**:
- `capture_id`, `field_id` (FK): References field_record
- `vitc0` through `vitc7` (INTEGER): 8 bytes of raw VITC data

#### 8. closed_caption table (optional)
Closed caption data (NTSC):

**Fields**:
- `capture_id`, `field_id` (FK): References field_record
- `data0`, `data1` (INTEGER): CC bytes (-1 if invalid, 0 if present but no data)

### Implementation Notes

1. **Field Indexing**: The database uses 0-based `field_id`, while original JSON format used 1-based `seqNo`
2. **Optional Tables**: Only `capture` and `field_record` are mandatory; others are created as needed
3. **Foreign Keys**: All tables use cascading deletes for data integrity
4. **Format Flexibility**: NULL values used for format-specific fields (e.g., NTSC fields are NULL in PAL captures)
5. **Version Control**: Schema version tracked via `PRAGMA user_version = 1`

## Encoding Algorithm Summary

### High-Level Flow

```
1. Initialize encoder with video system parameters
2. For each frame:
   a. Read RGB/YUV input data
   b. Convert to Y'UV or Y'IQ color space
   c. For each of two fields:
      - For each line in field:
        * Calculate sync and timing parameters
        * Generate color burst
        * Modulate chroma onto subcarrier
        * Combine luma + chroma + sync
        * Apply gating functions
        * Quantize to 16-bit
      - Write field to .tbc file
   d. Update metadata in .tbc.db
```

### Critical Parameters by Standard

| Parameter | PAL | NTSC |
|-----------|-----|------|
| Subcarrier freq | 4,433,618.75 Hz | ~3,579,545.45 Hz |
| Sample rate | 4 × fSC | 4 × fSC |
| Field width | 1135 px | 910 px |
| Field height | 313/312 lines | 263 lines |
| Lines per frame | 625 | 525 |
| Fields per frame | 2 | 2 |
| Active lines | 576 | 486 |
| Color space | Y'UV | Y'UV or Y'IQ |
| Chroma BW | 1.3 MHz | 1.3 MHz (Q can be narrower) |
| White 16-bit | 0xD300 | 0xC800 |
| Black 16-bit | 0x4000 | varies (setup dependent) |
| Blanking 16-bit | 0x4000 | 0x3C00 |

## Key Algorithms

### Raised Cosine Gate

Used for smooth signal transitions (chroma, luma, burst):

```cpp
double raisedCosineGate(double t, double startTime, double endTime, double halfRiseTime) {
    if (t < startTime - halfRiseTime) return 0.0;
    if (t > endTime + halfRiseTime) return 0.0;
    if (t < startTime + halfRiseTime) {
        // Rising edge
        double x = (t - startTime) / halfRiseTime;
        return 0.5 * (1.0 + cos(π * (x - 1.0)));
    }
    if (t > endTime - halfRiseTime) {
        // Falling edge
        double x = (t - endTime) / halfRiseTime;
        return 0.5 * (1.0 + cos(π * x));
    }
    return 1.0;  // Full amplitude in middle
}
```

### FIR Filter

Low-pass filter for chroma components:
- PAL: 1.3 MHz cutoff for both U and V
- NTSC: 1.3 MHz for I/U, optionally narrower for Q

## References from Source Code

The ld-chroma-encoder references:
- **[Poynton]**: "Digital Video and HDTV Algorithms and Interfaces" by Charles Poynton
- **[EBU]**: EBU Tech 3280 - PAL specifications
- **[SMPTE]**: SMPTE 170M - NTSC specifications
- **[IEC 60857-1986]**: VBI data format
- **[IEC 60856-1986]**: Teletext and VBI
- **[IEC 61880]**: NTSC Video ID
- **[ANSI/CTA-608]**: Closed captioning

## Implementation Recommendations

For encode-orc development:

1. **Start Simple**: Begin with blanking-level output (M0) before full encoding
2. **Reuse Algorithms**: The ld-chroma-encoder is GPL-licensed and can be studied/adapted
3. **Use VideoParameters**: Match ld-decode's VideoParameters structure exactly
4. **SQLite Integration**: Use the documented schema without modifications
5. **Testing**: Verify output with ld-decode tools for round-trip validation
6. **Color Space**: Implement both Y'UV and Y'IQ for maximum compatibility
7. **Precision**: Use double-precision floating point during encoding, quantize at output only

## Key Differences from ld-chroma-encoder

encode-orc should differ in:
- **Language**: C++17 (can use modern features)
- **Test Focus**: Emphasize test pattern generation over real video
- **Standalone**: No Qt dependencies (ld-chroma-encoder uses Qt)
- **CLI**: Different command-line interface design
- **Features**: Add dropout/noise simulation not in ld-chroma-encoder
