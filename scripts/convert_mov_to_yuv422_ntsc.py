#!/usr/bin/env python3
"""
Convert MOV files (10-bit v210 YCbCr) to raw YUV422 format for NTSC

Takes a MOV file containing NTSC testcards in v210 format and extracts
the first frame, converting it to raw YUV422 format (YUYV-packed, 10-bit
in 16-bit little-endian containers, ITU-R BT.601 studio range).

Usage:
    python3 convert_mov_to_yuv422_ntsc.py <input.mov> [output.raw]

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


def pad_to_target_width(yuyv_data, current_width, target_width, height):
    """
    Pad YUYV 4:2:2 data to target width if narrower
    
    Pads with black (Y=64, U=512, V=512) in 10-bit studio range.
    
    Args:
        yuyv_data: YUYV packed data
        current_width: Current width (must be even)
        target_width: Target width (must be even)
        height: Frame height
    
    Returns:
        Padded YUYV data (if current_width < target_width) or original data
    """
    if current_width >= target_width:
        return yuyv_data
    
    padded_data = bytearray()
    
    # Neutral (black) pixel pair: Y=64, U=512, V=512 (10-bit studio range)
    # Each pixel pair = 8 bytes (Y0, U, Y1, V as 16-bit values)
    neutral_pair = struct.pack('<HHHH', 64, 512, 64, 512)
    
    # Calculate padding
    pixels_to_add = target_width - current_width
    pairs_to_add = pixels_to_add // 2
    
    # Distribute padding: left_pairs on left, right_pairs on right
    # If odd total, put extra pair on right
    left_pairs = pairs_to_add // 2
    right_pairs = pairs_to_add - left_pairs
    
    left_pad = neutral_pair * left_pairs
    right_pad = neutral_pair * right_pairs
    
    # Each line in YUYV format:
    # For current_width pixels: each pair of pixels = 8 bytes (Y0 U Y1 V as 16-bit values)
    # So current_width pixels = (current_width / 2) * 8 bytes = current_width * 4 bytes
    line_size = current_width * 4
    
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


def extract_frame_to_yuv422(mov_file, output_file, target_width=720, target_height=480):
    """
    Extract first frame from MOV file and convert to YUV422 raw format
    
    The MOV file is expected to be in v210 format (10-bit 4:2:2 YCbCr).
    Output is YUYV-packed format with 10-bit values in 16-bit little-endian
    containers (ITU-R BT.601 studio range: Y 64-940, Cb/Cr 64-960).
    
    For NTSC, the standard resolution is 720×480.
    
    Args:
        mov_file: Path to input MOV file
        output_file: Path to output .raw file
        target_width: Target output width (default 720 for NTSC)
        target_height: Target output height (default 480 for NTSC)
    
    Returns:
        True if successful, False otherwise
    """
    # Check if input file exists
    if not os.path.exists(mov_file):
        print(f"Error: Input file not found: {mov_file}", file=sys.stderr)
        return False
    
    # Get video dimensions
    reported_width, reported_height = get_video_info(mov_file)
    if reported_width is None or reported_height is None:
        return False
    
    print(f"Input: {mov_file}")
    print(f"  Reported dimensions: {reported_width}x{reported_height}")
    print(f"  Target dimensions: {target_width}x{target_height}")
    print(f"  Output: {output_file}")
    
    # Use ffmpeg to extract first frame in YUV422P10LE format (10-bit 4:2:2 planar)
    temp_yuv = output_file + ".tmp.yuv"
    
    try:
        # Extract first frame in YUV422P10LE format
        # Use -vf format=yuv422p10le to ensure we get the correct format
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
        
        # Calculate actual dimensions from file size
        # YUV422P10LE: Y plane (W*H*2) + U plane (W/2*H*2) + V plane (W/2*H*2)
        # Total = W*H*2 + W*H = W*H*4 bytes
        # So: actual_width = len(yuv_data) / (4 * height)
        
        # Try to determine actual width from file size
        # We need to figure out the actual height first
        actual_height = target_height
        actual_width = len(yuv_data) // (4 * actual_height)
        
        if actual_width * actual_height * 4 != len(yuv_data):
            # Try reported height
            actual_height = reported_height
            actual_width = len(yuv_data) // (4 * actual_height)
        
        print(f"  Actual YUV data dimensions: {actual_width}x{actual_height}")
        
        # Convert to YUYV 10-bit packed format using actual dimensions
        yuyv_packed = convert_yuv422p10le_to_yuyv10bit(yuv_data, actual_width, actual_height)
        
        # Pad width if needed
        if actual_width < target_width:
            print(f"  Padding width from {actual_width} to {target_width} pixels...")
            yuyv_packed = pad_to_target_width(yuyv_packed, actual_width, target_width, actual_height)
        
        # If height doesn't match, we need to crop or pad
        if actual_height != target_height:
            print(f"  Adjusting height from {actual_height} to {target_height}...")
            if actual_height > target_height:
                # Crop: keep only first target_height lines
                line_size = target_width * 2 * 2  # width * 2 samples per pixel pair * 2 bytes per sample
                yuyv_packed = yuyv_packed[:target_height * line_size]
            else:
                # Pad with black lines at bottom
                line_size = target_width * 2 * 2
                black_line = struct.pack('<HHHH', 64, 512, 64, 512) * (target_width // 2)
                lines_to_add = target_height - actual_height
                yuyv_packed.extend(black_line * lines_to_add)
        
        # Write YUYV packed data to output file
        with open(output_file, 'wb') as f:
            f.write(yuyv_packed)
        
        # Clean up temp file
        if os.path.exists(temp_yuv):
            os.remove(temp_yuv)
        
        output_size = len(yuyv_packed)
        expected_output_size = target_width * target_height * 4  # 4 bytes per 2 pixels (YUYV pair)
        
        if output_size == expected_output_size:
            print(f"  Success! Output size: {output_size} bytes ({target_width}x{target_height})")
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
        print("Usage: python3 convert_mov_to_yuv422_ntsc.py <input.mov> [output.raw]", file=sys.stderr)
        print("\nConverts NTSC MOV files (v210 format) to raw YUV422 format (720×480)", file=sys.stderr)
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Generate output filename if not provided
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        # Use input filename with .raw extension
        output_file = Path(input_file).stem + ".raw"
    
    # Convert the file (NTSC: 720×480)
    success = extract_frame_to_yuv422(input_file, output_file, target_width=720, target_height=480)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
