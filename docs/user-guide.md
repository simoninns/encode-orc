# encode-orc User Guide

## Overview

`encode-orc` is a video encoding application that converts digital video sources into composite video signals (TBC format) with optional artifacts, metadata, and signal effects. The application is configured entirely through YAML project files.

```bash
./encode-orc project.yaml
```

---

## Pipeline Architecture

The encode-orc pipeline processes video through seven stages:

```
Stage 1: Source Loading
  ├─ Read video from disk (YUV422, PNG, MOV, MP4)
  └─ Decode to raw YUV 4:2:2 frames

Stage 3: Preprocessing
  ├─ Apply chroma low-pass filter (optional)
  └─ Apply luma low-pass filter (optional)

Stage 5: Composite Encoding
  ├─ Encode YUV 4:2:2 to composite video
  ├─ Insert color burst reference signals
  └─ Generate color-based VBI/VITS/VITC metadata

Stage 7: Post-Processing (Phase 6)
  ├─ Apply Gaussian noise (tape hiss simulation)
  ├─ Apply line dropouts (tape damage)
  └─ Apply phase jitter (VCR time-base errors)

Stage 8: Output
  ├─ Write TBC file(s) with metadata
  └─ Generate SQLite metadata database
```

Each stage is independently configurable, and stages can be enabled/disabled as needed.

---

## Project Structure

Every encode-orc project requires:

```yaml
name: "Project Name"
description: "Project description"

output:
  filename: "output.tbc"
  format: "pal-composite"  # or ntsc-composite, pal-yc, ntsc-yc

laserdisc:
  standard: "iec60857-1986"  # Project-wide standard: iec60857-1986, iec60856-1986, consumer-tape, or none
  mode: "cav"                # Timecode mode: cav, clv, picture-numbers, or none

sections:
  - name: "Section 1"
    # ... section configuration ...
```

The three top-level blocks (`output`, `laserdisc`, `sections`) are required. All other configuration is optional.

---

## Stage 1: Source Loading

Configure video input sources in each section.

### Supported Source Types

```yaml
sections:
  - name: "Section Name"
    duration: 100  # Number of frames to encode
    
    source:
      type: "yuv422-image"   # Raw YUV 4:2:2 planar
      file: "path/to/file.raw"
      # File must contain exactly: width × height × 2 bytes (2 bytes per pixel)
```

**YUV422 Format Details:**
- Data layout: All Y samples first, then all U samples, then all V samples (planar)
- Byte order: Big-endian (Y/U/V components)
- Dimensions must match the output video system (720×576 PAL, 720×486 NTSC)

#### Alternative: PNG Images

```yaml
source:
  type: "png-image"
  file: "path/to/image.png"
```

PNG files are scaled to fit the output video system and repeated for all `duration` frames. Supports any standard PNG format.

#### Alternative: MOV/MP4 Files

```yaml
source:
  type: "mov-file"  # or "mp4-file"
  file: "path/to/video.mov"
  start_frame: 0     # Optional: which frame to start from (0-indexed, default: 0)
  duration: 1000     # Optional: how many frames to extract (default: all remaining)
```

MOV/MP4 files are decoded by ffmpeg, supporting any video format:
- **MOV**: v210, ProRes, etc.
- **MP4**: H.264, H.265, etc.

If `duration` is omitted, all remaining frames from `start_frame` to the end of file are used.

### Example: Multiple Sections with Different Sources

```yaml
sections:
  - name: "Leader - Color Bars"
    duration: 100
    source:
      type: "yuv422-image"
      file: "colorbar.raw"
  
  - name: "Content - Video File"
    source:
      type: "mov-file"
      file: "content.mov"
      start_frame: 0
      # duration omitted: use all frames from the MOV file
  
  - name: "Outro - PNG Pattern"
    duration: 50
    source:
      type: "png-image"
      file: "pattern.png"
```

---

## Stage 3: Preprocessing Filters

Apply low-pass filtering to remove high-frequency artifacts before composite encoding.

### Configuration

```yaml
pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true   # Default: true (recommended)
      luma:
        enabled: false  # Default: false (rarely needed)
```

### Filter Specifications

**Chroma Filter** (enabled by default):
- **PAL**: 1.3 MHz Gaussian low-pass (25-tap FIR)
- **NTSC**: 600 kHz low-pass (25-tap FIR)
- Prevents color bar artifacts and ringing on sharp color transitions

**Luma Filter** (disabled by default):
- **PAL**: 5.5 MHz Gaussian low-pass (25-tap FIR)
- **NTSC**: 3.6 MHz low-pass (25-tap FIR)
- Enable only if you observe high-frequency artifacts in grayscale content

### When to Use Filters

| Content Type | Chroma | Luma | Reason |
|--------------|--------|------|--------|
| Color bars (test patterns) | ✓ | ✗ | Prevent ringing on color edges |
| Photo/artwork | ✓ | ✗ | Smooth color transitions |
| High-resolution text | ✓ | ✓ | Reduce edge ringing |
| Smooth video content | ✓ | ✗ | Standard approach |

### Section-Level Filter Overrides

You can override filters for specific sections:

```yaml
sections:
  - name: "Color Bars with Filtering"
    duration: 100
    source:
      type: "yuv422-image"
      file: "colorbars.raw"
    filters:
      chroma:
        enabled: true   # Override global setting
      luma:
        enabled: false
```

---

## Standards and Metadata Requirements

The `laserdisc.standard` setting is a critical choice that **enforces specific VBI/VITS/VITC data generation** and **line placement rules**. You cannot selectively enable or disable the metadata that a standard requires—it's all-or-nothing per standard choice.

### Standard Enforcement Overview

Each standard automatically generates:

```
┌─────────────────────┬──────────┬──────────┬──────────┬─────────────────┐
│ Standard            │ VBI Data │ VITS     │ VITC     │ What It Encodes │
├─────────────────────┼──────────┼──────────┼──────────┼─────────────────┤
│ iec60857-1986 (PAL) │ ✓ Auto   │ ✓ Auto   │ ✗        │ Picture #, Ch.# │
│ iec60856-1986 (NTSC)│ ✓ Auto   │ ✓ Auto   │ ✗        │ Picture #, Ch.# │
│ consumer-tape       │ ✗        │ ✗        │ ✓ Auto   │ Timecode only   │
│ none                │ ✗        │ ✗        │ ✗        │ Nothing (clean) │
└─────────────────────┴──────────┴──────────┴──────────┴─────────────────┘
```

### LaserDisc Standards (iec60857-1986 / iec60856-1986)

When you select a LaserDisc standard, the application **automatically generates** both VBI and VITS according to IEC specifications. You cannot disable these—they are inherent to the standard.

#### VBI (Vertical Blanking Interval) - Automatic Placement

VBI data appears on **lines 16, 17, 18** (1-indexed, per field):

```yaml
laserdisc:
  standard: "iec60857-1986"  # PAL LaserDisc - forces VBI on lines 16-18
  # OR
  standard: "iec60856-1986"  # NTSC LaserDisc - forces VBI on lines 16-18
```

**VBI Encodes:**
- **CAV mode**: Picture numbers (1–79,999) on lines 16–18, increments per frame
- **CLV mode**: Chapter number + timecode (HH:MM:SS:FF) on lines 16–18, increments per frame
- **Programme/lead-in/lead-out markers**: Automatically determined by `disc_area` setting

**Data Format:**
- Manchester (biphase) encoding on lines 16–18
- Automatic: no configuration needed, standard-driven entirely

#### VITS (Vertical Interval Test Signals) - Automatic Placement

VITS signals automatically appear on:
- **PAL**: Lines 19, 20, 332, 333 (per field parity)
- **NTSC**: Lines 19, 20, 282, 283 (per field parity)

```yaml
laserdisc:
  standard: "iec60857-1986"  # Automatically adds PAL VITS
```

**VITS Includes (IEC Standard):**
- Multiburst test signal (chroma response)
- UK National Television test signal
- Field synchronization markers
- Amplitude calibration references

**Data Format:**
- Automatically formatted per IEC 60857 (PAL) or IEC 60856 (NTSC)
- Cannot be customized; built-in waveforms only

### Consumer Tape Standard (consumer-tape)

Consumer video tape formats (VHS, Betamax, Video8) use **VITC (Vertical Interval Time Code)** instead of LaserDisc VBI/VITS:

```yaml
laserdisc:
  standard: "consumer-tape"  # Disables VBI/VITS, enables VITC
  mode: "cav"                # or "clv"
```

