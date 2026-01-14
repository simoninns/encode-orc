#!/usr/bin/env python3
"""
Verify PAL VITS signal generation in TBC file
"""

import struct
import sys

def analyze_line(filename, field_num, line_num):
    """Extract and analyze a specific line from a TBC file"""
    
    FIELD_WIDTH = 1135
    FIELD_HEIGHT = 313
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
        
        # Convert to mV (0x0000 = -300mV, 0x4000 = 0mV, 0xE000 = 700mV)
        def to_mv(val):
            return (val / 65535.0) * 1203.3 - 300.0
        
        print(f"\nField {field_num}, Line {line_num}:")
        print(f"  Min:  0x{min_val:04x} ({to_mv(min_val):7.2f} mV)")
        print(f"  Max:  0x{max_val:04x} ({to_mv(max_val):7.2f} mV)")
        print(f"  Mean: 0x{int(mean_val):04x} ({to_mv(mean_val):7.2f} mV)")
        
        # Check for specific features
        if max_val > 0xE000:
            print(f"  ✓ Contains signal above white level (test signal content)")
        elif max_val > 0x6000:
            print(f"  ✓ Contains active video content")
        else:
            print(f"  • Blanking level only")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: verify_vits.py <tbc_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    print(f"Analyzing PAL VITS signals in: {filename}")
    print("=" * 60)
    
    # Field 0 (first field) - lines 19, 20
    print("\nFirst Field VITS:")
    analyze_line(filename, 0, 18)  # Line 19 (0-indexed)
    analyze_line(filename, 0, 19)  # Line 20
    
    # Field 1 (second field) - lines 332, 333
    print("\nSecond Field VITS:")
    analyze_line(filename, 1, 18)  # Line 332 (line 19 in field)
    analyze_line(filename, 1, 19)  # Line 333 (line 20 in field)
    
    # Check a blanking line for comparison
    print("\nBlanking line for comparison:")
    analyze_line(filename, 0, 10)  # Line 11 (in VBI, blanking only)
