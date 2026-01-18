# encode-orc YAML Project Configuration Format

## Overview

The `encode-orc` application exclusively uses YAML configuration files to define video encoding projects. This format provides a flexible and maintainable way to configure complex video encoding tasks with LaserDisc metadata support.

## Design Goals

- **Declarative configuration**: Define complete video encoding projects in a single file
- **Multi-section support**: Allow different sections with varying standards and sources
- **LaserDisc metadata**: Support CLV/CAV timecodes, chapter numbers, and picture numbers
- **VBI/VITS control**: Fine-grained control over vertical blanking interval data
- **Reusability**: Projects can be version-controlled and shared
- **Self-documenting**: Clear project name and description fields

## Usage

```bash
./encode-orc project.yaml
```

The application requires exactly one argument: a valid YAML project file (`.yaml` or `.yml` extension).

## File Structure

### Top-Level Fields

```yaml
# Project metadata
name: "My Video Encoding Project"
description: "A multi-section PAL LaserDisc test disc with various test signals"

# Global output settings
output:
  filename: "output.tbc"
  format: "pal-composite"  # pal-composite, ntsc-composite, pal-yc, ntsc-yc
  mode: "combined"  # Optional: combined (default), separate-yc, separate-yc-legacy
  metadata_decoder: "encode-orc"  # Optional: decoder string in metadata (default: "encode-orc")
  
  # Optional: Override video signal levels (16-bit IRE scale)
  # Use this to customize blanking, black, and white levels for specific projects
  # Values should be in the 16-bit range (0-65535)
  video_levels:
    blanking_16b_ire: 17125   # Optional: blanking level (default: system-dependent)
    black_16b_ire: 17125      # Optional: black level (default: system-dependent)
    white_16b_ire: 54016      # Optional: white/peak level (default: system-dependent)

# When mode is "separate-yc" or "separate-yc-legacy", two files are produced:
# - <basename>.tbcy : luma
# - <basename>.tbcc : chroma
# The base name comes from output.filename without extension.
# Note: format can still be pal-composite/ntsc-composite; mode controls output splitting.
  
# LaserDisc / IEC project settings (applies to all sections)
laserdisc:
  standard: "iec60857-1986"  # iec60857-1986 (PAL), iec60856-1986 (NTSC), or "none"
  mode: "cav"                # cav, clv, picture-numbers, or none (must be consistent for all sections)

  # Implementation note:
  # - The project-level standard alone decides whether VBI and VITS are generated.
  #   * iec60857-1986 (PAL) → VBI+VITS for PAL
  #   * iec60856-1986 (NTSC) → VBI+VITS for NTSC
  #   * none → VBI and VITS are disabled
  # - This applies to both composite and separate Y/C outputs.
  # - Section-level vbi.enabled / vits.enabled flags are currently ignored (reserved for future use).

# List of sections to encode
sections:
  - name: "Leader"
    # ... section configuration ...
  
  - name: "Color Bars"
    # ... section configuration ...
```

---

## Section Configuration

Each section in the `sections` list represents a continuous sequence of frames with consistent encoding parameters.

### Section Structure

```yaml
sections:
  - name: "Section Name"
    duration: 100  # Number of frames (required for yuv422/png; optional for mov-file - omit to use all remaining frames)
    source:
      type: "yuv422-image"  # or "png-image" or "mov-file"
      file: "path/to/image.raw"  # or .png or .mov file
      start_frame: 0  # Optional: for mov-file, which frame to start from (0-indexed, default: 0)
    
    # Optional: filter configuration
    filters:
      chroma:
        enabled: true  # Default: true
      luma:
        enabled: false  # Default: false
    
    # LaserDisc section settings
    laserdisc:
      disc_area: "programme-area"  # lead-in, programme-area, or lead-out
      # ... section-specific parameters: picture_start, chapter, timecode_start, vbi, vits ...
```

**Note on source types**: 
- `yuv422-image`: Raw planar YUV 4:2:2 data. File must contain exactly `width × height × 2` bytes (2 bytes per pixel).
- `png-image`: Any standard PNG format is supported. Single image repeated for `duration` frames.
- `mov-file`: Video file in any format supported by ffmpeg (v210, ProRes, etc.). Extracts `duration` frames starting from `start_frame` (default: 0). If `duration` is omitted, all remaining frames from `start_frame` to end of file are used.



