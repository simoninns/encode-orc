# encode-orc Test Projects

This folder contains YAML project files for comprehensive testing of all video format and LaserDisc mode combinations. All outputs are written to `../test-output/`.

## Comprehensive Test Projects

These four projects cover all combinations of video format (NTSC/PAL) and LaserDisc disc format (CAV/CLV):

### NTSC CAV
- `ntsc-cav-eia.yaml` — NTSC composite, EIA test card, 50 frames total (10 per section)
  - Lead-in (10 frames) → Chapter 1 (10 frames, picture 1-10) → Chapter 2 (10 frames, picture 11-20) → Chapter 3 (10 frames, picture 21-30) → Lead-out (10 frames)

### NTSC CLV
- `ntsc-clv-eia.yaml` — NTSC composite, EIA test card, 50 frames total (10 per section)
  - Lead-in (10 frames) → Chapter 1 (10 frames, 00:00:00-00:00:10) → Chapter 2 (10 frames, 00:00:10-00:00:20) → Chapter 3 (10 frames, 00:00:20-00:00:30) → Lead-out (10 frames)

### PAL CAV
- `pal-cav-ebu.yaml` — PAL composite, EBU test card, 50 frames total (10 per section)
  - Lead-in (10 frames) → Chapter 1 (10 frames, picture 1-10) → Chapter 2 (10 frames, picture 11-20) → Chapter 3 (10 frames, picture 21-30) → Lead-out (10 frames)

### PAL CLV
- `pal-clv-ebu.yaml` — PAL composite, EBU test card, 50 frames total (10 per section)
  - Lead-in (10 frames) → Chapter 1 (10 frames, 00:00:00-00:00:10) → Chapter 2 (10 frames, 00:00:10-00:00:20) → Chapter 3 (10 frames, 00:00:20-00:00:30) → Lead-out (10 frames)

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
2. Encode all four test projects
3. Verify output files were created
4. Display test summary

Outputs land in `../test-output/` with corresponding `.tbc` and `.db` (metadata) files.
