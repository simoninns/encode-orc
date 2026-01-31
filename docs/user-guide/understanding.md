---
title: Understanding encode-orc
layout: default
parent: User Guide
nav_order: 1
---

# Understanding encode-orc

encode-orc generates synthetic video test data in TBC (Time Base Corrected) format. It's designed for:

- Testing decode-orc implementations
- Creating reproducible test cases
- Validating video decoding pipelines
- Educational and research purposes

## Basic Workflow

1. **Create a YAML project file** - Define your video project
2. **Prepare input files** - Gather source images or videos
3. **Run encode-orc** - Generate TBC files and metadata
4. **Use output files** - Feed into your decode pipeline