#### VITC (Vertical Interval Time Code) - Automatic Placement

VITC timecode appears on fixed lines per video system:

**NTSC VITC Placement:**
- Lines 14 and 16 (1-indexed, per field)
- Each frame encodes 4 VITC lines (2 lines × 2 fields)

**PAL VITC Placement:**
- Lines 19 and 21 (1-indexed, per field)
- Each frame encodes 4 VITC lines (2 lines × 2 fields)

**VITC Encodes:**
- Timecode: HH:MM:SS:FF (hours:minutes:seconds:frames)
- Frame-accurate, increments per frame
- User bits: currently set to 0 (reserved for future use)

**Data Format:**
- Manchester (biphase) encoding
- Sine-squared edge shaping (~50 ns rise/fall time)
- Amplitude: 0–100 IRE (blanking to white)

### No Standard (none)

When you select `none`, all VBI, VITS, and VITC generation is disabled:

```yaml
laserdisc:
  standard: "none"  # No automatic metadata
  mode: "none"      # No picture numbers or timecode
```

**Result:**
- Clean composite video without vertical interval data
- Useful for test patterns, art, or archival where metadata is not required
- Smallest file size

### Standard Selection Table

| Goal | Standard | Mode | Metadata |
|------|----------|------|----------|
| Encode LaserDisc (PAL) | `iec60857-1986` | `cav` or `clv` | VBI + VITS |
| Encode LaserDisc (NTSC) | `iec60856-1986` | `cav` or `clv` | VBI + VITS |
| Encode VHS/Betamax tape | `consumer-tape` | `cav` or `clv` | VITC only |
| Create test patterns (no metadata) | `none` | `none` | None |

### Important Notes: How Standards and Pipeline Configuration Work Together

1. **Standards drive pipeline generator enablement**: The `laserdisc.standard` setting controls which pipeline generators are **applicable** via `is_applicable()` checks. For example:
   - `iec60857-1986` makes `BiphaseVBIMetadataGenerator` and `VITSMetadataGenerator` applicable
   - `consumer-tape` makes `VITCPipelineGenerator` applicable
   - This is built into the C++ encoder logic, not configurable in YAML

2. **Pipeline configuration customizes generator parameters**: The `pipeline.metadata.generators` YAML configuration allows you to customize **how** the applicable generators operate (which lines they use, what signals they generate, etc.). The configuration is **parsed and applied** by the encoder.

3. **Both are needed but serve different purposes**:
   - **Standard** = "What type of metadata does this content need?" (enforced rule)
   - **Pipeline generators** = "How should each metadata type be implemented?" (customizable details)
   
   Example: If you choose `consumer-tape`, VITC generation is enabled by the standard. The `pipeline.metadata.generators` section lets you customize VITC line placement if needed.

4. **Line placement has reasonable defaults but can be customized**: While each standard has default line placements (VBI on 16-18, VITS on specific PAL/NTSC lines, VITC on 14/16 or 19/21), the pipeline configuration allows limited customization of these lines through the `lines` parameter in generator configs. However, standards enforce which signals are generated—you cannot use pipeline config to override a standard's choices.

5. **Section-level `biphase-vbi` only controls numbering, not signal type**: The `laserdisc.standard` sets the metadata **type** (LaserDisc VBI vs. VITC vs. none), while section-level `biphase-vbi` configuration (like `picture_start`, `chapter`, `timecode_start`) controls only the **numerical values** within that standard.

---

## Stage 5: Composite Encoding & Metadata

### Color Burst (Always Enabled)

Color burst reference signals are automatically added to enable proper chroma demodulation.

```yaml
pipeline:
  metadata:
    generators:
      - type: "color-burst"
        enabled: true   # Usually always enabled
```

### VBI/VITS/VITC Metadata

The project-level `laserdisc.standard` determines which metadata is generated:

| Standard | VBI | VITS | VITC | Use Case |
|----------|-----|------|------|----------|
| `iec60857-1986` | ✓ (PAL) | ✓ | ✗ | PAL LaserDisc discs |
| `iec60856-1986` | ✓ (NTSC) | ✓ | ✗ | NTSC LaserDisc discs |
| `consumer-tape` | ✗ | ✗ | ✓ | VHS, Betamax, consumer tape |
| `none` | ✗ | ✗ | ✗ | Clean test signals only |

