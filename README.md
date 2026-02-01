# encode-orc

![](docs/assets/encode-orc_logotype.png)

Encode-Orc is a PAL and NTSC encoder capable of producing ld-decode and vhs-decode compatible TBC files (and SQLite metadata) to assist with testing.

Please see the [Encode-Orc Documentation](https://simoninns.github.io/encode-orc) for details.

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

## Project Status

First release

## Build Status

[![Build and Test](https://github.com/simoninns/encode-orc/actions/workflows/ci.yml/badge.svg)](https://github.com/simoninns/encode-orc/actions/workflows/ci.yml)
[![Release](https://github.com/simoninns/encode-orc/actions/workflows/release.yml/badge.svg)](https://github.com/simoninns/encode-orc/actions/workflows/release.yml)

Continuous integration builds and tests for Windows, macOS, and Linux (Fedora/Ubuntu/Flatpak) are run automatically on every commit. Release packages (MSI, DMG, and Flatpak) are automatically generated when version tags are pushed.

See [BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md) for detailed build and packaging documentation.

## Contributing

Contributions are welcome! Please discuss in the Domesday86 discord, raise issues or submit PRs.

## License

See [LICENSE](LICENSE) file for details.

## Related Projects

- [decode-orc](https://github.com/simoninns/decode-orc) - The primary consumer of encode-orc test data

## Acknowledgments

This project builds upon the work of the ld-decode community and the video preservation community at large.

encode-orc's encoding is based on the ld-chroma-encoder component from the ld-decode project:
https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder