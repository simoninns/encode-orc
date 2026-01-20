#!/usr/bin/env python3
"""
Convert MOV files (10-bit v210 YCbCr) to raw YUV422 format

Takes a MOV file containing PAL testcards in v210 format and extracts
the first frame, converting it to raw YUV422 format (YUYV-packed, 10-bit
in 16-bit little-endian containers, ITU-R BT.601 studio range).

Usage:
    python3 convert_mov_to_yuv422.py <input.mov> [output.raw]

If output filename is not specified, it will be based on the input filename.
"""

import subprocess
import sys
import os
import struct
from pathlib import Path


def get_video_info(mov_file):
    """Get video dimensions using ffprobe"""
    try:
        cmd = [
            'ffprobe',
            '-v', 'error',
            '-select_streams', 'v:0',
            '-show_entries', 'stream=width,height',
            '-of', 'csv=p=0',
            mov_file
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        # ffprobe may add trailing comma, so filter empty strings
        parts = [p for p in result.stdout.strip().split(',') if p]
        width, height = map(int, parts[:2])
        return width, height
    except (subprocess.CalledProcessError, ValueError) as e:
        print(f"Error getting video info from {mov_file}: {e}", file=sys.stderr)
        return None, None


def convert_v210_to_yuyv10bit(v210_data, width, height):
    """
    Convert v210 (10-bit 4:2:2 packed) to YUYV 10-bit packed format
    
    v210 packing (4 bytes = 3 10-bit components):
    Cb0(10) Y0(10) Cr0(10) | Y1(10) Cb2(10) Y2(10) | Cr2(10) Y3(10) Cb4(10) | ...
    
    YUYV output: Y0 U0 Y1 V0 (as 16-bit little-endian words)
    
    Args:
        v210_data: Raw v210 data bytes
        width: Frame width (must be multiple of 2)
        height: Frame height
    
    Returns:
        YUYV 10-bit packed data
    """
    yuyv_packed = bytearray()
    
    # v210 line size: width pixels needs ceil(width * 10 / 8) bytes
    # For width divisible by 6: (width / 6) * 16 bytes per line
    v210_line_size = ((width + 47) // 48) * 128
    
    for row in range(height):
        v210_line_start = row * v210_line_size
        
        col = 0
        byte_offset = 0
        
        while col < width:
            # v210 packs 4 pixels worth of data into groups
            # Each group is 16 bytes containing 6 pixels worth of data:
            # 4 chroma samples (2 pairs for 4 pixels horizontally in 4:2:2)
            # 6 luma samples (one per pixel)
            
            line_offset = v210_line_start + byte_offset
            
            if line_offset + 16 <= len(v210_data):
                # Read 16 bytes
                group = v210_data[line_offset:line_offset + 16]
                
                # Extract 6 pixels (3 pairs) from this group
                # v210 format for 6 pixels (in bits, reading left-to-right):
                # Bytes 0-3:   Cb0(10) Y0(10) Cr0(10)  = 30 bits
                # Bytes 4-7:   Y1(10)  Cb2(10) Y2(10)  = 30 bits  
                # Bytes 8-11:  Cr2(10) Y3(10)  Cb4(10) = 30 bits
                # Bytes 12-15: Y4(10)  Cr4(10) Y5(10)  = 30 bits
                
                # Parse as uint32 little-endian
                dw0 = struct.unpack('<I', group[0:4])[0]
                dw1 = struct.unpack('<I', group[4:8])[0]
                dw2 = struct.unpack('<I', group[8:12])[0]
                dw3 = struct.unpack('<I', group[12:16])[0]
                
                # Extract 10-bit values from packed data
                cb0 = (dw0 >> 0) & 0x3FF
                y0  = (dw0 >> 10) & 0x3FF
                cr0 = (dw0 >> 20) & 0x3FF
                y1  = (dw1 >> 0) & 0x3FF
                cb2 = (dw1 >> 10) & 0x3FF
                y2  = (dw1 >> 20) & 0x3FF
                cr2 = (dw2 >> 0) & 0x3FF
                y3  = (dw2 >> 10) & 0x3FF
                cb4 = (dw2 >> 20) & 0x3FF
                y4  = (dw3 >> 0) & 0x3FF
                cr4 = (dw3 >> 10) & 0x3FF
                y5  = (dw3 >> 20) & 0x3FF
                
                # For 4:2:2, we average/use chroma appropriately
                # Pixel pair 0: Y0, (Cb0+Cb2)/2, Y1, (Cr0+Cr2)/2
                # But v210 already has properly positioned chroma, so:
                # Pair 0 (pixels 0-1): Y0, Cb0, Y1, Cr0
                # Pair 1 (pixels 2-3): Y2, Cb2, Y3, Cr2
                # Pair 2 (pixels 4-5): Y4, Cb4, Y5, Cr4
                
                if col < width:
                    yuyv_packed.extend(struct.pack('<HHHH', y0, cb0, y1, cr0))
                    col += 2
                
                if col < width:
                    yuyv_packed.extend(struct.pack('<HHHH', y2, cb2, y3, cr2))
                    col += 2
                
                if col < width:
                    yuyv_packed.extend(struct.pack('<HHHH', y4, cb4, y5, cr4))
                    col += 2
                
                byte_offset += 16
            else:
                break
    
    return yuyv_packed


def convert_yuv422p10le_to_yuyv10bit(yuv_data, width, height):
    """
    Convert YUV422P10LE (planar 10-bit little-endian) to YUYV 10-bit packed format
    
    YUV422P10LE layout:
    - Y plane: width * height samples (10-bit, little-endian in 16-bit words)
    - U plane: width/2 * height samples (10-bit, little-endian in 16-bit words)
    - V plane: width/2 * height samples (10-bit, little-endian in 16-bit words)
    
    YUYV output: Y0 U0 Y1 V0 (as 16-bit little-endian words)
    
    Args:
        yuv_data: Raw YUV422P10LE data bytes
        width: Frame width (must be multiple of 2)
        height: Frame height
    
    Returns:
        YUYV 10-bit packed data
    """
    # Calculate plane sizes (each 10-bit sample = 2 bytes)
    y_plane_size = width * height * 2
    u_plane_size = (width // 2) * height * 2
    v_plane_size = (width // 2) * height * 2
    
    total_expected = y_plane_size + u_plane_size + v_plane_size
    if len(yuv_data) < total_expected:
        print(f"Warning: YUV data size {len(yuv_data)} is less than expected {total_expected}", 
              file=sys.stderr)
    
    # Extract planes
    y_data = yuv_data[0:y_plane_size]
    u_data = yuv_data[y_plane_size:y_plane_size + u_plane_size]
    v_data = yuv_data[y_plane_size + u_plane_size:y_plane_size + u_plane_size + v_plane_size]
    
    yuyv_packed = bytearray()
    
    # Process each pair of pixels
    for row in range(height):
        for col in range(0, width, 2):
            # Get sample indices
            y0_idx = (row * width + col) * 2
            y1_idx = (row * width + col + 1) * 2
            c_idx = (row * (width // 2) + col // 2) * 2
            
            # Extract 10-bit samples from 16-bit little-endian words
            # Make sure we don't go out of bounds
            if y0_idx + 2 <= len(y_data) and y1_idx + 2 <= len(y_data) and c_idx + 2 <= len(u_data) and c_idx + 2 <= len(v_data):
                y0 = struct.unpack('<H', y_data[y0_idx:y0_idx+2])[0] & 0x3FF
                y1 = struct.unpack('<H', y_data[y1_idx:y1_idx+2])[0] & 0x3FF
                u = struct.unpack('<H', u_data[c_idx:c_idx+2])[0] & 0x3FF
                v = struct.unpack('<H', v_data[c_idx:c_idx+2])[0] & 0x3FF
                
                # Pack as YUYV in little-endian 16-bit format
                yuyv_packed.extend(struct.pack('<HHHH', y0, u, y1, v))
            else:
                # Pad with neutral values if we run out
                yuyv_packed.extend(struct.pack('<HHHH', 512, 512, 512, 512))
    
    return yuyv_packed


def pad_to_720_width(yuyv_data, width, height):
    """
    Pad YUYV 4:2:2 data to 720 pixels wide if narrower
    
    Pads with black (Y=64, U=512, V=512) in 10-bit studio range.
    Useful for MOV files that are cropped to 702x576 after extraction.
    
    Args:
        yuyv_data: YUYV packed data
        width: Current width (must be even)
        height: Frame height
    
    Returns:
        Padded YUYV data (if width < 720) or original data (if already 720+)
    """
    if width >= 720:
        return yuyv_data
    
    target_width = 720
    padded_data = bytearray()
    
    # Neutral (black) pixel pair: Y=64, U=512, V=512 (10-bit studio range)
    # Each pixel pair = 8 bytes (Y0, U, Y1, V as 16-bit values)
    neutral_pair = struct.pack('<HHHH', 64, 512, 64, 512)
    
    # Calculate padding
    pixels_to_add = target_width - width
    pairs_to_add = pixels_to_add // 2
    
    # Distribute padding: left_pairs on left, right_pairs on right
    # If odd total, put extra pair on right
    left_pairs = pairs_to_add // 2
    right_pairs = pairs_to_add - left_pairs
    
    left_pad = neutral_pair * left_pairs
    right_pad = neutral_pair * right_pairs
    
    # Each line: width pixels = (width/2) pairs = (width/2) * 8 bytes
    line_size = (width // 2) * 8
    
    # Process each line
    for line_idx in range(height):
        line_start = line_idx * line_size
        line_end = line_start + line_size
        line_data = yuyv_data[line_start:line_end]
        
        # Write: left padding + original line + right padding
        padded_data.extend(left_pad)
        padded_data.extend(line_data)
        padded_data.extend(right_pad)
    
    return padded_data


def extract_frame_to_yuv422(mov_file, output_file):
    """
    Extract first frame from MOV file and convert to YUV422 raw format
    
    The MOV file is expected to be in v210 format (10-bit 4:2:2 YCbCr).
    Output is YUYV-packed format with 10-bit values in 16-bit little-endian
    containers (ITU-R BT.601 studio range: Y 64-940, Cb/Cr 64-960).
    
    Args:
        mov_file: Path to input MOV file
        output_file: Path to output .raw file
    
    Returns:
        True if successful, False otherwise
    """
    # Check if input file exists
    if not os.path.exists(mov_file):
        print(f"Error: Input file not found: {mov_file}", file=sys.stderr)
        return False
    
    # Get video dimensions
    width, height = get_video_info(mov_file)
    if width is None or height is None:
        return False
    
    print(f"Input: {mov_file}")
    print(f"  Dimensions: {width}x{height}")
    print(f"  Output: {output_file}")
    
    # Use ffmpeg to extract first frame in YUV422P10LE format (10-bit 4:2:2 planar)
    temp_yuv = output_file + ".tmp.yuv"
    
    try:
        # Extract first frame in YUV422P10LE format
        # Use -vf format=yuv422p10le to ensure we get the full 720x576 without applying crop metadata
        cmd = [
            'ffmpeg',
            '-i', mov_file,
            '-vframes', '1',
            '-vf', 'format=yuv422p10le',
            '-f', 'rawvideo',
            '-an',  # no audio
            '-y',   # overwrite
            temp_yuv
        ]
        
        print("  Extracting with ffmpeg...")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Error: ffmpeg conversion failed", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            if os.path.exists(temp_yuv):
                os.remove(temp_yuv)
            return False
        
        # Read the YUV data
        with open(temp_yuv, 'rb') as f:
            yuv_data = f.read()
        
        print("  Converting from YUV422P10LE to YUYV 10-bit...")
        
        # Calculate actual width from file size
        # YUV422P10LE total size = 4 * actual_width * height
        # (Y_plane + U_plane + V_plane = width*height*2 + width*height)
        actual_width = len(yuv_data) // (4 * height)
        
        if actual_width != width:
            print(f"  Note: YUV data is {actual_width}x{height} (will be padded to {width}x{height})")
        
        # Convert to YUYV 10-bit packed format using actual width
        yuyv_packed = convert_yuv422p10le_to_yuyv10bit(yuv_data, actual_width, height)
        
        # Pad to target width if needed
        if actual_width < width:
            print(f"  Padding from {actual_width} to {width} pixels wide...")
            yuyv_packed = pad_to_720_width(yuyv_packed, actual_width, height)
        
        # Write YUYV packed data to output file
        with open(output_file, 'wb') as f:
            f.write(yuyv_packed)
        
        # Clean up temp file
        if os.path.exists(temp_yuv):
            os.remove(temp_yuv)
        
        output_size = len(yuyv_packed)
        expected_output_size = width * height * 4  # 4 bytes per 2 pixels
        
        if output_size == expected_output_size:
            print(f"  Success! Output size: {output_size} bytes ({width}x{height})")
            return True
        else:
            print(f"Error: Output size mismatch. Expected {expected_output_size}, got {output_size}", 
                  file=sys.stderr)
            return False
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        if os.path.exists(temp_yuv):
            os.remove(temp_yuv)
        return False


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 convert_mov_to_yuv422.py <input.mov> [output.raw]", file=sys.stderr)
        print("\nConverts MOV files (v210 format) to raw YUV422 format", file=sys.stderr)
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Generate output filename if not provided
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        # Use input filename with .raw extension
        output_file = Path(input_file).stem + ".raw"
    
    # Convert the file
    success = extract_frame_to_yuv422(input_file, output_file)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