### VBI (Vertical Blanking Interval) - LaserDisc

VBI encodes picture numbers, chapter numbers, and timecodes on lines 16–18 in each field.

**Configuration (PAL example):**

```yaml
laserdisc:
  standard: "iec60857-1986"  # Enables LaserDisc VBI
  mode: "cav"                # CAV mode: picture numbers

sections:
  - name: "Content"
    duration: 1000
    biphase-vbi:
      disc_area: "programme-area"  # lead-in, programme-area, or lead-out
      picture_start: 1             # Starting picture number (optional, continues if omitted)
```

**CAV Mode (Picture Numbers):**

Picture numbers increment by 1 per frame:

```yaml
sections:
  - name: "Section 1"
    duration: 100
    biphase-vbi:
      picture_start: 1         # Pictures 1–100
  
  - name: "Section 2"
    duration: 50
    biphase-vbi:
      # picture_start omitted: continues as pictures 101–150
```

**CLV Mode (Timecode & Chapter):**

Timecode increments according to video frame rate (25 fps PAL, 29.97 fps NTSC):

```yaml
laserdisc:
  standard: "iec60857-1986"
  mode: "clv"

sections:
  - name: "Chapter 1"
    duration: 1500  # 60 seconds at 25fps
    biphase-vbi:
      chapter: 1
      timecode_start: "00:00:00:00"  # HH:MM:SS:FF format
  
  - name: "Chapter 1 Continued"
    duration: 750
    biphase-vbi:
      # chapter and timecode omitted: continues from 00:01:00:00
  
  - name: "Chapter 2"
    duration: 1500
    biphase-vbi:
      chapter: 2  # New chapter
      # timecode omitted: continues from previous timecode
```

### VITS (Vertical Interval Test Signals)

VITS signals are automatically added to lines 19/20 (PAL) or 19/20/282/283 (NTSC) when a LaserDisc standard is enabled:

```yaml
pipeline:
  metadata:
    generators:
      - type: "vits"
        enabled: true   # Automatic for IEC standards
```

VITS includes standard test waveforms (multiburst, UK national) per IEC specifications. Custom VITS is not currently supported.

### VITC (Vertical Interval Time Code) - Consumer Tape

VITC is used for consumer video formats (VHS, Betamax, etc.) instead of LaserDisc VBI:

```yaml
laserdisc:
  standard: "consumer-tape"  # Enables VITC on lines 14/16 (NTSC) or 19/21 (PAL)
  mode: "cav"                # or "clv"

sections:
  - name: "Consumer Tape"
    duration: 1800
    biphase-vbi:
      picture_start: 1        # or chapter/timecode_start for CLV
```

**VITC Placement:**
- **NTSC**: Lines 14 and 16 (1-indexed)
- **PAL**: Lines 19 and 21 (1-indexed)

**VITC Encoding:**
- Format: HH:MM:SS:FF (hours:minutes:seconds:frames)
- Encoding: Manchester (biphase) with sine-squared edge shaping
- Amplitude: 0–100 IRE (blanking to white)

### Example: Full Metadata Configuration

```yaml
output:
  filename: "laserdisc-master.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"  # PAL LaserDisc
  mode: "clv"                # CLV timecode mode

pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"   # Automatically configured per mode
        enabled: true
      - type: "vits"          # VITS test signals
        enabled: true
      - type: "color-burst"   # Color reference (always recommended)
        enabled: true

sections:
  - name: "Opening Leader"
    duration: 250
    source:
      type: "yuv422-image"
      file: "colorbars.raw"
    biphase-vbi:
      disc_area: "lead-in"
      chapter: 0
      timecode_start: "00:00:00:00"
  
  - name: "Main Content"
    duration: 3000  # 2 minutes at 25fps
    source:
      type: "mov-file"
      file: "content.mov"
    biphase-vbi:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:10:00"  # Offset for leader
```

---

## Stage 7: Post-Processing Effects (Phase 6)

Simulate tape artifacts and degradation on the final composite signal. All effects are optional and applied in sequence.

### Noise (Gaussian Tape Hiss)

Simulate tape hiss by adding white Gaussian noise:

```yaml
pipeline:
  effects:
    - type: "noise"
      enabled: true
      snr_db: 40.0     # Signal-to-Noise Ratio in dB
      seed: 42         # Optional: for reproducible results
```

