# encode-orc User Guide

## Overview

`encode-orc` is a video encoding application that converts digital video sources into composite video signals (TBC format) with optional artifacts, metadata, and signal effects. The application is configured entirely through YAML project files.

```bash
./encode-orc project.yaml
```

---

## Pipeline Architecture

The encode-orc pipeline processes video through the following stages:

```
Stage 1: Source Loading
  ├─ Read video from disk (YUV422, PNG, MOV, MP4)
  └─ Decode to raw YUV 4:2:2 frames

Stage 2: Preprocessing
  ├─ Apply chroma low-pass filter (optional)
  └─ Apply luma low-pass filter (optional)

Stage 3: Composite Encoding
  ├─ Encode YUV 4:2:2 to composite video
  ├─ Insert color burst reference signals
  └─ Generate color-based VBI/VITS/VITC signals (including metadata)

Stage 4: Post-Processing
  ├─ Apply Gaussian noise (noise simulation)
  ├─ Apply line dropouts (RF breaks)
  └─ Apply phase jitter (jitter/wobble simulation)

Stage 5: Output
  ├─ Write TBC file(s) in ld-decode TBC format
  └─ Generate SQLite metadata database in ld-decode format
```

Each stage is independently configurable.

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

## Stage 2: Preprocessing Filters

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

## Stage 3: Composite Encoding & line signals

The line signal generation (VBI, VITS, VITC, color burst) is configured entirely through the `pipeline.metadata.generators` list. Each generator you enable adds specific vertical interval signals to the output.

### Available Metadata Generators

#### Color Burst (Always Recommended)

Adds color burst reference signal to enable proper chroma demodulation:

```yaml
pipeline:
  metadata:
    generators:
      - type: "color-burst"
        enabled: true
```

#### Biphase VBI (LaserDisc Picture Numbers / Timecode)

Encodes picture numbers or timecode on specified lines using two-field absolute line numbering.

For consistency with VITS generators, biphase-vbi uses absolute frame line numbers (1-indexed in YAML):

**NTSC Configuration:**
```yaml
pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [16, 17, 18, 278, 279, 280]  # Field 1 and Field 2 lines (1-indexed)
        format: "picture-number" # or "timecode"
```

**PAL Configuration:**
```yaml
pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [17, 18, 19, 330, 331, 332]  # Field 1 and Field 2 lines (1-indexed)
        format: "picture-number" # or "timecode"
```

**Line Numbering:**
- Lines are specified as 1-indexed absolute frame line numbers
- NTSC: 525 total lines (1-525), with Field 1 = lines 1-263, Field 2 = lines 264-525
- PAL: 625 total lines (1-625), with Field 1 = lines 1-312, Field 2 = lines 313-625
- Specify 3 lines per field for proper VBI encoding (vbi0, vbi1, vbi2 per field)

**VBI Encodes:**
- **Picture numbers**: 1-frame increment per frame (CAV mode)
- **Timecode**: HH:MM:SS:FF with chapter number (CLV mode)
- Each field contains its own VBI data (separate picture number or timecode)

**Configuration with sections:**

```yaml
sections:
  - name: "Section 1"
    duration: 100
    biphase-vbi:
      disc_area: "programme-area"
      picture_start: 1     # CAV: start picture number
      # OR for CLV:
      # chapter: 1
      # timecode_start: "00:00:00:00"
```

#### VITS (Vertical Interval Test Signals)

Adds IEC-standard test signals on user-specified lines.

**PAL VITS Configuration:**
```yaml
pipeline:
  metadata:
    generators:
      - type: "vits-pal"
        enabled: true
        signals:
          - line: 13
            signal: "multiburst"
          - line: 19
            signal: "uk-national"
          - line: 325
            signal: "itu-combination"
          - line: 331
            signal: "itu-composite"
```

**Available PAL Signals:**
- `"multiburst"` - ITU Multiburst Test Signal
- `"uk-national"` - UK PAL National Test Signal #1
- `"itu-combination"` - ITU Combination ITS
- `"itu-composite"` - ITU Composite Test Signal

**NTSC VITS Configuration:**
```yaml
pipeline:
  metadata:
    generators:
      - type: "vits-ntsc"
        enabled: true
        signals:
          - line: 13
            signal: "ntc7-composite"
          - line: 19
            signal: "vir"
          - line: 275
            signal: "ntc7-combination"
          - line: 281
            signal: "vir"
```

**Available NTSC Signals:**
- `"itu-composite"` - NTC-7 Composite Test Signal
- `"itu-combination"` - NTC-7 Combination Test Signal
- `"multiburst"` - Multiburst (uses NTC-7 composite)

