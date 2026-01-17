#!/usr/bin/env python3
"""
Generate NTSC EIA/SMPTE color bars in RGB30 format (10-bit per channel)
Stored in RGB48 container (16-bit little-endian per channel)
"""

import struct
import os

# 10-bit 64-940 standard (ITU-R BT.601)
BLACK_10BIT = 64
WHITE_10BIT = 940
RANGE_10BIT = WHITE_10BIT - BLACK_10BIT

def generate_colorbar_frame(width, height, saturation_percent):
    """
    Generate EIA/SMPTE color bars
    
    Args:
        width: Frame width (720 for NTSC)
        height: Frame height (480 for NTSC)
        saturation_percent: 75 or 100
    
    Returns:
        bytes: Raw RGB48 data
    """
    # Calculate bar widths (8 bars total)
    bar_width = width // 8
    
    # Define 100% EIA color bars (R, G, B) in 10-bit 64-940 range
    # Order: White, Yellow, Cyan, Green, Magenta, Red, Blue, Black
    bars_100 = [
        (940, 940, 940),  # White
        (940, 940, 64),   # Yellow
        (64, 940, 940),   # Cyan
        (64, 940, 64),    # Green
        (940, 64, 940),   # Magenta
        (940, 64, 64),    # Red
        (64, 64, 940),    # Blue
        (64, 64, 64),     # Black
    ]
    
    # For 75% saturation, scale the color components
    if saturation_percent == 75:
        # 75% bars: scale from black level
        # value = BLACK + 0.75 * (color_100 - BLACK)
        bars = []
        for r, g, b in bars_100:
            r75 = BLACK_10BIT + int(0.75 * (r - BLACK_10BIT))
            g75 = BLACK_10BIT + int(0.75 * (g - BLACK_10BIT))
            b75 = BLACK_10BIT + int(0.75 * (b - BLACK_10BIT))
            bars.append((r75, g75, b75))
    else:
        bars = bars_100
    
    # Generate frame data
    frame_data = bytearray()
    
    for y in range(height):
        for x in range(width):
            # Determine which bar this pixel belongs to
            bar_index = min(x // bar_width, 7)  # Clamp to 0-7
            r, g, b = bars[bar_index]
            
            # Pack as 16-bit little-endian (RGB48 container for RGB30 data)
            # Upper 6 bits are unused, 10-bit value in lower bits
            frame_data.extend(struct.pack('<HHH', r, g, b))
    
    return bytes(frame_data)

def main():
    width = 720
    height = 480  # Corrected from 486 to 480
    
    output_dir = os.path.join(os.path.dirname(__file__), '..', 'testcard-images', 'ntsc-raw')
    os.makedirs(output_dir, exist_ok=True)
    
    # Generate 100% bars
    print(f"Generating NTSC EIA 100% color bars ({width}x{height})...")
    data_100 = generate_colorbar_frame(width, height, 100)
    output_file_100 = os.path.join(output_dir, 'ntsc-eia-colorbars-100.raw')
    with open(output_file_100, 'wb') as f:
        f.write(data_100)
    print(f"  Written: {output_file_100} ({len(data_100)} bytes)")
    
    # Generate 75% bars
    print(f"Generating NTSC EIA 75% color bars ({width}x{height})...")
    data_75 = generate_colorbar_frame(width, height, 75)
    output_file_75 = os.path.join(output_dir, 'ntsc-eia-colorbars-75.raw')
    with open(output_file_75, 'wb') as f:
        f.write(data_75)
    print(f"  Written: {output_file_75} ({len(data_75)} bytes)")
    
    expected_size = width * height * 3 * 2
    print(f"\nExpected file size: {expected_size} bytes")
    print(f"100% file size: {len(data_100)} bytes")
    print(f"75% file size: {len(data_75)} bytes")
    print("\nDone!")

if __name__ == '__main__':
    main()