---

## LaserDisc/IEC Standard Configuration

LaserDisc settings are split into:
- **Project-level (once)**: IEC standard and timecode mode
- **Section-level**: Disc area and per-section start points plus VBI/VITS controls

### Project-level LaserDisc Settings (single definition per YAML)

```yaml
laserdisc:
  standard: "iec60857-1986"  # PAL (iec60857-1986), NTSC (iec60856-1986), or "none"
  mode: "cav"                # cav, clv, picture-numbers, or none (must be consistent for all sections)
```

All sections inherit these project-level values; sections cannot change `standard` or `mode`.

### Section-level LaserDisc Settings (per section)

```yaml
laserdisc:
  disc_area: "programme-area"  # lead-in, programme-area, or lead-out
  
  # Convenience boolean flags (alternative to disc_area string):
  # leadin: true   # Equivalent to disc_area: "lead-in"
  # leadout: true  # Equivalent to disc_area: "lead-out"
  
  # Mode-specific per-section starts (inherit mode from project)
  picture_start: 1        # CAV mode: starting picture number (optional, continues if omitted)
  chapter: 1              # CLV mode: chapter number (optional, continues if omitted)
  timecode_start: "00:00:00:00"  # CLV mode: start timecode (optional, continues if omitted)
  start: 1000             # picture-numbers mode: starting number (optional)
  
  # VBI/VITS configuration (parsed but not applied in current version)
  vbi:
    enabled: true  # Currently ignored
  
  vits:
    enabled: true  # Currently ignored
```

---

## Chroma/Luma Filtering Configuration

To prevent high-frequency artifacts (ringing, aliasing) when encoding RGB images to composite video, encode-orc applies optional bandpass filters to chroma and luma components. These filters are based on the ld-chroma-encoder implementation and follow ITU/IEC video standards.

### Filter Types

**Chroma Filter (enabled by default)**
- PAL: 1.3 MHz Gaussian low-pass filter (13-tap)
  - Specification: 0 dB @ 0 Hz, ≥ -3 dB @ 1.3 MHz, ≤ -20 dB @ 4.0 MHz
  - Attenuates high-frequency chroma content that would cause aliasing on vertical color edges

- NTSC: 1.3 MHz low-pass filter (9-tap)
  - Specification: 0 dB @ 0 Hz, ≥ -2 dB @ 1.3 MHz, < -20 dB @ 3.6 MHz

**Luma Filter (disabled by default)**
- Same coefficients as chroma filter
- Typically not needed since luma can support full bandwidth
- Enable only if you have specific HF luma artifacts (rare)

### Section-Level Filter Configuration

Per-section filter settings override default behavior:

```yaml
sections:
  - name: "Section with filters"
    duration: 100
    source:
      type: "yuv422-image"
      file: "image.raw"
    
    # Optional: Configure filters for this section
    filters:
      chroma:
        enabled: true   # Default: true (recommended for color bars)
      luma:
        enabled: false  # Default: false (usually not needed)
    
    laserdisc:
      # ... laserdisc configuration ...
```

### Why Filtering Matters

Without chroma filtering, vertical edges in color bars exhibit:
- **Ringing artifacts** on sharp color transitions
- **Aliasing** due to high-frequency content exceeding Nyquist limit
- **HF noise** spikes at color boundaries

The 1.3 MHz bandpass filter:
1. Limits chroma bandwidth to video standard limits (PAL/NTSC)
2. Removes high-frequency components before subcarrier modulation
3. Produces smooth, artifact-free color transitions
4. Matches real LaserDisc encoding practices

### Example: Disabling Filters (Not Recommended)

```yaml
sections:
  - name: "No filtering (not recommended)"
    duration: 100
    source:
      type: "yuv422-image"
      file: "image.raw"
    
    filters:
      chroma:
        enabled: false  # WARNING: Artifacts likely
      luma:
        enabled: false
```

---

## Timecode and Numbering Modes

The project selects a single timecode mode (`cav`, `clv`, `picture-numbers`, or `none`) in the top-level `laserdisc` block. Sections can only supply per-section start values; they cannot change the mode.

#### CAV Mode (Constant Angular Velocity)

For CAV discs with picture numbers:

```yaml
laserdisc:
  disc_area: "programme-area"
  picture_start: 1  # Starting picture number (1-79999), optional: defaults to 'continue'
  # Picture numbers increment by 1 per frame
```

**Example CAV Configuration:**
```yaml
sections:
  - name: "CAV Section 1"
    duration: 1000
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1  # Pictures 1-1000
      
  - name: "CAV Section 2"
    duration: 500
    laserdisc:
      disc_area: "programme-area"
      # picture_start omitted: continues from 1001-1500
```

#### CLV Mode (Constant Linear Velocity)

For CLV discs with timecode and chapter numbers:

```yaml
laserdisc:
  disc_area: "programme-area"
  chapter: 1  # Chapter number (0-79), optional: continues from previous if omitted
  timecode_start: "00:00:00:00"  # Format: HH:MM:SS:FF, optional: continues from previous if omitted
  # Timecode increments according to video standard (25fps PAL, 29.97fps NTSC)
```

**Example CLV Configuration:**
```yaml
sections:
  - name: "CLV Chapter 1"
    duration: 1500  # 60 seconds at 25fps
    laserdisc:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:00:00"
      
  - name: "CLV Chapter 1 continued"
    duration: 750  # 30 more seconds
    laserdisc:
      disc_area: "programme-area"
      # chapter and timecode_start omitted: continues from 00:01:00:00 in chapter 1
      
  - name: "CLV Chapter 2"
    duration: 1500
    laserdisc:
      disc_area: "programme-area"
      chapter: 2  # Explicitly start new chapter
      # timecode_start omitted: continues from previous timecode
```

#### Picture Numbers Only

For simple sequential picture numbering:

```yaml
laserdisc:
  disc_area: "programme-area"
  start: 1000  # Starting picture number, optional: continues from previous if omitted
```

#### No Timecode

To disable timecode/picture number encoding:

```yaml
laserdisc:
  disc_area: "programme-area"
  # No timecode/picture numbers when project mode is "none"
```

---

### VBI (Vertical Blanking Interval) – current implementation

- VBI is generated only when the project standard is IEC 60856-1986 (NTSC) or IEC 60857-1986 (PAL). Standard `none` disables VBI.
- Section-level `vbi.enabled` flag is **parsed but not applied** (reserved for future use).
- VBI line-level configuration (line16, line17, line18 with auto modes, bytes, status codes) is **not parsed or implemented**.
- Encoding is automatic and standard-driven:
  - CAV: lines 16–18 carry picture numbers in biphase per IEC LaserDisc rules.
  - CLV chapter/timecode: lines carry chapter or timecode in biphase per IEC rules.
  - Programme/lead-in/lead-out status follows the IEC defaults.
- The examples showing detailed VBI line configuration are **for future reference only** and do not work in the current version.

---

### VITS (Vertical Interval Test Signals) – current implementation

- VITS is generated only when the project standard is IEC 60856-1986 (NTSC) or IEC 60857-1986 (PAL). Standard `none` disables VITS entirely.
- VITS is included for both composite and separate Y/C outputs when the standard allows it.
- Section-level `vits.enabled` flag is **parsed but not applied** (reserved for future use).
- Only the built-in IEC waveforms/line assignments are emitted (PAL: lines 19/20/332/333 per parity; NTSC: lines 19/20/282/283).
- Custom VITS line overrides are **not implemented**.

---

## Video Signal Levels Configuration

Video signal levels define the electrical amplitudes for blanking, black, and white components of the composite video signal. These values are expressed in a 16-bit IRE scale where the full range is 0-65535.

### Default Video Levels

encode-orc uses standard video levels by default:

**PAL Composite (default)**
- `blanking_16b_ire`: 17125 (0 IRE blanking level)
- `black_16b_ire`: 17125 (0 IRE black level, same as blanking in PAL)
- `white_16b_ire`: 54016 (700mV peak white)

**NTSC Composite (default)**
- `blanking_16b_ire`: 15058 (−300mV blanking level)
- `black_16b_ire`: 17768 (7.5 IRE setup for NTSC)
- `white_16b_ire`: 51200 (700mV peak white)

### Customizing Video Levels

To override the default levels for a specific project, add a `video_levels` section to the `output` configuration:

```yaml
output:
  filename: "output.tbc"
  format: "ntsc-composite"
  
  # Override video levels for NTSC-J (Japan) with different black setup
  video_levels:
    blanking_16b_ire: 15058
    black_16b_ire: 18196    # Alternative black setup
    white_16b_ire: 51200
```

### When to Use Custom Video Levels

Custom video levels are useful for:
- **Regional variants**: NTSC-J (Japan) uses different black setup than standard NTSC
- **Specialized equipment**: Legacy equipment may require non-standard levels
- **Calibration**: Matching specific reference materials or test patterns
- **Legacy archive**: Reproducing historical recording formats

### Partial Overrides

You can override just the levels you need; omitted levels use defaults:

```yaml
output:
  filename: "output.tbc"
  format: "pal-composite"
  
  # Only override black level, keep blanking and white as defaults
  video_levels:
    black_16b_ire: 18000
```

---

## Complete Example Projects

### Example 1: Simple PAL Test with YUV422

```yaml
name: "PAL Color Bars Test"
description: "YUV422 EBU color bars with IEC 60857 VITS"

output:
  filename: "pal-test.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "Color Bars"
    duration: 100
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/pal-ebu-colorbars-75.raw"
    filters:
      chroma:
        enabled: true  # Apply 1.3 MHz filter to prevent artifacts
      luma:
        enabled: false
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1
```

### Example 2: Multi-Section LaserDisc with YUV422

```yaml
name: "Educational LaserDisc"
description: "Multi-chapter CLV disc with YUV422 images"

output:
  filename: "educational-master.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "clv"

sections:
  # Leader
  - name: "Leader - Color Bars"
    duration: 250  # 10 seconds at 25fps
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/pal-ebu-colorbars-75.raw"
    laserdisc:
      disc_area: "lead-in"
      chapter: 0
      timecode_start: "00:00:00:00"
        
  # Chapter 1
  - name: "Chapter 1 - Content"
    duration: 2500  # 100 seconds
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/custom-content-1.raw"
    laserdisc:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:10:00"
        
  # Chapter 2
  - name: "Chapter 2 - Content"
    duration: 7500  # 300 seconds (5 minutes)
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/custom-content-2.raw"
    laserdisc:
      disc_area: "programme-area"
      chapter: 2
      timecode_start: "00:01:50:00"
```

### Example 3: NTSC CAV with YUV422

```yaml
name: "NTSC CAV Reference"
description: "CAV test disc with YUV422 color bars"

output:
  filename: "ntsc-cav-reference.tbc"
  format: "ntsc-composite"

laserdisc:
  standard: "iec60856-1986"
  mode: "cav"

sections:
  # EIA bars - 100%
  - name: "EIA Bars 100%"
    duration: 1800  # 60 seconds at 29.97fps
    source:
      type: "yuv422-image"
      file: "testcard-images/ntsc-raw/ntsc-eia-colorbars-100.raw"
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1
        
  # EIA bars - 75%
  - name: "EIA Bars 75%"
    duration: 1800
    source:
      type: "yuv422-image"
      file: "testcard-images/ntsc-raw/ntsc-eia-colorbars-75.raw"
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1801
```

### Example 4: Minimal YUV422 Configuration

```yaml
name: "Simple Test"
description: "Quick test with YUV422 image"

output:
  filename: "test.tbc"
  format: "pal-composite"

laserdisc:
  standard: "none"
  mode: "none"

sections:
  - name: "Test Content"
    duration: 50
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/pal-ebu-colorbars-75.raw"
```

### Example 5: PNG Image Source

```yaml
name: "PNG Test"
description: "Encoding from PNG images"

output:
  filename: "png-test.tbc"
  format: "pal-composite"

laserdisc:
  standard: "none"
  mode: "none"

sections:
  - name: "Test Pattern 1"
    duration: 25
    source:
      type: "png-image"
      file: "testcard-images/test-pattern-1.png"
  - name: "Test Pattern 2"
    duration: 25
    source:
      type: "png-image"
      file: "testcard-images/test-pattern-2.png"
```

### Example 6: Separate Y/C Output

```yaml
name: "Separate Y/C Test"
description: "Output separate luma and chroma TBC files"

output:
  filename: "test.tbc"
  format: "pal-composite"
  mode: "separate-yc"  # Creates test.tbcy and test.tbcc

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "Test Content"
    duration: 100
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/pal-ebu-colorbars-75.raw"
    laserdisc:
      picture_start: 1
```

