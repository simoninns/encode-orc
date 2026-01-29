# Phase 4 Pipeline Metadata Configuration

## Overview

Phase 4 introduces a new, explicit pipeline-based configuration for metadata generators. Instead of relying on the old `laserdisc.standard` field to implicitly enable generators, you now explicitly configure which generators to use and how.

## Benefits

1. **Explicit Configuration**: Clear visibility into which metadata generators are active
2. **Composable**: Mix and match generators as needed
3. **Flexible**: Configure generators with custom line placements and parameters
4. **Extensible**: Easy to add new generator types in future phases

## Configuration Structure

```yaml
pipeline:
  metadata:
    generators:
      - type: "generator-type"
        enabled: true
        # ... generator-specific configuration
```

## Available Generator Types

### 1. `biphase-vbi` - LaserDisc VBI Frame Numbers

Encodes 24-bit VBI data onto lines 16, 17, and 18 using biphase (Manchester) encoding.

**Configuration**:
```yaml
- type: "biphase-vbi"
  enabled: true
  lines: [15, 16, 17]  # 0-indexed line numbers
```

**Use Cases**: LaserDisc CAV/CLV formats

### 2. `vitc` - Consumer Tape Timecode

Encodes VITC (Vertical Interval Time Code) on specified lines for consumer tape formats.

**Configuration**:
```yaml
- type: "vitc"
  enabled: true
  lines: [18, 20]  # 0-indexed (lines 19, 21 in 1-indexed)
  start_frame_offset: 0  # Optional: offset for timecode start
```

**Use Cases**: Consumer tape formats (VHS, Betamax, etc.)

### 3. `vits-pal` - PAL Vertical Interval Test Signals

Generates PAL VITS test signals on configured lines.

**Configuration**:
```yaml
- type: "vits-pal"
  enabled: true
  signals:
    - line: 12       # 0-indexed line number
      field: 1       # Field number (1 or 2)
      signal: "multiburst"  # Signal type
    - line: 18
      field: 1
      signal: "uk-national"
    - line: 12
      field: 2
      signal: "itu-its"
    - line: 18
      field: 2
      signal: "itu-composite"
```

**Available PAL Signals**:
- `"multiburst"` - ITU Multiburst Test Signal
- `"uk-national"` - UK PAL National Test Signal #1
- `"itu-its"` - ITU Combination ITS
- `"itu-composite"` - ITU Composite Test Signal

### 4. `vits-ntsc` - NTSC Vertical Interval Test Signals

Generates NTSC VITS test signals on configured lines.

**Configuration**:
```yaml
- type: "vits-ntsc"
  enabled: true
  signals:
    - line: 17
      field: 1
      signal: "itu-composite"
    - line: 17
      field: 2
      signal: "itu-its"
```

**Available NTSC Signals**:
- `"itu-composite"` - NTC7 Composite Test Signal
- `"itu-its"` - NTC7 Combination Test Signal
- `"multiburst"` - Multiburst (uses NTC7 composite)

**Note**: `"uk-national"` is PAL-specific and not available for NTSC.

### 5. `color-burst` - Color Burst Reference Signal

Adds color burst reference signal to all active video and VBI lines.

**Configuration**:
```yaml
- type: "color-burst"
  enabled: true
```

**Use Cases**: Always needed for composite video formats.

## Complete Examples

### LaserDisc CAV (PAL)

```yaml
name: "PAL LaserDisc CAV"
output:
  filename: "pal-cav.tbc"
  format: "pal-composite"

pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]
      - type: "vits-pal"
        enabled: true
        signals:
          - { line: 12, field: 1, signal: "multiburst" }
          - { line: 18, field: 1, signal: "uk-national" }
          - { line: 12, field: 2, signal: "itu-its" }
          - { line: 18, field: 2, signal: "itu-composite" }
      - type: "color-burst"
        enabled: true

sections:
  - name: "Content"
    source:
      type: "yuv422-image"
      file: "image.yuv"
    duration: 100
    laserdisc:
      picture_start: 1000
      vbi:
        enabled: true
      vits:
        enabled: true
```

### Consumer Tape (NTSC)

```yaml
name: "NTSC Consumer Tape"
output:
  filename: "ntsc-tape.tbc"
  format: "ntsc-composite"

pipeline:
  metadata:
    generators:
      - type: "vitc"
        enabled: true
        lines: [18, 20]
      - type: "color-burst"
        enabled: true

sections:
  - name: "Content"
    source:
      type: "yuv422-image"
      file: "image.yuv"
    duration: 150
```

## Migration from Old Format

**Old Format** (DEPRECATED):
```yaml
laserdisc:
  standard: "iec60857-1986"
```

**New Format**:
```yaml
pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
      - type: "vits-pal"
        enabled: true
        signals: [...]
      - type: "color-burst"
        enabled: true
```

## Line Number Indexing

**Important**: All line numbers in the pipeline configuration are **0-indexed**.

| 1-Indexed (Standard) | 0-Indexed (YAML) | Description |
|---------------------|------------------|-------------|
| 16, 17, 18          | 15, 16, 17       | LaserDisc VBI lines |
| 19, 21              | 18, 20           | VITC lines (consumer tape) |
| 13, 19              | 12, 18           | Common VITS lines |

## Implementation Status

Phase 4 has created the infrastructure for pipeline-based metadata generation:

✅ Created `PipelineMetadataGenerator` base class
✅ Implemented all generator types:
  - `BiphaseVBIGenerator`
  - `VITCPipelineGenerator`
  - `PALVITSPipelineGenerator`
  - `NTSCVITSPipelineGenerator`
  - `ColorBurstPipelineGenerator`
✅ Updated YAML configuration structure
✅ Updated YAML parser to support new format

🚧 **TODO** (remaining work):
- Integrate generators into `VideoEncoder` (Phase 8)
- Remove old `laserdisc.standard` logic
- Migrate existing test projects
- Add validation for pipeline configuration
- Test complete pipeline end-to-end

## Next Steps

The next phase of work will integrate these generators into the `VideoEncoder` class, replacing the current monolithic encoding logic with the new composable pipeline.

