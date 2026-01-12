# encode-orc

Encoder for decode-orc (for making test TBC/Metadata files)

## Overview

encode-orc is a C++17 command-line application that generates test data for [decode-orc](https://github.com/happycube/ld-decode). It produces field-based video files in the TBC (Time Base Corrected) format along with accompanying metadata, simulating the output of RF capture devices used in the LaserDisc and analog video preservation community.

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

### NTSC (486i, 29.97 fps)
- **Composite**: `video.tbc` + `video.tbc.db`
- **Y/C (S-Video)**: `video.tbcy` + `video.tbcc` + `video.tbc.db`

Where:
- `.tbc` = Composite field-based video file
- `.tbcy` = Luma (Y) field-based video file
- `.tbcc` = Chroma (C) field-based video file
- `.tbc.db` = SQLite metadata database (ld-decode format)

## Features

### Current
- *Project is in early development*

### Planned
- RGB frame input processing
- Built-in test card generation (SMPTE, PM5544, BBC Test Card F, etc.)
- Proper PAL/NTSC signal encoding based on ld-chroma-encoder
- VITS and VBI line generation
- Timecode and frame number embedding
- OSD overlay system for frame identification
- Signal degradation simulation (dropouts, noise)
- Configurable metadata generation

## Building

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or later
- SQLite3 development libraries

### Dependencies
- SQLite3
- A CLI parsing library (CLI11, cxxopts, or Boost.Program_options)
- (Optional) Google Test for running unit tests

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

# Run tests (optional)
make test
```

## Usage

*Note: encode-orc is currently under development. Usage examples will be added as features are implemented.*

### Basic Example (Planned)
```bash
# Generate PAL composite video from RGB frames
encode-orc --format pal-composite --input frames.rgb --output video.tbc

# Generate NTSC Y/C video with built-in test card
encode-orc --format ntsc-yc --testcard smpte --frames 300 --output test.tbc

# Add frame numbers and simulate dropouts
encode-orc --format pal-composite --input frames.rgb --output video.tbc \
  --overlay frame-number --add-dropouts 5
```

## File Format Documentation

The `.tbc.db` metadata format follows the ld-decode specification:
https://happycube.github.io/ld-decode-docs/Development/tools-metadata-format.html

## Implementation

encode-orc's encoding is based on the ld-chroma-encoder component from the ld-decode project:
https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder

For detailed information about the project vision and implementation plan, see:
- [Vision Statement](docs/vision.md)
- [Implementation Plan](docs/implementation-plan.md)

## Project Status

**Current Milestone**: M0 - Initial development

See the [Implementation Plan](docs/implementation-plan.md) for detailed roadmap and milestones.

## Contributing

Contributions are welcome! This project is in early development, so expect significant changes to the codebase and API.

## License

See [LICENSE](LICENSE) file for details.

## Related Projects

- [ld-decode](https://github.com/happycube/ld-decode) - Software for decoding RF captures of analog video
- [decode-orc](https://github.com/happycube/ld-decode) - The decoder this tool generates test data for

## Acknowledgments

This project builds upon the work of the ld-decode community and the video preservation community at large.
