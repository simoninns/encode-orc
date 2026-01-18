# MP4 File Support

## Overview

The MP4 loader enables encode-orc to load and encode frames from standard MP4 video files with normal color space (typically Rec.709 or similar). This is similar to the MOV loader, but specifically designed for MP4 files encoded with common codecs like H.264 (AVC) or H.265 (HEVC).

## Differences from MOV Loader

| Feature | MOV Loader | MP4 Loader |
|---------|------------|------------|
| Target files | Professional video files (v210, ProRes) | Standard consumer/web video files |
| Color space | Studio range YUV (typically YUV422P10LE) | Normal color space (typically Rec.709) |
| Pixel format | YUV422P10LE (10-bit 4:2:2) | YUV420P (8-bit 4:2:0) |
| Deinterlacing | Applied by default (yadif filter) | Not applied (assumes progressive) |
| Typical use | Professional broadcast/mastering files | Consumer video, web downloads, screen recordings |

## Supported Formats

The MP4 loader can handle any video codec that ffmpeg can decode to YUV420P, including:

- H.264/AVC (most common)
- H.265/HEVC
- MPEG-4 Part 2
- VP9 (in MP4 container)
- And many others

## Color Space Conversion

The MP4 loader automatically converts 8-bit studio range YUV420P to 10-bit studio range YUV444P16:

- **Luma (Y)**: 16-235 (8-bit) → 64-940 (10-bit)
- **Chroma (U/V)**: 16-240 (8-bit) → 64-960 (10-bit)

This ensures compatibility with the encoder's internal processing pipeline.

## Frame Rate Requirements

Like the MOV loader, the MP4 loader validates that the video file's frame rate matches the target video system:

- **PAL**: 25 fps (±0.1 fps tolerance)
- **NTSC**: 29.97 fps (±0.1 fps tolerance)

## Usage in YAML Projects

### Basic PAL Example

```yaml
name: "PAL MP4 Test"
description: "Encode video from MP4 file"

output:
  filename: "output.tbc"
  format: "pal-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "Main Video"
    duration: 100  # Optional - will use all frames if omitted
    source:
      type: "mp4-file"
      file: "input-video.mp4"
      start_frame: 0  # Optional - defaults to 0
    laserdisc:
      picture_start: 1
```

### Basic NTSC Example

```yaml
name: "NTSC MP4 Test"
description: "Encode video from MP4 file for NTSC"

output:
  filename: "output.tbc"
  format: "ntsc-composite"

laserdisc:
  standard: "iec60857-1986"
  mode: "cav"

sections:
  - name: "Main Video"
    source:
      type: "mp4-file"
      file: "input-video.mp4"
    laserdisc:
      picture_start: 1
```

### Multiple Sections

You can combine MP4 files with other source types:

```yaml
sections:
  # Start with a title card
  - name: "Title Card"
    duration: 50
    source:
      type: "png-image"
      file: "title.png"
    laserdisc:
      picture_start: 1
  
  # Main video content from MP4
  - name: "Main Video"
    duration: 250
    source:
      type: "mp4-file"
      file: "video.mp4"
      start_frame: 0
    laserdisc:
      picture_start: 51
  
  # End credits
  - name: "Credits"
    duration: 75
    source:
      type: "png-image"
      file: "credits.png"
    laserdisc:
      picture_start: 301
```

## Dependencies

The MP4 loader requires:

- **ffmpeg**: For decoding MP4 files
- **ffprobe**: For extracting video metadata

These should be installed and available in your system PATH.

## Technical Details

### Frame Extraction Process

1. ffprobe retrieves video dimensions, frame count, and frame rate
2. ffmpeg extracts the requested frames using the `select` filter
3. Frames are decoded to raw YUV420P format and saved to a temporary file
4. The loader reads the YUV data and converts it to YUV444P16
5. Temporary files are automatically cleaned up

### Padding

If the MP4 file dimensions are smaller than the target video format (e.g., 640x480 MP4 for PAL's 720x576), the loader automatically pads the video with black/neutral values, distributing padding evenly on left and right sides.

### Performance Considerations

- Frame extraction uses temporary files in `/tmp`
- Each extraction creates files named `/tmp/encode_orc_mp4_<pid>.yuv`
- For large videos, ensure sufficient disk space in `/tmp`
- FFmpeg is invoked as a subprocess for each section

## Troubleshooting

### "MP4 frame rate mismatch" Error

Your MP4 file's frame rate doesn't match the target system. Convert the video to the correct frame rate first:

```bash
# Convert to 25 fps for PAL
ffmpeg -i input.mp4 -r 25 -c:v libx264 -preset slow -crf 18 output-pal.mp4

# Convert to 29.97 fps for NTSC
ffmpeg -i input.mp4 -r 29.97 -c:v libx264 -preset slow -crf 18 output-ntsc.mp4
```

### "MP4 dimension mismatch" Error

The MP4 file dimensions don't match the expected video system dimensions. Either:

1. Resize the video to match (720x576 for PAL, 720x480 for NTSC)
2. Use a smaller resolution that will be padded automatically

### FFmpeg Not Found

Install ffmpeg on your system:

```bash
# Ubuntu/Debian
sudo apt install ffmpeg

# macOS (Homebrew)
brew install ffmpeg

# Fedora/RHEL
sudo dnf install ffmpeg
```

## See Also

- [MOV File Support](MOV-File-Support.md) - For professional v210/ProRes files
- [YAML Project Format](yaml-project-format.md) - Complete YAML syntax reference
- [Test Signals](Test-Signals.md) - Using test patterns
