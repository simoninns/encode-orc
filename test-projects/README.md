# encode-orc Test Projects

This folder contains a minimal comprehensive set of YAML project files for testing encode-orc functionality. All outputs are written to `../test-output/`.

## Test Projects Overview

The test suite consists of 9 projects covering all major features:

### PAL Tests

1. **pal-composite.yaml** - PAL composite encoding
   - Tests: Lead-in/lead-out, LaserDisc CAV mode, chapters, VITC timecode
   - Source: YUV422 EBU color bars (75% and 100%)
   - Output: Standard composite TBC

2. **pal-separate-yc.yaml** - PAL separate Y/C encoding
   - Tests: Separate Y/C output mode (.tbcy and .tbcc files)
   - Source: YUV422 EBU color bars (75%)
   - Output: Separate luma and chroma TBC files
   - Features: Chroma and luma filtering enabled

3. **pal-png.yaml** - PAL PNG image loader
   - Tests: PNG image loading and conversion
   - Source: Multiple PNG test cards
   - Output: Standard composite TBC

4. **pal-mov.yaml** - PAL MOV/ProRes video loader
   - Tests: MOV video file loading, frame extraction, ProRes decoding
   - Source: MOV video file
   - Output: Standard composite TBC
   - Features: Chroma filtering enabled

5. **pal-consumer-tape.yaml** - PAL consumer tape mode (VHS/Betamax/Video8)
   - Tests: Consumer-tape standard with VITC timecode
   - Source: YUV422 EBU color bars (75%)
   - Output: PAL composite TBC with VITC (no LaserDisc VBI/VITS)
   - Features: VITC timecode for consumer video tape formats

### NTSC Tests

6. **ntsc-composite.yaml** - NTSC composite encoding
   - Tests: Lead-in/lead-out, LaserDisc CAV mode, chapters, VITC timecode
   - Source: YUV422 EIA color bars (75% and 100%)
   - Output: Standard composite TBC

7. **ntsc-separate-yc.yaml** - NTSC separate Y/C encoding
   - Tests: Separate Y/C output mode (.tbcy and .tbcc files)
   - Source: YUV422 EIA color bars (75%)
   - Output: Separate luma and chroma TBC files
   - Features: Chroma and luma filtering enabled

8. **ntsc-mp4.yaml** - NTSC MP4/H.264 video loader
   - Tests: MP4 video file loading, frame extraction, H.264 decoding
   - Source: MP4 video file
   - Output: Standard composite TBC
   - Features: Chroma filtering enabled

9. **ntsc-consumer-tape.yaml** - NTSC consumer tape mode (VHS/Betamax/Video8)
   - Tests: Consumer-tape standard with VITC timecode
   - Source: YUV422 EIA color bars (75%)
   - Output: NTSC composite TBC with VITC (no LaserDisc VBI/VITS)
   - Features: VITC timecode for consumer video tape formats

## Coverage

These 9 tests provide comprehensive coverage of:

- **Video Standards**: PAL and NTSC
- **Output Formats**: Composite TBC, Separate Y/C (.tbcy/.tbcc)
- **Source Types**: YUV422 raw images, PNG images, MP4 video files, MOV video files
- **LaserDisc Features**: Lead-in, lead-out, CAV picture numbering, chapters
- **Consumer Tape**: Consumer-tape mode for both PAL and NTSC (VHS/Betamax/Video8) with VITC
- **VBI Data**: VITC timecode encoding (both LaserDisc and consumer-tape modes)
- **Filters**: Chroma and luma filtering
- **Test Patterns**: EBU color bars (75%, 100%), EIA color bars (75%, 100%), various PNG test cards

## Running Tests

To run all tests:
```bash
./run-tests.sh
```

To run a specific test:
```bash
./build/encode-orc test-projects/pal-composite.yaml
```

## Local Test Projects

Additional test projects have been moved to the `local-projects/` directory (gitignored). These can be used for development and experimentation without cluttering the main test suite.
