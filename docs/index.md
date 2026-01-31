---
title: Home
layout: default
nav_order: 1
---

# encode-orc Documentation

Welcome to the **encode-orc** project documentation. This site provides guides and reference material for using encode-orc to generate test data for the decode-orc video decoding pipeline.

## Quick Links

- **[Getting Started](getting-started/)** - Installation and first steps
- **[User Guide](user-guide/)** - Detailed documentation and examples
- **[Technical Reference](technical/)** - File formats and technical details
- **[Testcards](testcards/)** - Testcard assets and usage
- **[Output Formats](technical/output-formats/)** - Supported TBC and metadata formats

## What is encode-orc?

encode-orc is a C++17 command-line application that generates test data for [decode-orc](https://github.com/simoninns/decode-orc). It produces field-based video files in the TBC (Time Base Corrected) format along with accompanying metadata, simulating the output of RF decoding applications.

## Key Features

- **YAML-based configuration** - Define video projects declaratively
- **Multiple input formats** - YUV422, PNG, MOV, MP4
- **PAL and NTSC support** - Both 576i and 480i video systems
- **Flexible output modes** - Composite and Y/C (S-Video) encoding
- **Metadata generation** - SQLite database and embedded metadata

## Getting Help

- Check the [User Guide](user-guide/) for common tasks
- Review the [User Guide](user-guide/) for YAML syntax
- See example projects in the repository's `example-projects/` directory

---

**Repository:** [github.com/simoninns/encode-orc](https://github.com/simoninns/encode-orc)
