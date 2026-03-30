# Testcards

Testcard assets are used by encode-orc when encoding TBC video and a variety of input formats are supported.

The testcard assets live under `assets/` in the repository:

- `assets/pal/raw/` — PAL raw Y'CbCr 4:2:2 test frames
- `assets/pal/mov/` — PAL QuickTime MOV sources
- `assets/pal/mp4/` — PAL MP4 sources
- `assets/pal/wrwetzel-png/` — PAL PNG testcards by Bill Wetzel
- `assets/ntsc/raw/` — NTSC raw Y'CbCr 4:2:2 test frames
- `assets/ntsc/mov/` — NTSC QuickTime MOV sources
- `assets/ntsc/mp4/` — NTSC MP4 sources
- `assets/ntsc/wrwetzel-png/` — NTSC PNG testcards by Bill Wetzel
- `sound/` — Audio assets used by sound-related tests

There is no separate generic `png/` directory in the current tree. PNG-based examples should reference the Bill Wetzel folders under `assets/pal/wrwetzel-png/` or `assets/ntsc/wrwetzel-png/`.

PAL-M projects use the PAL-M output formats, but typically source their still images and video from the NTSC-sized asset folders under `assets/ntsc/` because PAL-M uses 720×480 active video.
