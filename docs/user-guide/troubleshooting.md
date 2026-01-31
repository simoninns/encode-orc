---
title: Troubleshooting
layout: default
parent: User Guide
nav_order: 8
---

# Troubleshooting

## Output Files Not Generated
- Check that the output directory exists and is writable
- Verify the YAML configuration syntax
- Run with `--verbose` flag for debugging information

## Build Fails on macOS
- Install YAML library: `brew install libyaml`
- Update Xcode command line tools: `xcode-select --install`

## Performance Issues
- Reduce frame count for testing
- Use lower resolution input files
- Process on a system with more RAM for large videos

---