### Example 7: Custom Metadata Decoder String

```yaml
name: "Custom Decoder String Test"
description: "Using a custom decoder identifier in metadata"

output:
  filename: "custom-test.tbc"
  format: "pal-composite"
  metadata_decoder: "my-custom-encoder"  # Override default "encode-orc"

laserdisc:
  standard: "none"
  mode: "none"

sections:
  - name: "Test Content"
    duration: 100
    source:
      type: "yuv422-image"
      file: "testcard-images/pal-raw/pal-ebu-colorbars-75.raw"
```

**Note**: The `metadata_decoder` field controls the decoder string written to the SQLite metadata database. This can be useful for compatibility testing with different decoder tools (e.g., "ld-decode", "vhs-decode") or for identifying files generated by custom encoding workflows. If omitted, it defaults to "encode-orc".

---

## Field Reference

### Top-Level Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Project name |
| `description` | string | Yes | Project description |
| `output` | object | Yes | Output configuration |
| `laserdisc` | object | Yes | Project-level IEC standard and mode (cav/clv/picture-numbers/none). The standard decides if VBI/VITS are generated. |
| `sections` | array | Yes | List of encoding sections |

### Output Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `filename` | string | Yes | Output TBC filename |
| `format` | string | Yes | Output format (pal-composite, ntsc-composite, pal-yc, ntsc-yc) |
| `mode` | string | No | Output mode: "combined" (default, single .tbc file), "separate-yc" (separate .tbcy/.tbcc files), or "separate-yc-legacy" |
| `metadata_decoder` | string | No | Decoder string written to metadata database (default: "encode-orc") |

### Section Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Section name |
| `duration` | integer | Yes | Number of frames in section |
| `source` | object | Yes | Video source configuration. Type must be "yuv422-image" for raw YUV 4:2:2 planar, or "png-image" for PNG files |
| `filters` | object | No | Filter configuration with optional chroma/luma filter settings (default: chroma enabled, luma disabled) |
| `laserdisc` | object | No | Section-level LaserDisc settings. Supports: disc_area (or leadin/leadout flags), picture_start, chapter, timecode_start, start. VBI/VITS enabled flags are parsed but not applied. |

### LaserDisc Modes

| Mode | Fields | Description |
|------|--------|-------------|
| `cav` | `picture_start` (optional) | CAV picture numbers (continues from previous if not specified) |
| `clv` | `chapter` (optional), `timecode_start` (optional) | CLV timecode with chapter (continues from previous if not specified) |
| `picture-numbers` | `start` (optional) | Simple picture numbering (continues from previous if not specified) |

**Note**: The mode is chosen once at the project level; sections cannot override it. Mixing `cav` and `clv` (or `none`) within the same project is not allowed.

---

## Validation Rules

The YAML parser enforces the following:

1. **Required top-level fields**: `name` and `output.filename`, `output.format` must be present
2. **Valid format**: Output format must be one of: `pal-composite`, `ntsc-composite`, `pal-yc`, `ntsc-yc`
3. **Valid mode**: Output mode must be one of: `combined` (default), `separate-yc`, `separate-yc-legacy`
4. **Section presence**: At least one section must be defined
5. **Section name**: Each section must have a non-empty name
6. **Section source**: Each section must specify a source type (only `yuv422-image` and `png-image` are supported)
7. **Section duration**: Duration must be specified and be a positive integer for all sections
8. **Picture numbers**: If specified, `picture_start` and `start` must be greater than 0
9. **LaserDisc standard**: If specified, must be `iec60856-1986`, `iec60857-1986`, or `none`

**Note**: The following are NOT currently validated:
- Matching of video system (PAL/NTSC) between format and LaserDisc standard
- Timecode format validity
- Chapter number ranges (0-79)
- CAV picture number ranges (1-79999)
- File existence or size
- Video level ranges

---

## Implementation Notes

### Sequential Frame Numbering

The application should:
1. Process sections in order
2. Automatically increment frame numbers, timecodes, and picture numbers
3. Continue from the previous section's end values if start values are not specified
4. Allow explicit start values to create gaps or reset numbering

### Frame Number Continuity

