---
title: Output Formats
layout: default
parent: Technical
nav_order: 1
---

# Output Formats

encode-orc generates video files in TBC (Time Base Corrected) format, compatible with decode-orc and other video decoding tools.

## TBC Format Overview

TBC files are field-based video files containing raw video data. Each field is stored sequentially without compression.

### File Specifications

| Aspect | Value |
|--------|-------|
| Bit Depth | 8-bit per component |
| Field Order | Top field first (for most formats) |
| Color Space | YUV 4:2:2 |
| No Compression | Raw video data |

## Composite Mode

Generates traditional composite video representation.

### Output Files
- `output.tbc` - Composite field-based video file
- `output.tbc.db` - SQLite metadata database

### File Structure

**TBC File:**
- Stores composite video as 8-bit grayscale
- Each field stored sequentially
- One byte per pixel

**Metadata Database:**
- VBI (Vertical Blanking Interval) data
- Frame/field numbering
- Timecode information
- Dropout detection data

### Characteristics
- Single file containing video
- Simpler file organization
- Smaller file sizes than Y/C mode
- Traditional video representation

### Usage
```yaml
output:
  format: "composite"
```

## Y/C Mode (S-Video)

Separates luma (brightness) and chroma (color) components, simulating S-Video output.

### Output Files
- `output.tbcy` - Luma (Y) component
- `output.tbcc` - Chroma (C) component
- `output.tbc.db` - SQLite metadata database

### File Structure

**Luma File (.tbcy):**
- 8-bit grayscale image data
- Full resolution (720×480 or 720×576)
- Higher quality brightness information

**Chroma File (.tbcc):**
- 8-bit chroma data
- 4:2:2 chroma subsampling
- Color information separate from brightness

**Metadata Database:**
- Same as composite mode
- Applies to both luma and chroma

### Characteristics
- Better separation of color components
- Improved color accuracy
- Larger file sizes than composite
- More closely simulates S-Video hardware

### Usage
```yaml
output:
  format: "yc"
```

## Metadata Database Format

The `.tbc.db` SQLite database contains:

### VBI (Vertical Blanking Interval) Data
- Teletext information
- Closed caption data
- Frame/field identification

### Frame Information
- Timecode
- Frame numbering
- Field order indicators
- Dropout metrics

### Example Query
```sql
SELECT * FROM vbi_data WHERE field_number > 100;
```

## File Size Reference

Approximate sizes for different configurations:

| Format | PAL (1 hour) | NTSC (1 hour) |
|--------|--------------|---------------|
| Composite | ~1.8 GB | ~1.6 GB |
| Y/C Mode | ~2.7 GB | ~2.4 GB |
| Metadata DB | ~50 MB | ~50 MB |

## Working with Output Files

### Using with decode-orc
```bash
# Decode composite TBC file
decode-orc output.tbc --output decoded.mov

# Decode Y/C TBC files
decode-orc output.tbcy output.tbcc --output decoded.mov
```

### Inspecting Metadata
```bash
# Query metadata database
sqlite3 output.tbc.db "SELECT COUNT(*) FROM vbi_data;"
```

### File Verification
```bash
# Check file sizes
ls -lh output.*

# Verify file integrity
file output.tbc
```

## Choosing Output Format

### Use Composite When:
- Testing basic decoding functionality
- Working with limited storage
- Simulating legacy composite video
- File size is a concern

### Use Y/C When:
- Testing color decoder accuracy
- Simulating S-Video hardware
- Need better color reproduction
- Advanced color correction testing

---

For more information on decode-orc usage, see its documentation.
