# Testcards

Testcard assets are used by encode-orc when encoding TBC video and a variety of input formats are supported.

The testcard assets live under `assets/` in the repository:

- `assets/720x576/stills/raw/` — PAL raw Y'CbCr 4:2:2 test frames
- `assets/720x576/video/mov_25_00/` — PAL QuickTime MOV sources
- `assets/720x576/video/mp4_25_00/` — PAL MP4 sources
- `assets/720x576/stills/png/` — PAL PNG testcards by Bill Wetzel
- `assets/720x480/stills/raw/` — NTSC raw Y'CbCr 4:2:2 test frames
- `assets/704x480/video/mov_29_97/` — NTSC QuickTime MOV sources
- `assets/720x480/video/mp4_29_97/` — NTSC MP4 sources
- `assets/720x480/stills/png/` — NTSC PNG testcards by Bill Wetzel
- `sound/` — Audio assets used by sound-related tests

There is no separate generic `png/` directory in the current tree. PNG-based examples should reference the Bill Wetzel folders under `assets/720x576/stills/png/` or `assets/720x480/stills/png/`.

PAL-M projects use the PAL-M output formats, but typically source their still images and video from the NTSC-sized asset folders under `assets/720x480/` because PAL-M uses 720×480 active video.