**Line Numbering:**
- Line numbers are 1-indexed and absolute (matching video specifications)
- PAL: 1-625 (field 1: 1-313, field 2: 314-625)
- NTSC: 1-525 (field 1: 1-263, field 2: 264-525)
- The field is automatically determined from the line number

#### VITC (Vertical Interval Time Code)

Encodes timecode on lines for consumer tape formats:

```yaml
pipeline:
  metadata:
    generators:
      - type: "vitc"
        enabled: true
        lines: [19, 21]           # PAL: 19, 21; NTSC: 14, 16
        start_frame_offset: 0     # Optional timecode offset
```

**VITC Placement:**
- **PAL**: Lines 19, 21 (1-indexed)
- **NTSC**: Lines 14, 16 (1-indexed)
- Encodes: HH:MM:SS:FF (hours:minutes:seconds:frames)
- User bits: set to 0 (reserved)

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
  mode: "clv"                # CLV timecode mode

pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"   # Configured per mode
        enabled: true
        lines: [15, 16, 17]   # 0-indexed
        format: "timecode"    # CLV uses timecode
      - type: "vits-pal"      # PAL VITS test signals
        enabled: true
        signals:
          - { line: 13, signal: "multiburst" }
          - { line: 19, signal: "uk-national" }
          - { line: 325, signal: "itu-combination" }
          - { line: 331, signal: "itu-composite" }
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

## Stage 4: Post-Processing Effects

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
      - type: "vits-pal"
        enabled: true
        signals:
          - { line: 13, signal: "multiburst" }
          - { line: 19, signal: "uk-national" }
          - { line: 325, signal: "itu-combination" }
          - { line: 331, signal: "itu-composite" }

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
      - type: "vits-pal"
        enabled: true
        signals:
          - { line: 13, signal: "multiburst" }
          - { line: 19, signal: "uk-national" }
          - { line: 325, signal: "itu-combination" }
          - { line: 331, signal: "itu-composite" }

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

## LaserDisc Format Configurations

The four common LaserDisc formats each require specific metadata generator configurations:

### PAL-CAV (IEC 60857-1986)

**Format**: PAL LaserDisc with CAV picture numbering  
**Use**: PAL region discs with still-frame capability

```yaml
output:
  format: "pal-composite"

laserdisc:
  mode: "cav"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true         # 1.3 MHz chroma filter
  
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]   # Lines 16-18 (1-indexed): biphase VBI
        format: "picture-number"
      
      - type: "vits-pal"      # PAL VITS test signals
        enabled: true
        signals:              # User-configured signals and placement
          - line: 18          # Line 19 (1-indexed)
            field: 1
            signal: "multiburst"
          - line: 19          # Line 20 (1-indexed)
            field: 1
            signal: "uk-national"
          - line: 331         # Line 332 (1-indexed)
            field: 2
            signal: "itu-combination"
          - line: 332         # Line 333 (1-indexed)
            field: 2
            signal: "itu-composite"
      
      - type: "color-burst"
        enabled: true

sections:
  - name: "Section 1"
    biphase-vbi:
      disc_area: "programme-area"
      picture_start: 1        # Picture 1
```

**Metadata Lines**:
- Lines 16-18: Biphase-encoded picture numbers (e.g., frame 1 = picture 1, 2, 3...)
- Lines 19-20: VITS test signals (multiburst, UK National)
- All active lines: Color burst reference

---

### PAL-CLV (IEC 60857-1986)

**Format**: PAL LaserDisc with CLV timecode  
**Use**: Extended play PAL discs (timecode, not picture numbers)

```yaml
output:
  format: "pal-composite"

laserdisc:
  mode: "clv"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
  
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]   # Lines 16-18 (1-indexed)
        format: "timecode"    # CLV uses timecode
      
      - type: "vits-pal"
        enabled: true
        signals:
          - { line: 13, signal: "multiburst" }
          - { line: 19, signal: "uk-national" }
          - { line: 325, signal: "itu-combination" }
          - { line: 331, signal: "itu-composite" }

      - type: "color-burst"
        enabled: true

sections:
  - name: "Section 1"
    biphase-vbi:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:00:00"
```

**Metadata Lines**:
- Lines 16-18: Biphase-encoded timecode and chapter markers
- Lines 19-20: VITS test signals
- All active lines: Color burst reference

---

### NTSC-CAV (IEC 60856-1986)

