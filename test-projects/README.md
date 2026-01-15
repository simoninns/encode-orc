# encode-orc Test Projects

This folder contains YAML project files for comprehensive testing of RGB30 raw image encoding. All outputs are written to `../test-output/`.

## RGB30 Raw Image Projects

These four projects use raw RGB30 images (10-bit per channel in 64-940 standard) from the testcard-images folder. Each image is repeated for all frames in the section:

### PAL with EBU Bars 75%
- `pal-rgb30-ebu-75.yaml` — PAL composite with 75% saturation EBU color bars, 50 frames
  - Source: `testcard-images/pal-ebu-colorbars-75.raw` (720×576, RGB30 64-940)

### PAL with EBU Bars 100%
- `pal-rgb30-ebu-100.yaml` — PAL composite with 100% saturation EBU color bars, 50 frames
  - Source: `testcard-images/pal-ebu-colorbars-100.raw` (720×576, RGB30 64-940)

### NTSC with EIA Bars 75%
- `ntsc-rgb30-eia-75.yaml` — NTSC composite with 75% saturation EIA color bars, 50 frames
  - Source: `testcard-images/ntsc-eia-colorbars-75.raw` (720×486, RGB30 64-940)

### NTSC with EIA Bars 100%
- `ntsc-rgb30-eia-100.yaml` — NTSC composite with 100% saturation EIA color bars, 50 frames
  - Source: `testcard-images/ntsc-eia-colorbars-100.raw` (720×486, RGB30 64-940)

## YAML Syntax for RGB30 Images

To use raw RGB30 images in a section:

```yaml
sections:
  - name: "Section Name"
    duration: 10          # Number of frames to encode
    source:
      type: "rgb30-image"
      file: "testcard-images/path-to-image.raw"
    laserdisc:
      picture_start: 1    # Optional: CAV picture numbering
      chapter: 1          # Optional: CLV chapter numbering
```

The encoder will:
1. Load the RGB30 raw file (verifying correct dimensions for the video format)
2. Convert each pixel from 10-bit RGB (64-940 standard) to 16-bit RGB
3. Convert to YUV for the target video system
4. Encode the image into every frame of the section

## Features

Each project tests:
- **Lead-in/Lead-out areas** with appropriate VBI encoding
- **Picture numbers (CAV)** or **Timecode (CLV)** with chapter markers
- **Multi-section encoding** with proper VBI continuity
- **IEC 60857-1986 (PAL)** or **IEC 60856-1986 (NTSC)** compliance
- **Proper programme status codes** for each disc area

## Running

Use `./run-tests.sh` from the repo root to run all projects. The script will:
1. Build the project if needed
2. Encode all test projects (both built-in patterns and RGB30 images)
3. Verify output files were created
4. Display test summary

Outputs land in `../test-output/` with corresponding `.tbc` and `.db` (metadata) files.
