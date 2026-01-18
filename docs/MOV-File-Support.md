# MOV File Support in encode-orc

## Overview

encode-orc now supports MOV files as input sources in YAML project files. This allows you to encode frame-accurate studio-level video files (v210, ProRes, etc.) directly without needing to convert them to raw YUV422 format first.

## Features

- **Native MOV support**: Uses ffmpeg to decode MOV files on-the-fly
- **Frame-accurate extraction**: Specify exact start frame and duration
- **Multiple formats supported**: Any video format that ffmpeg can decode (v210, ProRes, H.264, etc.)
- **Automatic dimension validation**: Ensures MOV dimensions match the target video system (PAL/NTSC)
- **Moving video content**: Perfect for testcards and other video with motion

## Requirements

- ffmpeg and ffprobe must be installed and available in your PATH
- MOV files must match the target video system dimensions:
  - PAL: 720×576
  - NTSC: 720×486

## YAML Configuration

### Basic Example

```yaml
name: "PAL Testcard from MOV"
description: "Encoding PAL testcards with moving elements"

output:
  filename: "output.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "Test Sequence"
    duration: 100  # Encode 100 frames (4 seconds at 25fps)
    source:
      type: "mov-file"
      file: "path/to/video.mov"
      start_frame: 0  # Optional: start from frame 0 (default)
    laserdisc:
      picture_start: 1
```

### Advanced: Multi-Section with Different Segments

```yaml
sections:
  # First 50 frames
  - name: "Opening"
    duration: 50
    source:
      type: "mov-file"
      file: "testcard.mov"
      start_frame: 0
    laserdisc:
      picture_start: 1
  
  # Next 50 frames from same file
  - name: "Middle"
    duration: 50
    source:
      type: "mov-file"
      file: "testcard.mov"
      start_frame: 50
    # LaserDisc picture numbers continue (51-100)
  
  # Mix with other source types
  - name: "Still Frame"
    duration: 25
    source:
      type: "png-image"
      file: "testcard.png"
```

## Implementation Details

### MOV Loader

The `MOVLoader` class (`include/mov_loader.h`, `src/mov_loader.cpp`) handles:
- Opening and probing MOV files using ffprobe
- Extracting frames using ffmpeg to temporary YUV422P10LE format
- Converting YUV422P10LE planar data to YUV444P16 frame buffers
- Automatic cleanup of temporary files

### Frame Extraction Process

1. **Probe**: ffprobe gets video dimensions and frame count
2. **Extract**: ffmpeg extracts requested frames to temporary YUV file
3. **Convert**: YUV422P10LE planar data converted to YUV444P16 for encoder
4. **Cleanup**: Temporary files automatically removed

### Performance Considerations

- Frame extraction is done in batch per section (not frame-by-frame)
- Temporary YUV files are stored in `/tmp/` directory
- Memory usage scales with section duration (all frames loaded at once)

## Comparison with convert_mov_to_yuv422.py

| Feature | convert_mov_to_yuv422.py | Native MOV Support |
|---------|-------------------------|-------------------|
| Purpose | Extract single frame to YUV422 raw | Extract multiple frames on-the-fly |
| Output | Raw YUV422 file | Directly to TBC |
| Frame count | 1 frame only | Any number of frames |
| Workflow | Two-step (convert, then encode) | One-step (encode directly) |
| Storage | Requires raw file on disk | No intermediate files |

The conversion script is still useful for:
- Pre-converting MOV files to raw YUV422 for archival
- Debugging color/format issues
- Creating single-frame test images

## Troubleshooting

### "ffmpeg not found" or "ffprobe not found"
- Install ffmpeg: `sudo apt install ffmpeg` (Ubuntu/Debian) or `brew install ffmpeg` (macOS)

### "MOV dimension mismatch"
- Ensure your MOV file is exactly 720×576 (PAL) or 720×486 (NTSC)
- Check dimensions with: `ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 file.mov`

### "Requested frame range exceeds video length"
- Check total frame count: `ffprobe -v error -select_streams v:0 -show_entries stream=nb_frames -of csv=p=0 file.mov`
- Ensure `start_frame + duration` doesn't exceed available frames

## Example

See `test-projects/pal-mov-test.yaml` for a working example using the test MOV file.
