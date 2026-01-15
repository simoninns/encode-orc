#!/usr/bin/env python3
"""
Verify NTSC VITS signals in generated .tbc files
"""

import struct
import sys
import numpy as np

def sample_to_ire(sample, blanking_16b=0x3c00, white_16b=0xc100):
    """Convert 16-bit sample value to IRE"""
    sync_level = 0x0000
    
    if sample <= blanking_16b:
        # In sync/blanking region
        sync_range = blanking_16b - sync_level
        if sync_range == 0:
            return 0
        return -43.0 * (blanking_16b - sample) / sync_range
    else:
        # In active video region
        luma_range = white_16b - blanking_16b
        if luma_range == 0:
            return 0
        return 100.0 * (sample - blanking_16b) / luma_range

def analyze_ntsc_vits(filename):
    """Analyze NTSC VITS signals"""
    
    try:
        with open(filename, 'rb') as f:
            data = f.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        return False
    
    # NTSC field parameters
    field_width = 910
    field_height = 263
    field_size = field_width * field_height * 2  # 16-bit samples
    
    print(f"File size: {len(data)} bytes")
    print(f"Expected field size: {field_size} bytes")
    print(f"Number of fields: {len(data) // field_size}")
    print()
    
    # Analyze lines 18-20 (VIR lines 19-20 are at indices 18-19 in 0-based counting)
    print("=" * 70)
    print("NTSC VITS Analysis - Corrected VIR Timing")
    print("=" * 70)
    print()
    
    blanking_16b = 0x3c00  # Blanking level for NTSC
    white_16b = 0xc100    # White level
    
    # Read first field
    field_data = data[0:field_size]
    samples = struct.unpack(f'<{field_width * field_height}H', field_data)
    
    # Reshape to [line][sample]
    lines = np.array(samples).reshape((field_height, field_width))
    
    # Analyze VIR line (line 18, zero-indexed for line 19)
    print("Field 0 - VIR Line 19 Analysis:")
    print("-" * 70)
    
    line_idx = 18
    line_data = lines[line_idx]
    min_val = np.min(line_data)
    max_val = np.max(line_data)
    mean_val = np.mean(line_data)
    
    min_ire = sample_to_ire(int(min_val), blanking_16b, white_16b)
    max_ire = sample_to_ire(int(max_val), blanking_16b, white_16b)
    mean_ire = sample_to_ire(int(mean_val), blanking_16b, white_16b)
    
    print(f"Overall line statistics:")
    print(f"  Min: 0x{int(min_val):04x} ({min_ire:6.2f} IRE)")
    print(f"  Max: 0x{int(max_val):04x} ({max_ire:6.2f} IRE)")
    print(f"  Mean: 0x{int(mean_val):04x} ({mean_ire:6.2f} IRE)")
    print()
    
    # Analyze specific timing regions for VIR
    print(f"Timing analysis (corrected):")
    
    # Calculate sample boundaries (NTSC = 910 samples per line for 63.556µs)
    # Each microsecond ≈ 14.32 samples
    samples_per_us = field_width / 63.556
    
    regions = [
        ("5.5-12µs (spacing)", int(5.5 * samples_per_us), int(12.0 * samples_per_us), 0, 0),
        ("12-36µs (chroma ref 50-90)", int(12.0 * samples_per_us), int(36.0 * samples_per_us), 50, 90),
        ("36-48µs (luma ref)", int(36.0 * samples_per_us), int(48.0 * samples_per_us), 50, 50),
        ("48-60µs (black ref)", int(48.0 * samples_per_us), int(60.0 * samples_per_us), 7.5, 7.5),
    ]
    
    for region_name, start_samp, end_samp, expected_min, expected_max in regions:
        if end_samp > field_width:
            end_samp = field_width
        region_data = line_data[start_samp:end_samp]
        region_min = np.min(region_data)
        region_max = np.max(region_data)
        region_mean = np.mean(region_data)
        
        region_min_ire = sample_to_ire(int(region_min), blanking_16b, white_16b)
        region_max_ire = sample_to_ire(int(region_max), blanking_16b, white_16b)
        region_mean_ire = sample_to_ire(int(region_mean), blanking_16b, white_16b)
        
        print(f"  {region_name}:")
        print(f"    Min: {region_min_ire:6.2f} IRE, Max: {region_max_ire:6.2f} IRE, Mean: {region_mean_ire:6.2f} IRE")
        if expected_min == expected_max:
            print(f"    Expected: ~{expected_min:.1f} IRE")
        else:
            print(f"    Expected: {expected_min:.1f}-{expected_max:.1f} IRE")
    
    print()
    
    # Compare with non-VITS lines
    print("Non-VITS Lines (blanking only, for comparison):")
    print("-" * 70)
    
    for line_idx in [5, 10]:
        line_data = lines[line_idx]
        min_val = np.min(line_data)
        max_val = np.max(line_data)
        
        min_ire = sample_to_ire(int(min_val), blanking_16b, white_16b)
        max_ire = sample_to_ire(int(max_val), blanking_16b, white_16b)
        
        print(f"Line {line_idx + 1}: Min: {min_ire:6.2f} IRE, Max: {max_ire:6.2f} IRE")
    
    print()
    return True

def analyze_line(filename, field_num, line_num):
    """Extract and analyze a specific line from a TBC file"""
    
    FIELD_WIDTH = 910
    FIELD_HEIGHT = 263
    SAMPLES_PER_FIELD = FIELD_WIDTH * FIELD_HEIGHT
    
    # Calculate offset to the specific line
    offset = (field_num * SAMPLES_PER_FIELD + line_num * FIELD_WIDTH) * 2  # 2 bytes per sample
    
    with open(filename, 'rb') as f:
        f.seek(offset)
        data = f.read(FIELD_WIDTH * 2)
        
        if len(data) != FIELD_WIDTH * 2:
            print(f"Error: Could not read full line (got {len(data)} bytes)")
            return
        
        # Unpack as little-endian uint16
        samples = struct.unpack(f'<{FIELD_WIDTH}H', data)
        
        # Calculate statistics
        min_val = min(samples)
        max_val = max(samples)
        mean_val = sum(samples) / len(samples)
        
        blanking_16b = 0x3c00
        white_16b = 0xc100
        
        min_ire = sample_to_ire(min_val, blanking_16b, white_16b)
        max_ire = sample_to_ire(max_val, blanking_16b, white_16b)
        mean_ire = sample_to_ire(int(mean_val), blanking_16b, white_16b)
        
        print(f"\nField {field_num}, Line {line_num + 1}:")
        print(f"  Min:  0x{min_val:04x} ({min_ire:7.2f} IRE)")
        print(f"  Max:  0x{max_val:04x} ({max_ire:7.2f} IRE)")
        print(f"  Mean: 0x{int(mean_val):04x} ({mean_ire:7.2f} IRE)")
        
        # Check for specific features
        if max_val > 0xE000:
            print(f"  ✓ Contains signal above white level (test signal content)")
        elif max_val > 0x6000:
            print(f"  ✓ Contains active video content")
        else:
            print(f"  • Blanking level only")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: verify_ntsc_vits.py <filename>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    print(f"Analyzing NTSC VITS signals in {filename}\n")
    
    # Perform detailed analysis
    success = analyze_ntsc_vits(filename)
    
    sys.exit(0 if success else 1)