**Format**: NTSC LaserDisc with CAV picture numbering  
**Use**: NTSC region discs with still-frame capability

```yaml
output:
  format: "ntsc-composite"

laserdisc:
  mode: "cav"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true         # 600 kHz chroma filter
  
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]   # Lines 16-18 (1-indexed)
        format: "picture-number"
      
      - type: "vits-ntsc"     # NTSC VITS test signals
        enabled: true
        signals:
          - line: 13
            signal: "ntc7-composite"
          - line: 19
            signal: "vir"
          - line: 275
            signal: "ntc7-combination"
          - line: 281
            signal: "vir"
      
      - type: "color-burst"
        enabled: true

sections:
  - name: "Section 1"
    biphase-vbi:
      disc_area: "programme-area"
      picture_start: 1
```

**Metadata Lines**:
- Lines 16-18: Biphase-encoded picture numbers
- Lines 19-20 (field 1) / 282-283 (field 2): VITS test signals (NTC-7)
- All active lines: Color burst reference

---

### NTSC-CLV (IEC 60856-1986)

**Format**: NTSC LaserDisc with CLV timecode  
**Use**: Extended play NTSC discs (timecode, not picture numbers)

```yaml
output:
  format: "ntsc-composite"

laserdisc:
  mode: "clv"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
  
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]   # Lines 16-18 (1-indexed)
        format: "timecode"
      
      - type: "vits-ntsc"
        enabled: true
        signals:
          - line: 13
            signal: "ntc7-composite"
          - line: 19
            signal: "vir"
          - line: 275
            signal: "ntc7-combination"
          - line: 281
            signal: "vir"
      
      - type: "color-burst"
        enabled: true

sections:
  - name: "Section 1"
    biphase-vbi:
      disc_area: "programme-area"
      chapter: 1
      timecode_start: "00:00:00:00"
```

**Metadata Lines**:
- Lines 16-18: Biphase-encoded timecode and chapter markers
- Lines 19-20 (field 1) / 282-283 (field 2): VITS test signals
- All active lines: Color burst reference

---

### Format Summary

| Format | System | Mode | Lines 16-18 | VITS Signals (user-configurable) | Chroma Filter |
|--------|--------|------|-------------|----------------------------------|---------------|
| **PAL-CAV** | PAL | `cav` | Picture numbers | Multiburst, UK National, ITU signals | 1.3 MHz |
| **PAL-CLV** | PAL | `clv` | Timecode/chapter | Multiburst, UK National, ITU signals | 1.3 MHz |
| **NTSC-CAV** | NTSC | `cav` | Picture numbers | NTC-7 composite/combo | 600 kHz |
| **NTSC-CLV** | NTSC | `clv` | Timecode/chapter | NTC-7 composite/combo | 600 kHz |

**Note**: VITS signals and their line placement must be explicitly configured via the `signals` array in `vits-pal` or `vits-ntsc` generator configuration. Line numbers are 1-indexed absolute values (1-625 for PAL, 1-525 for NTSC) matching video specifications. The field is automatically determined from the line number. See examples above for standard LaserDisc placement.

**All formats include**:
- Color burst reference signal on all active video lines
- Optional post-processing effects (noise, dropouts, phase errors)
- Optional preprocessing filters (chroma/luma low-pass)

For consumer tape formats (VHS, Betamax), see [metadata-standards-reference.md](metadata-standards-reference.md#consumer-tape-vhs-betamax-video8).

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
- **Metadata**: See [metadata-standards-reference.md](metadata-standards-reference.md) for generator configurations
- **VITC Lines**: 19, 21 (1-indexed)

### NTSC (National Television System Committee)
- **Dimensions**: 720×486 (4:3 aspect)
- **Frame Rate**: 29.97 fps (or ~30 fps)
- **Chroma Filter**: 600 kHz
- **Standard**: `ntsc-composite` or `ntsc-yc`
- **Metadata**: See [metadata-standards-reference.md](metadata-standards-reference.md) for generator configurations
- **VITC Lines**: 14, 16 (1-indexed)

---

## Next Steps

1. **Start Simple**: Begin with a basic YUV422 or PNG project (Example 1)
2. **Add Metadata**: Enable LaserDisc VBI or VITC (Example 2)
3. **Apply Filtering**: Use preprocessing to clean up artifacts
4. **Simulate Tape**: Add effects for realistic tape simulation (Example 3)
5. **Fine-Tune**: Adjust SNR, dropout density, and other parameters as needed

For detailed technical reference, see [yaml-project-format.md](yaml-project-format.md).