**SNR Guide:**
- **45 dB**: Archive-quality (minimal noise)
- **40 dB**: High-quality consumer VCR
- **35 dB**: Typical consumer VCR
- **30 dB**: Worn equipment
- **25 dB**: Severely degraded tape

**Alternative: Direct Noise Level**

```yaml
pipeline:
  effects:
    - type: "noise"
      enabled: true
      noise_level_db: -40.0  # Direct noise level (alternative to snr_db)
```

### Dropouts (Tape Damage)

Simulate line dropouts from tape damage or media defects:

```yaml
pipeline:
  effects:
    - type: "dropout"
      enabled: true
      pattern: "random"      # "random", "periodic", or "specific-lines"
      density: 0.005         # 0.5% of lines affected
      seed: 42
```

**Dropout Patterns:**

```yaml
# Random pattern: 0.5% of lines randomly affected
- type: "dropout"
  pattern: "random"
  density: 0.005
  seed: 42

# Periodic pattern: dropout repeats every N lines
- type: "dropout"
  pattern: "periodic"
  density: 0.01      # Affected lines per period
  
# Specific lines: affect only listed lines
- type: "dropout"
  pattern: "specific-lines"
  lines: [50, 100, 150, 200]
```

**Density Guide:**
- 0.001: 0.1% of lines (1 in 1000)
- 0.005: 0.5% of lines (1 in 200)
- 0.01: 1% of lines (1 in 100)
- 0.05: 5% of lines (1 in 20)

### Phase Error (VCR Wobble)

Simulate VCR time-base errors and playback jitter:

```yaml
pipeline:
  effects:
    - type: "phase-error"
      enabled: true
      phase_jitter_samples: 10.0  # Maximum jitter in samples
      frequency_hz: 1.0           # Wobble frequency in Hz
      seed: 42
```

**Phase Error Guide:**
- **phase_jitter_samples**: Maximum deviation from nominal line timing (in samples at composite frequency)
  - 5: Minimal jitter (near-professional)
  - 10–20: Typical consumer VCR wobble
  - 30+: Severely degraded VCR
  
- **frequency_hz**: How fast the wobble cycles
  - 0.5–2.0: Slow periodic drift
  - 2.0–5.0: Typical VCR flutter
  - 10+: Rapid high-frequency flutter

### Complete Effect Example

```yaml
pipeline:
  effects:
    # Moderate tape hiss
    - type: "noise"
      enabled: true
      snr_db: 35.0
      seed: 12345
    
    # Random line dropouts
    - type: "dropout"
      enabled: true
      pattern: "random"
      density: 0.008     # 0.8% of lines
      seed: 54321
    
    # Periodic wobble
    - type: "phase-error"
      enabled: true
      phase_jitter_samples: 8.0
      frequency_hz: 2.5
      seed: 99999
```

---

## Output Configuration

### Basic Output Settings

```yaml
output:
  filename: "output.tbc"        # Output filename (required)
  format: "pal-composite"       # Output format (required)
  mode: "combined"              # Optional: combined (default), separate-yc
  writer: "tbc"                 # Optional: tbc (default), standard
  metadata_decoder: "encode-orc" # Optional: decoder string for metadata
```

### Output Formats

| Format | Resolution | Frame Rate | Description |
|--------|-----------|-----------|-------------|
| `pal-composite` | 720×576 | 25 fps | PAL composite video |
| `ntsc-composite` | 720×486 | 29.97 fps | NTSC composite video |
| `pal-yc` | 720×576 | 25 fps | PAL Y/C (luma/chroma separated) |
| `ntsc-yc` | 720×486 | 29.97 fps | NTSC Y/C (luma/chroma separated) |

### Output Modes

```yaml
# Mode 1: Combined (default) - Single .tbc file
output:
  filename: "output.tbc"
  format: "pal-composite"
  mode: "combined"
# Result: output.tbc

# Mode 2: Separate Y/C - Split luma and chroma
output:
  filename: "output.tbc"
  format: "pal-composite"
  mode: "separate-yc"
# Result: output.tbcy (luma) and output.tbcc (chroma)
```

### Writer Modes

```yaml
# TBC Writer (default) - Standard TBC with padding and metadata
output:
  writer: "tbc"           # Full TBC with metadata support

# Standard Writer - Raw field data for external tools
output:
  writer: "standard"      # Raw output for hackdac/ld-chroma-decoder
```

