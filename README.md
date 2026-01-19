# encode-orc

Encoder for decode-orc (for making test TBC/Metadata files)

## Overview

encode-orc is a C++17 command-line application that generates test data for [decode-orc](https://github.com/simoninns/decode-orc). It produces field-based video files in the TBC (Time Base Corrected) format along with accompanying metadata, simulating the output of RF decoding applications such as ld-decode and vhs-decode used in the LaserDisc and analog video preservation community.

The application uses YAML project files to define encoding configurations, supporting multiple input formats and output modes with both video-embedded and SQLite-based metadata generation.

## Purpose

This tool enables developers and testers to create synthetic test data for validating video decoding pipelines without requiring physical hardware or RF captures. It's particularly useful for:

- Testing decode-orc and ld-decode tool chains
- Validating color decoder implementations
- Creating reproducible test cases
- Benchmarking decoder performance
- Educational purposes and algorithm development

## Supported Output Formats

encode-orc can generate the following output formats:

### PAL (576i, 25 fps)
- **Composite**: `video.tbc` + `video.tbc.db`
- **Y/C (S-Video)**: `video.tbcy` + `video.tbcc` + `video.tbc.db`

### NTSC (480i, 29.97 fps)
- **Composite**: `video.tbc` + `video.tbc.db`
- **Y/C (S-Video)**: `video.tbcy` + `video.tbcc` + `video.tbc.db`

Where:
- `.tbc` = Composite field-based video file
- `.tbcy` = Luma (Y) field-based video file
- `.tbcc` = Chroma (C) field-based video file
- `.tbc.db` = SQLite metadata database (ld-decode format)

## Features

### Current
- **YAML-based project configuration** - Define multi-section video projects in declarative configuration files
- **Multiple input formats**:
  - YUV422 raw image files
  - PNG image files
  - QuickTime MOV files
  - MP4 video files
- **PAL and NTSC encoding** - Full support for both 576i (PAL) and 486i (NTSC) video systems
- **Multiple output modes**:
  - Composite (.tbc)
  - Separate Y/C (.tbcy + .tbcc)
  - Legacy Y/C naming mode
- **LaserDisc metadata generation** - IEC 60857/60856 standards with CAV/CLV timecodes, chapter numbers, and picture numbers
- **VBI and VITS line generation** - Vertical blanking interval data for authentic LaserDisc simulation
- **Configurable filtering** - Separate luma and chroma FIR filter controls
- **Video level customization** - Override blanking, black, and white levels for specific projects
- **Flexible section system** - Combine different sources with varying durations in a single output

## Building

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or later
- SQLite3 development libraries

### Dependencies
- SQLite3 - For metadata database generation
- yaml-cpp - For YAML project file parsing
- libpng - For PNG image loading
- spdlog - For structured logging
- FFmpeg libraries (libavformat, libavcodec, libavutil, libswscale) - For MOV/MP4 file support

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/encode-orc.git
cd encode-orc

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make

# Run tests
./run-tests.sh
```

## Usage

encode-orc uses YAML project files to define encoding configurations. The application takes a single YAML file as input and generates TBC output files with metadata.

### Basic Example
```bash
# Encode a project
./encode-orc project.yaml

# Enable debug logging
./encode-orc project.yaml --log-level debug

# Save logs to file
./encode-orc project.yaml --log-level debug --log-file output.log

# Show version
./encode-orc --version

# Show help
./encode-orc --help
```

### Example Project File

```yaml
name: "PAL Test Project"
description: "PAL composite video with color bars"

output:
  filename: "output.tbc"
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
    laserdisc:
      picture_start: 1
      chapter: 1
```

See [docs/yaml-project-format.md](docs/yaml-project-format.md) for complete YAML configuration documentation.

## File Format Documentation

The `.tbc.db` metadata format follows the ld-decode specification:
https://happycube.github.io/ld-decode-docs/Development/tools-metadata-format.html

## Implementation

encode-orc's encoding is based on the ld-chroma-encoder component from the ld-decode project:
https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder

## Project Status

Initial development

## Contributing

Contributions are welcome! This project is in early development, so expect significant changes to the codebase and API.

## License

See [LICENSE](LICENSE) file for details.

## Documentation

- [YAML Project Format](docs/yaml-project-format.md) - Complete YAML configuration reference
- [MOV File Support](docs/MOV-File-Support.md) - QuickTime MOV file format details
- [MP4 File Support](docs/MP4-File-Support.md) - MP4 file format details
- [Test Signals](docs/Test-Signals.md) - Available test signal documentation
- [VBI Documentation](docs/VBI.md) - Vertical blanking interval implementation
- [PAL Frame and Field Numbering](docs/PAL-frame-and-field-numbering.md) - PAL-specific details

## Related Projects

- [decode-orc](https://github.com/simoninns/decode-orc) - The primary consumer of encode-orc test data

## Acknowledgments

This project builds upon the work of the ld-decode community and the video preservation community at large.
