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
  metadata_decoder: "encode-orc"  # Optional: decoder string in metadata (default: "encode-orc")
  
  # Optional: Override video signal levels (16-bit IRE scale)
  # Use this to customize blanking, black, and white levels for specific projects
  # Values should be in the 16-bit range (0-65535)
  video_levels:
    blanking_16b_ire: 17125   # Optional: blanking level (default: system-dependent)
    black_16b_ire: 17125      # Optional: black level (default: system-dependent)
    white_16b_ire: 54016      # Optional: white/peak level (default: system-dependent)

# For YC formats (pal-yc, ntsc-yc), two files are produced:
# - <basename>.tbcy : luma
# - <basename>.tbcc : chroma
# The base name comes from output.filename without extension.
  
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
    duration: 100  # Number of frames in this section
    source:
      type: "rgb30-image"  # Only supported source type
      file: "path/to/image.raw"
    
    # LaserDisc section settings
    laserdisc:
      disc_area: "programme-area"  # lead-in, programme-area, or lead-out
      # ... section-specific parameters: picture_start, chapter, timecode_start, vbi, vits ...
```



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
  
  # Mode-specific per-section starts (inherit mode from project)
  picture_start: 1        # CAV mode: starting picture number (optional, continues if omitted)
  chapter: 1              # CLV mode: chapter number (optional, continues if omitted)
  timecode_start: "00:00:00:00"  # CLV mode: start timecode (optional, continues if omitted)
  start: 1000             # picture-numbers mode: starting number (optional)
  
  # VBI configuration (lines 16, 17, 18)
  vbi:
    enabled: true
    # ... VBI settings ...
  
  # VITS configuration
  vits:
    enabled: true
    # ... VITS settings ...
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
      type: "rgb30-image"
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
      type: "rgb30-image"
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
- Section-level `vbi.enabled` flags and line overrides are not currently applied (reserved for future use).
- Encoding is automatic and standard-driven:
  - CAV: lines 16–18 carry picture numbers in biphase per IEC LaserDisc rules.
  - CLV chapter/timecode: lines carry chapter or timecode in biphase per IEC rules.
  - Programme/lead-in/lead-out status follows the IEC defaults; there is no custom byte injection.
- Manual byte arrays, custom status codes, and signal parameter overrides described previously are **not implemented** yet.

---

### VITS (Vertical Interval Test Signals) – current implementation

- VITS is generated only when the project standard is IEC 60856-1986 (NTSC) or IEC 60857-1986 (PAL). Standard `none` disables VITS entirely.
- VITS is included for both composite and separate Y/C outputs when the standard allows it.
- Section-level `vits.enabled` flags and custom line overrides are not currently applied (reserved for future use).
- Only the built-in IEC waveforms/line assignments are emitted (PAL: lines 19/20/332/333 per parity; NTSC: lines 19/20/282/283). No per-section overrides yet.

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

### Example 1: Simple PAL Test with RGB30

```yaml
name: "PAL Color Bars Test"
description: "RGB30 EBU color bars with IEC 60857 VITS"

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
      type: "rgb30-image"
      file: "testcard-images/pal-ebu-colorbars-100.raw"
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1
      
      vbi:
        enabled: true
        line16:
          auto: "status"
          status_code: 0x80
        line17:
          auto: "picture-number"
        line18:
          auto: "picture-number"
      
      vits:
        enabled: true
```

### Example 2: Multi-Section LaserDisc with RGB30

```yaml
name: "RGB30 Educational LaserDisc"
description: "Multi-chapter CLV disc with RGB30 images"

output:
  filename: "educational-master.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "clv"

sections:
  # Leader with RGB30 bars
  - name: "Leader - Color Bars"
    duration: 250  # 10 seconds at 25fps
    source:
      type: "rgb30-image"
      file: "testcard-images/pal-ebu-colorbars-75.raw"
    laserdisc:
      disc_area: "lead-in"
      chapter: 0
      timecode_start: "00:00:00:00"
      
      vbi:
        enabled: true
        line17:
          auto: "timecode"
        line18:
          auto: "chapter"
      
      vits:
        enabled: true
        
  # Chapter 1: RGB30 content
  - name: "Chapter 1 - Content"
    duration: 2500  # 100 seconds
    source:
      type: "rgb30-image"
      file: "testcard-images/custom-content-1.raw"
    laserdisc:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:10:00"
      
      vbi:
        enabled: true
        line17:
          auto: "timecode"
        line18:
          auto: "chapter"
      
      vits:
        enabled: true
        
  # Chapter 2: Different RGB30 image
  - name: "Chapter 2 - Content"
    duration: 7500  # 300 seconds (5 minutes)
    source:
      type: "rgb30-image"
      file: "testcard-images/custom-content-2.raw"
    laserdisc:
      disc_area: "programme-area"
      chapter: 2
      timecode_start: "00:01:50:00"
      
      vbi:
        enabled: true
        line17:
          auto: "timecode"
        line18:
          auto: "chapter"
      
      vits:
        enabled: true