### Custom Metadata Decoder

Identify the tool that encoded this file in the metadata:

```yaml
output:
  filename: "output.tbc"
  metadata_decoder: "my-custom-encoder"  # Default: "encode-orc"
```

### Custom Video Levels (16-bit IRE Scale)

Override signal levels (0–65535 = 0–100 IRE):

```yaml
output:
  filename: "output.tbc"
  format: "ntsc-composite"
  
  video_levels:
    blanking_16b_ire: 15058   # Blanking level (0 IRE NTSC = −300mV)
    black_16b_ire: 17768      # Black level (7.5 IRE NTSC)
    white_16b_ire: 51200      # White level (100 IRE = 700mV)
```

**Default Values:**

| Property | PAL | NTSC |
|----------|-----|------|
| `blanking_16b_ire` | 17125 | 15058 |
| `black_16b_ire` | 17125 | 17768 |
| `white_16b_ire` | 54016 | 51200 |

---

## Complete Examples

### Example 1: Simple PAL Color Bars

```yaml
name: "PAL Color Bars Test"
description: "Quick test with color bars and VBI"

output:
  filename: "test.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "EBU Color Bars"
    duration: 100
    source:
      type: "yuv422-image"
      file: "colorbars-75.raw"
    biphase-vbi:
      picture_start: 1
```

### Example 2: Multi-Chapter LaserDisc

```yaml
name: "Educational LaserDisc"
description: "CLV disc with multiple chapters"

output:
  filename: "educational.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "clv"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
  metadata:
    generators:
      - type: "color-burst"
        enabled: true
      - type: "vits"
        enabled: true

sections:
  - name: "Leader"
    duration: 250
    source:
      type: "yuv422-image"
      file: "colorbars.raw"
    biphase-vbi:
      disc_area: "lead-in"
      chapter: 0
      timecode_start: "00:00:00:00"
  
  - name: "Chapter 1"
    duration: 3000
    source:
      type: "mov-file"
      file: "chapter1.mov"
    biphase-vbi:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:10:00"
  
  - name: "Chapter 2"
    duration: 2500
    source:
      type: "mov-file"
      file: "chapter2.mov"
    biphase-vbi:
      disc_area: "programme-area"
      chapter: 2
      # timecode continues from previous
```

### Example 3: Tape Simulation

```yaml
name: "Simulated VCR Tape"
description: "Encode with realistic tape artifacts"

output:
  filename: "simulated-vhs.tbc"
  format: "ntsc-composite"

laserdisc:
  standard: "consumer-tape"  # VITC instead of LaserDisc VBI
  mode: "clv"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
  
  effects:
    # Typical consumer VCR noise
    - type: "noise"
      enabled: true
      snr_db: 35.0
      seed: 11111
    
    # Random tape damage
    - type: "dropout"
      enabled: true
      pattern: "random"
      density: 0.005
      seed: 22222
    
    # VCR flutter
    - type: "phase-error"
      enabled: true
      phase_jitter_samples: 8.0
      frequency_hz: 2.0
      seed: 33333

sections:
  - name: "VHS Content"
    source:
      type: "mov-file"
      file: "content.mov"
      start_frame: 0
    biphase-vbi:
      chapter: 1
      timecode_start: "00:00:00:00"
```

### Example 4: Archive Quality without Effects

```yaml
name: "Archive Master"
description: "High-quality encoding without artifacts"

output:
  filename: "archive-master.tbc"
  format: "pal-composite"
  mode: "separate-yc"  # Separate luma/chroma for archival

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
      luma:
        enabled: true   # Extra filtering for archive
  
  metadata:
    generators:
      - type: "color-burst"
        enabled: true
      - type: "vits"
        enabled: true

sections:
  - name: "Archive Material"
    source:
      type: "mov-file"
      file: "archive.mov"
    biphase-vbi:
      picture_start: 1
```

---

## Workflow Examples

### Workflow 1: Digitize a Consumer VHS Tape

```yaml
name: "VHS Digitization"
description: "Convert captured VHS signal to TBC with VITC"

output:
  filename: "vhs-master.tbc"
  format: "ntsc-composite"

laserdisc:
  standard: "consumer-tape"
  mode: "clv"

sections:
  - name: "VHS Content"
    source:
      type: "mov-file"
      file: "vhs-capture.mov"
    biphase-vbi:
      chapter: 1
      timecode_start: "00:00:00:00"
```

