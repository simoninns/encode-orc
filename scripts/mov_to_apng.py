#!/usr/bin/env python3
"""
Generate animated PNG (APNG) preview images from MOV test-card files for documentation.

Each MOV is sampled to produce 6–12 frames at 240px wide, which are assembled
into a looping APNG suitable for embedding in MkDocs pages.

Zone-plate clips (highly complex per-frame content) use sparse frame selection
to keep file sizes reasonable.  Semi-static clips (PLUGE) use continuous fps
down-sampling — the resulting file is tiny because the frames are nearly identical.

Usage:
    Run from the repository root:
        nix develop
        python3 scripts/mov_to_apng.py

    Or convert a single file:
        python3 scripts/mov_to_apng.py <input.mov> <output.apng> [--width W] [--fps F] [--frames N]

Requirements:
    ffmpeg must be available on PATH (provided by `nix develop`).
"""

import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).parent.parent
DOCS_IMAGES = REPO_ROOT / "docs" / "technical" / "testcards" / "images"

# Each entry: (source_mov, output_dir_tag, output_stem, display_width, playback_fps, sparse_step)
# sparse_step = None  → use fps filter (all frames resampled to playback_fps)
# sparse_step = N     → select every Nth source frame, display at playback_fps
CONVERSIONS = [
    # --- PAL 720×576, 25 fps ---
    (
        REPO_ROOT / "assets/720x576/video/mov_25_00/Moving-Zone-2H.mov",
        "pal",
        "Moving-Zone-2H",
        180, 4, 6,          # zone plate: 180 px, 43 source frames → ~7 sparse samples
    ),
    (
        REPO_ROOT / "assets/720x576/video/mov_25_00/PLUGE.mov",
        "pal",
        "PLUGE",
        240, 6, None,       # mostly static; full fps resample, tiny output
    ),
    (
        REPO_ROOT / "assets/720x576/video/mov_25_00/pt5300.mov",
        "pal",
        "pt5300",
        240, 4, None,       # 75 frames → ~12 output frames at 4 fps
    ),
    # --- NTSC 704×480, 29.97 fps ---
    (
        REPO_ROOT / "assets/704x480/video/mov_29_97/MOVING_ZONE_2H.mov",
        "ntsc",
        "MOVING_ZONE_2H",
        180, 4, 8,          # zone plate: 180 px, 50 source frames → ~6 sparse samples
    ),
]


def convert(mov_path: Path, out_path: Path, width: int, fps: int, sparse_step):
    """Convert a MOV to a looping APNG at the given display width."""
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if sparse_step is not None:
        # Select every sparse_step-th source frame, then reset PTS so they
        # play at playback_fps.
        vf = (
            f"select='not(mod(n,{sparse_step}))',"
            f"scale={width}:-2,"
            f"setpts=N/({fps}*TB)"
        )
        cmd = [
            "ffmpeg", "-y",
            "-i", str(mov_path),
            "-vf", vf,
            "-an",
            "-plays", "0",   # loop for ever in browsers that respect it
            str(out_path),
        ]
    else:
        vf = f"fps={fps},scale={width}:-2"
        cmd = [
            "ffmpeg", "-y",
            "-i", str(mov_path),
            "-vf", vf,
            "-an",
            "-plays", "0",
            str(out_path),
        ]

    print(f"  {mov_path.name} → {out_path.relative_to(REPO_ROOT)}", end="", flush=True)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"\n  ERROR:\n{result.stderr[-800:]}")
        return False

    size_kb = out_path.stat().st_size // 1024
    print(f"  ({size_kb} KB)")
    return True


def generate_all():
    for mov_path, tag, stem, width, fps, sparse_step in CONVERSIONS:
        out_path = DOCS_IMAGES / tag / f"{stem}.apng"
        convert(mov_path, out_path, width, fps, sparse_step)


def main():
    args = sys.argv[1:]

    if not args:
        generate_all()
        return

    if len(args) < 2:
        print(__doc__)
        sys.exit(1)

    mov_path = Path(args[0])
    out_path = Path(args[1])

    # Parse optional keyword-style flags
    width  = 240
    fps    = 6
    sparse = None
    i = 2
    while i < len(args):
        if args[i] == "--width" and i + 1 < len(args):
            width = int(args[i + 1]); i += 2
        elif args[i] == "--fps" and i + 1 < len(args):
            fps = int(args[i + 1]); i += 2
        elif args[i] == "--frames" and i + 1 < len(args):
            sparse = int(args[i + 1]); i += 2
        else:
            i += 1

    convert(mov_path, out_path, width, fps, sparse)


if __name__ == "__main__":
    main()