```

### Example 3: NTSC CAV with RGB30

```yaml
name: "NTSC CAV RGB30 Reference"
description: "CAV test disc with RGB30 color bars"

output:
  filename: "ntsc-cav-reference.tbc"
  format: "ntsc-composite"

laserdisc:
  standard: "iec60856-1986"
  mode: "cav"

sections:
  # RGB30 EIA bars - 100%
  - name: "EIA Bars 100%"
    duration: 1800  # 60 seconds at 29.97fps
    source:
      type: "rgb30-image"
      file: "testcard-images/ntsc-eia-colorbars-100.raw"
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1
      
      vbi:
        enabled: true
        line17:
          auto: "picture-number"
        line18:
          auto: "picture-number"
      
      vits:
        enabled: true
        
  # RGB30 EIA bars - 75%
  - name: "EIA Bars 75%"
    duration: 1800
    source:
      type: "rgb30-image"
      file: "testcard-images/ntsc-eia-colorbars-75.raw"
    laserdisc:
      disc_area: "programme-area"
      picture_start: 1801
      
      vbi:
        enabled: true
        line17:
          auto: "picture-number"
        line18:
          auto: "picture-number"
      
      vits:
        enabled: true
```

### Example 4: Minimal RGB30 Configuration

```yaml
name: "Simple RGB30 Test"
description: "Quick test with RGB30 image"

output:
  filename: "test.tbc"
  format: "pal-composite"

laserdisc:
  standard: "none"
  mode: "none"

sections:
  - name: "RGB30 Content"
    duration: 50
    source:
      type: "rgb30-image"
      file: "testcard-images/pal-ebu-colorbars-100.raw"
```

### Example 5: Custom Metadata Decoder String

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
      type: "rgb30-image"
      file: "testcard-images/pal-ebu-colorbars-100.raw"
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
| `format` | string | Yes | Output format (pal-composite, ntsc-composite, pal-yc, ntsc-yc); YC formats emit two files: .tbcy (luma) and .tbcc (chroma) |
| `metadata_decoder` | string | No | Decoder string written to metadata database (default: "encode-orc"); can be set to any string like "ld-decode" or "custom-encoder" |

### Section Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Section name |
| `duration` | integer | Yes | Number of frames in section; frames = duration (RGB30 files must have exactly this many frames) |
| `source` | object | Yes | Video source configuration (must be type: "rgb30-image") |
| `laserdisc` | object | No | Section-level LaserDisc settings (disc_area, per-section starts). VBI/VITS flags are parsed but currently not applied. |

### LaserDisc Modes

| Mode | Fields | Description |
|------|--------|-------------|
| `cav` | `picture_start` (optional) | CAV picture numbers (continues from previous if not specified) |
| `clv` | `chapter` (optional), `timecode_start` (optional) | CLV timecode with chapter (continues from previous if not specified) |
| `picture-numbers` | `start` (optional) | Simple picture numbering (continues from previous if not specified) |

**Note**: The mode is chosen once at the project level; sections cannot override it. Mixing `cav` and `clv` (or `none`) within the same project is not allowed.

---

## Validation Rules

The YAML parser should enforce:

1. **Required fields**: `name`, `description`, `output.filename`, `output.format` must be present
2. **Valid format**: Output format must be one of the four supported types
3. **Section duration**: Must be positive integer; required for all RGB30 image sections
4. **RGB30 file validation**: File must exist, be readable, and have exactly `width × height × 6` bytes (720×576 = 2,488,320 for PAL; 720×486 = 2,099,520 for NTSC)
5. **Picture numbers**: CAV picture numbers must be in range 1-79999
6. **Chapter numbers**: CLV chapters must be in range 0-79
7. **Timecode format**: Must be HH:MM:SS:FF with valid values
8. **VBI bytes**: Must be in range 0x00-0xFF
9. **IEC standard**: Choosing `none` disables VBI/VITS. A PAL/NTSC match is recommended but not currently enforced by the parser.
10. **Non-empty sections**: At least one section must be defined
11. **Project-wide mode**: Mode is set once at the project level; sections cannot override it
12. **Continuation logic**: If start values are omitted, the first section must specify initial values; subsequent sections continue from previous

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

The application should automatically:
- Generate appropriate TBC metadata JSON
- Encode VBI data in biphase format
- Insert VITS signals on correct lines
- Calculate field phase IDs correctly

### Future Extensions

Possible future additions:
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

If you previously used CLI switches, create a YAML project file with equivalent settings (see examples below).

---