### Workflow 2: Create Multi-Format Test Discs

```yaml
name: "Multi-Format Test Suite"
description: "Tests for PAL and NTSC encoding"

output:
  filename: "test-suite.tbc"
  format: "pal-composite"

laserdisc:
  standard: "none"  # No metadata for clean test signals
  mode: "none"

sections:
  - name: "Color Bars"
    duration: 100
    source:
      type: "png-image"
      file: "colorbars.png"
  
  - name: "Grayscale Ramp"
    duration: 100
    source:
      type: "png-image"
      file: "grayscale.png"
  
  - name: "Resolution Test"
    duration: 100
    source:
      type: "png-image"
      file: "resolution-chart.png"
```

### Workflow 3: Archive LaserDisc Content

```yaml
name: "LaserDisc Archive Project"
description: "Encode CAV disc with all test signals and quality filtering"

output:
  filename: "laserdisc-archive.tbc"
  format: "pal-composite"
  mode: "separate-yc"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
      luma:
        enabled: true

sections:
  - name: "Main Content"
    source:
      type: "mov-file"
      file: "disc-scan.mov"
    biphase-vbi:
      picture_start: 1
```

---

## Troubleshooting

### High-Frequency Artifacts on Color Bars

**Symptom**: Ringing or aliasing on sharp color transitions

**Solution**: Enable chroma filtering (default)

```yaml
pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true  # Enabled by default
```

### VITC Not Appearing

**Symptom**: No VITC timecode in output

**Solution**: Ensure `laserdisc.standard` is set to `consumer-tape`

```yaml
laserdisc:
  standard: "consumer-tape"  # Not iec60857-1986 or iec60856-1986
```

### Picture Numbers Not Incrementing

**Symptom**: Picture numbers restart in each section

**Solution**: Omit `picture_start` in later sections to continue automatically

```yaml
sections:
  - name: "Section 1"
    biphase-vbi:
      picture_start: 1      # Explicit start
  
  - name: "Section 2"
    biphase-vbi:
      # picture_start omitted: continues from previous
```

### Effects Not Visible

**Symptom**: Noise/dropouts/phase errors not appearing in output

**Solution**: Ensure effects are `enabled: true` and the output is being checked with appropriate tools

```yaml
pipeline:
  effects:
    - type: "noise"
      enabled: true   # Must be true
```

---

## Command Line Usage

```bash
# Encode a project
./encode-orc project.yaml

# Project file must:
# - Have .yaml or .yml extension
# - Contain valid YAML syntax
# - Include name, output, laserdisc, and sections

# Output locations:
# - output/filename.tbc         (main video file)
# - output/filename.tbc.db      (SQLite metadata)
# - output/filename.tbc.json    (JSON metadata)
```

---

## Video System Quick Reference

### PAL (Phase Alternating Line)
- **Dimensions**: 720×576 (4:3 aspect)
- **Frame Rate**: 25 fps
- **Chroma Filter**: 1.3 MHz
- **Standard**: `pal-composite` or `pal-yc`
- **IEC Standard**: `iec60857-1986` (LaserDisc) or `consumer-tape` (VHS)
- **VITC Lines**: 19, 21 (1-indexed)

### NTSC (National Television System Committee)
- **Dimensions**: 720×486 (4:3 aspect)
- **Frame Rate**: 29.97 fps (or ~30 fps)
- **Chroma Filter**: 600 kHz
- **Standard**: `ntsc-composite` or `ntsc-yc`
- **IEC Standard**: `iec60856-1986` (LaserDisc) or `consumer-tape` (VHS/Betamax)
- **VITC Lines**: 14, 16 (1-indexed)

---

## Next Steps

1. **Start Simple**: Begin with a basic YUV422 or PNG project (Example 1)
2. **Add Metadata**: Enable LaserDisc VBI or VITC (Example 2)
3. **Apply Filtering**: Use preprocessing to clean up artifacts
4. **Simulate Tape**: Add effects for realistic tape simulation (Example 3)
5. **Fine-Tune**: Adjust SNR, dropout density, and other parameters as needed

For detailed technical reference, see [yaml-project-format.md](yaml-project-format.md).

