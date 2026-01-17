#!/usr/bin/env python3
"""
Generate PAL EBU color bars in Y'CbCr 4:2:2 format
ITU-R BT.601-derived component video, 10-bit quantization,
packed as YUYV, studio range, little-endian, top field first.

Active picture: 720 × 576 @ 50i
"""

import struct
import os

# ITU-R BT.601 studio range (10-bit)
# Y':  64-940 (reference black to white)
# Cb/Cr: 64-960 (reference range centered at 512)
Y_BLACK = 64
Y_WHITE = 940
C_MIN = 64
C_MAX = 960
C_ZERO = 512  # Zero chroma (neutral)

def rgb_to_ycbcr_bt601(r10, g10, b10):
    """
    Convert RGB (10-bit, 64-940 range) to Y'CbCr (10-bit, studio range)
    Using ITU-R BT.601 coefficients
    
    Args:
        r10, g10, b10: RGB values in 10-bit 64-940 range
    
    Returns:
        (y, cb, cr): Y'CbCr values in 10-bit studio range
    """
    # First normalize RGB to 0.0-1.0
    r_norm = (r10 - 64) / (940 - 64)
    g_norm = (g10 - 64) / (940 - 64)
    b_norm = (b10 - 64) / (940 - 64)
    
    # BT.601 matrix coefficients
    # Y' = 0.299*R + 0.587*G + 0.114*B
    # Cb = -0.168736*R - 0.331264*G + 0.5*B
    # Cr = 0.5*R - 0.418688*G - 0.081312*B
    
    y_norm = 0.299 * r_norm + 0.587 * g_norm + 0.114 * b_norm
    cb_norm = -0.168736 * r_norm - 0.331264 * g_norm + 0.5 * b_norm
    cr_norm = 0.5 * r_norm - 0.418688 * g_norm - 0.081312 * b_norm
    
    # Scale to 10-bit studio range
    y = int(64 + y_norm * (940 - 64) + 0.5)
    cb = int(512 + cb_norm * (960 - 64) + 0.5)
    cr = int(512 + cr_norm * (960 - 64) + 0.5)
    
    # Clamp to valid ranges
    y = max(64, min(940, y))
    cb = max(64, min(960, cb))
    cr = max(64, min(960, cr))
    
    return y, cb, cr

def generate_colorbar_frame_yuv422(width, height, saturation_percent):
    """
    Generate EBU color bars in Y'CbCr 4:2:2 format (YUYV)
    
    Args:
        width: Frame width (must be even, typically 720 for PAL)
        height: Frame height (576 for PAL)
        saturation_percent: 75 or 100
    
    Returns:
        bytes: Raw YUYV data (10-bit packed in 16-bit little-endian)
    """
    if width % 2 != 0:
        raise ValueError("Width must be even for 4:2:2 sampling")
    
    # Calculate bar widths (8 bars total)
    bar_width = width // 8
    
    # Define 100% EBU color bars (R, G, B) in 10-bit 64-940 range
    # Order: White, Yellow, Cyan, Green, Magenta, Red, Blue, Black
    bars_rgb_100 = [
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
        bars_rgb = []
        for r, g, b in bars_rgb_100:
            r75 = 64 + int(0.75 * (r - 64))
            g75 = 64 + int(0.75 * (g - 64))
            b75 = 64 + int(0.75 * (b - 64))
            bars_rgb.append((r75, g75, b75))
    else:
        bars_rgb = bars_rgb_100
    
    # Convert RGB bars to Y'CbCr
    bars_yuv = [rgb_to_ycbcr_bt601(r, g, b) for r, g, b in bars_rgb]
    
    # Generate frame data in YUYV format
    # YUYV packing: Y0 U0 Y1 V0 (for pixels 0 and 1)
    # Each component is 10-bit, packed in 16-bit little-endian
    frame_data = bytearray()
    
    for y in range(height):
        for x in range(0, width, 2):  # Process pairs of pixels
            # Get bar indices for this pixel pair
            bar_index0 = min(x // bar_width, 7)
            bar_index1 = min((x + 1) // bar_width, 7)
            
            y0, cb0, cr0 = bars_yuv[bar_index0]
            y1, cb1, cr1 = bars_yuv[bar_index1]
            
            # Average chroma for the pair (4:2:2 subsampling)
            cb_avg = (cb0 + cb1 + 1) // 2
            cr_avg = (cr0 + cr1 + 1) // 2
            
            # Pack as YUYV: Y0 U Y1 V (16-bit little-endian per component)
            frame_data.extend(struct.pack('<HHHH', y0, cb_avg, y1, cr_avg))
    
    return bytes(frame_data)

def main():
    width = 720
    height = 576
    
    output_dir = os.path.join(os.path.dirname(__file__), '..', 'testcard-images', 'pal-raw')
    os.makedirs(output_dir, exist_ok=True)
    
    # Generate 100% bars
    print(f"Generating PAL EBU 100% color bars ({width}x{height}, Y'CbCr 4:2:2 YUYV)...")
    data_100 = generate_colorbar_frame_yuv422(width, height, 100)
    output_file_100 = os.path.join(output_dir, 'pal-ebu-colorbars-100.raw')
    with open(output_file_100, 'wb') as f:
        f.write(data_100)
    print(f"  Written: {output_file_100} ({len(data_100)} bytes)")
    
    # Generate 75% bars
    print(f"Generating PAL EBU 75% color bars ({width}x{height}, Y'CbCr 4:2:2 YUYV)...")
    data_75 = generate_colorbar_frame_yuv422(width, height, 75)
    output_file_75 = os.path.join(output_dir, 'pal-ebu-colorbars-75.raw')
    with open(output_file_75, 'wb') as f:
        f.write(data_75)
    print(f"  Written: {output_file_75} ({len(data_75)} bytes)")
    
    # YUYV format: 4 components (Y U Y V) per 2 pixels = 8 bytes per 2 pixels
    expected_size = (width // 2) * height * 4 * 2  # (width/2 pairs) * height * 4 components * 2 bytes
    print(f"\nExpected file size: {expected_size} bytes")
    print(f"100% file size: {len(data_100)} bytes")
    print(f"75% file size: {len(data_75)} bytes")
    print("\nFormat: Y'CbCr 4:2:2, 10-bit, YUYV packed, studio range, little-endian")
    print("ITU-R BT.601 studio range:")
    print("  Y':     64-940")
    print("  Cb/Cr:  64-960 (centered at 512)")
    print("\nDone!")

if __name__ == '__main__':
    main()
