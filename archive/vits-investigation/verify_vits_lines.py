#!/usr/bin/env python3
"""
Verify VITS line placement in PAL TBC output.
Expected: Frame lines 19, 20, 332, 333 should contain VITS signals.
"""

import struct
import sys
import os

def read_tbc_file(filename):
    """Read TBC file and return frame data."""
    with open(filename, 'rb') as f:
        data = f.read()
    
    # TBC format: 16-bit samples, 1135 samples per line, 313 lines per field, 2 fields per frame
    samples_per_line = 1135
    lines_per_field = 313
    sample_size = 2  # 16-bit
    
    # Total samples per frame
    samples_per_frame = samples_per_line * lines_per_field * 2
    bytes_per_frame = samples_per_frame * sample_size
    
    frames = []
    offset = 0
    
    while offset + bytes_per_frame <= len(data):
        frame_data = data[offset:offset + bytes_per_frame]
        
        # Parse frame into two fields
        field1 = []
        field2 = []
        
        for line in range(lines_per_field):
            # Field 1 line
            line_start = line * samples_per_line * sample_size
            line_end = line_start + samples_per_line * sample_size
            field1.append(frame_data[line_start:line_end])
            
            # Field 2 line (starts after field 1)
            line_start = (lines_per_field * samples_per_line * sample_size) + (line * samples_per_line * sample_size)
            line_end = line_start + samples_per_line * sample_size
            field2.append(frame_data[line_start:line_end])
        
        frames.append((field1, field2))
        offset += bytes_per_frame
    
    return frames

def analyze_line(line_data, threshold=30000):
    """Analyze if a line contains a signal (not blanking)."""
    samples = struct.unpack(f'{len(line_data)//2}H', line_data)
    
    # Get average and range
    avg = sum(samples) / len(samples)
    variation = max(samples) - min(samples)
    
    return {
        'avg': avg,
        'min': min(samples),
        'max': max(samples),
        'variation': variation,
        'has_signal': variation > threshold
    }

def frame_line_to_field_line(frame_line, field_num):
    """Convert frame line (1-625) and field number to field line (0-312/311)."""
    if frame_line < 1 or frame_line > 625:
        return None
    
    if frame_line <= 313:
        # First field
        if field_num == 0:
            return frame_line - 1  # Convert 1-based to 0-based
        else:
            return None
    else:
        # Second field
        if field_num == 1:
            return frame_line - 314  # 314->0, 625->311
        else:
            return None

def main():
    if len(sys.argv) < 2:
        print("Usage: verify_vits_lines.py <tbc_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    if not os.path.exists(filename):
        print(f"Error: File not found: {filename}")
        sys.exit(1)
    
    print(f"Reading TBC file: {filename}")
    frames = read_tbc_file(filename)
    
    if not frames:
        print("Error: No frames found in TBC file")
        sys.exit(1)
    
    print(f"Found {len(frames)} frames")
    
    # Analyze first frame
    frame_idx = 0
    field1, field2 = frames[frame_idx]
    
    print(f"\nAnalyzing frame {frame_idx}:")
    print(f"  Field 1: {len(field1)} lines")
    print(f"  Field 2: {len(field2)} lines")
    
    # Expected VITS frame lines
    vits_frame_lines = [19, 20, 332, 333]
    
    print("\n" + "="*70)
    print("VITS Line Verification")
    print("="*70)
    
    for vits_line in vits_frame_lines:
        # Determine which field this line belongs to
        if vits_line <= 313:
            field_num = 0
            field_data = field1
            field_name = "Field 1"
        else:
            field_num = 1
            field_data = field2
            field_name = "Field 2"
        
        field_line = frame_line_to_field_line(vits_line, field_num)
        
        if field_line is None or field_line >= len(field_data):
            print(f"Frame line {vits_line:3d}: ERROR - invalid mapping")
            continue
        
        # Analyze the line
        analysis = analyze_line(field_data[field_line])
        has_signal = "✓ VITS SIGNAL PRESENT" if analysis['has_signal'] else "✗ NO SIGNAL (blanking)"
        
        print(f"Frame line {vits_line:3d} ({field_name}, field index {field_line:3d}): "
              f"avg={analysis['avg']:8.0f} range={analysis['variation']:6.0f} {has_signal}")
    
    print("="*70)

if __name__ == "__main__":
    main()
