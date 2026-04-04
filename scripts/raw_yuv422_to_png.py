#!/usr/bin/env python3
"""
Convert raw Y'CbCr 4:2:2 test card files to PNG preview images for documentation.

Input format:
  YUYV packed, each component (Y0, Cb, Y1, Cr) stored as a 10-bit value in
  the lower 10 bits of a 16-bit little-endian word.  Studio range: Y 64-940,
  Cb/Cr 64-960 with chroma neutral at 512.

Usage:
  python3 raw_yuv422_to_png.py <input.raw> <width> <height> <output.png>

Or run with no arguments to generate all documentation preview images.
"""

import struct
import subprocess
import sys
import zlib
from pathlib import Path


# ---------------------------------------------------------------------------
# PNG writer (stdlib only)
# ---------------------------------------------------------------------------

def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    import zlib as _zlib
    crc = _zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    """Write 8-bit RGB pixel data as a PNG file."""
    # Filter type 0 (None) prepended to each scanline
    row_bytes = width * 3
    filtered = bytearray(height * (1 + row_bytes))
    for y in range(height):
        filtered[y * (1 + row_bytes)] = 0  # filter type
        filtered[y * (1 + row_bytes) + 1 : (y + 1) * (1 + row_bytes)] = \
            rgb[y * row_bytes : (y + 1) * row_bytes]

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = _png_chunk(b"IHDR",
                      struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    idat = _png_chunk(b"IDAT", zlib.compress(bytes(filtered), 6))
    iend = _png_chunk(b"IEND", b"")

    path.write_bytes(sig + ihdr + idat + iend)


# ---------------------------------------------------------------------------
# YCbCr → RGB conversion (BT.601 studio range)
# ---------------------------------------------------------------------------

# Pre-build LUTs for speed
_Y_LUT: list[float] = [(v - 64) / 876.0 for v in range(1024)]
_C_LUT: list[float] = [(v - 512) / 448.0 for v in range(1024)]

def _clamp8(x: float) -> int:
    v = int(x * 255.0 + 0.5)
    return max(0, min(255, v))


def raw_to_rgb8(raw_path: Path, width: int, height: int) -> bytes:
    """
    Read a YUYV 10-bit LE raw file and return 8-bit RGB bytes (row-major).

    Pixel pair layout: Y0 Cb Y1 Cr (each a uint16-LE, 10-bit value in bits 9:0).
    """
    data = raw_path.read_bytes()
    expected = width * height * 4  # 4 bytes per pixel (2 bytes × 2 components each pixel)
    if len(data) != expected:
        raise ValueError(
            f"{raw_path.name}: expected {expected} bytes "
            f"({width}×{height}×4), got {len(data)}"
        )

    fmt = f"<{width * height * 2}H"
    vals = struct.unpack(fmt, data)

    rgb = bytearray(width * height * 3)
    pair_count = width // 2

    for row in range(height):
        row_base = row * pair_count * 4  # index into vals
        rgb_base = row * width * 3

        for col_pair in range(pair_count):
            idx = row_base + col_pair * 4
            y0_raw = vals[idx]     & 0x3FF
            cb_raw = vals[idx + 1] & 0x3FF
            y1_raw = vals[idx + 2] & 0x3FF
            cr_raw = vals[idx + 3] & 0x3FF

            yn0 = _Y_LUT[y0_raw]
            yn1 = _Y_LUT[y1_raw]
            cbn = _C_LUT[cb_raw]
            crn = _C_LUT[cr_raw]

            # BT.601 matrix
            r0 = _clamp8(yn0 + 1.402 * crn)
            g0 = _clamp8(yn0 - 0.344136 * cbn - 0.714136 * crn)
            b0 = _clamp8(yn0 + 1.772 * cbn)

            r1 = _clamp8(yn1 + 1.402 * crn)
            g1 = _clamp8(yn1 - 0.344136 * cbn - 0.714136 * crn)
            b1 = _clamp8(yn1 + 1.772 * cbn)

            out = rgb_base + col_pair * 6
            rgb[out]     = r0
            rgb[out + 1] = g0
            rgb[out + 2] = b0
            rgb[out + 3] = r1
            rgb[out + 4] = g1
            rgb[out + 5] = b1

    return bytes(rgb)


def convert(raw_path: Path, width: int, height: int, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"  {raw_path.name} → {out_path}", end="", flush=True)
    rgb = raw_to_rgb8(raw_path, width, height)
    write_png(out_path, width, height, rgb)
    print(f"  ({out_path.stat().st_size // 1024} KB)")


# ---------------------------------------------------------------------------
# Bulk generation for documentation
# ---------------------------------------------------------------------------

DOCS_IMAGES_ROOT = Path(__file__).parent.parent / "docs" / "technical" / "testcards" / "images"

FORMATS = [
    ("pal",  Path(__file__).parent.parent / "assets" / "720x576" / "stills" / "raw", 720, 576),
    ("ntsc", Path(__file__).parent.parent / "assets" / "720x480" / "stills" / "raw", 720, 480),
]


def generate_all() -> None:
    for tag, raw_dir, w, h in FORMATS:
        out_dir = DOCS_IMAGES_ROOT / tag
        print(f"\n[{tag.upper()} {w}×{h}]  {raw_dir}")
        for raw_file in sorted(raw_dir.glob("*.raw")):
            out_png = out_dir / (raw_file.stem + ".png")
            try:
                convert(raw_file, w, h, out_png)
            except Exception as exc:
                print(f"  ERROR: {exc}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main() -> None:
    args = sys.argv[1:]

    if not args:
        generate_all()
        return

    if len(args) != 4:
        print(__doc__)
        sys.exit(1)

    raw_path = Path(args[0])
    width    = int(args[1])
    height   = int(args[2])
    out_path = Path(args[3])
    convert(raw_path, width, height, out_path)


if __name__ == "__main__":
    main()