For CAV mode:
```
Section 1: picture_start=1, duration=100 → frames 1-100
Section 2: picture_start omitted, duration=50 → frames 101-150 (continues)
Section 3: picture_start=200, duration=50 → frames 200-249 (explicit gap)
```

For CLV mode:
```
Section 1: chapter=1, timecode_start="00:00:00:00", duration=1500 → ends at 00:01:00:00
Section 2: chapter/timecode omitted, duration=750 → continues from 00:01:00:00 to 00:01:30:00 in chapter 1
Section 3: chapter=2, timecode omitted, duration=1500 → continues timecode but starts chapter 2
```

### Metadata Generation

The application automatically:
- Generates TBC metadata in JSON and SQLite formats
- Encodes VBI data in biphase format (when LaserDisc standard is enabled)
- Inserts VITS signals on correct lines (when LaserDisc standard is enabled)
- Calculates field phase IDs correctly
- Writes the decoder identifier to the metadata (configurable via `metadata_decoder`)

### Current Limitations

- **VBI line-level configuration**: While the YAML structure supports detailed VBI configuration (line16, line17, line18 with auto modes, custom bytes, status codes), **this is not currently implemented**. VBI is generated automatically based on the LaserDisc standard and mode.
- **VITS line-level configuration**: Section-level VITS enabled flags are parsed but not applied. VITS generation is controlled solely by the project-level LaserDisc standard.
- **Per-section VBI/VITS control**: The `vbi.enabled` and `vits.enabled` flags at the section level are parsed but currently ignored.

### Future Extensions

Possible future additions:
- Per-section and per-line VBI configuration
- Per-section VITS control
- Closed caption data configuration
- VITC (Vertical Interval Timecode) support
- Audio configuration (currently silent)
- Frame rate override
- Custom VITS waveform definitions
- Include/import other YAML files

---

## Migration Note

The command-line interface (with switches like `-o`, `-f`, `-t`, `-n`, etc.) has been removed. All encoding is now done exclusively through YAML project files. This provides:

- **Better documentation**: Projects are self-describing and can include comments
- **Version control**: Easy to track changes to encoding configurations
- **Complex projects**: Multi-section projects with different standards and sources
- **Reproducibility**: Same project file always produces the same output

---

## Example: MOV File Input

Here's an example of using a MOV file as input. This is useful for frame-accurate studio-level video files:

```yaml
name: "PAL Testcard from MOV"
description: "Encoding PAL testcards with moving elements from v210 MOV file"

output:
  filename: "test-output/pal-testcard-mov.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  # Encode frames 0-49 (first 2 seconds at 25fps)
  - name: "Opening Sequence"
    duration: 50
    source:
      type: "mov-file"
      file: "testcard-images/pal-mov/pt5300.mov"
      start_frame: 0
    laserdisc:
      picture_start: 1
    filters:
      chroma:
        enabled: true
      luma:
        enabled: false
  
  # Continue with frames 50-99 (next 2 seconds)
  - name: "Continuation"
    duration: 50
    source:
      type: "mov-file"
      file: "testcard-images/pal-mov/pt5300.mov"
      start_frame: 50
    # LaserDisc picture numbers continue automatically (51-100)
```

**MOV File Requirements:**
- Any video format supported by ffmpeg (v210, ProRes, etc.)
- Dimensions must match the target video system (720×576 for PAL, 720×486 for NTSC)
- Use `start_frame` to specify which frame to start extracting from (0-indexed, optional, default: 0)
- Use `duration` to specify how many frames to extract and encode
  - If `duration` is omitted, all remaining frames from `start_frame` to the end of the file will be used
  - This is useful for encoding entire MOV files or splitting them across multiple sections with continuous picture numbers

**Example: Encoding entire MOV file across multiple sections:**

```yaml
name: "Complete PAL MOV File"
description: "Encode entire MOV split into chapters"

output:
  filename: "test-output/complete-mov.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  # Use entire MOV file, picture numbers continue across all sections
  - name: "Full Video"
    source:
      type: "mov-file"
      file: "testcard-images/pal-mov/pt5300.mov"
      # No start_frame or duration - uses all frames
    laserdisc:
      picture_start: 1
```

**Note:** The MOV loader uses ffmpeg/ffprobe internally, so these tools must be installed and available in your PATH.

If you previously used CLI switches, create a YAML project file with equivalent settings (see examples below).

---
