#!/usr/bin/env python3
"""Analyze VITS signal timing from generated TBC file."""

import struct
import sys

# PAL parameters
SAMPLE_RATE = 17734475  # Hz
ACTIVE_START = 185  # First sample of active video
ACTIVE_END = 1107   # Last sample of active video
ACTIVE_WIDTH = ACTIVE_END - ACTIVE_START  # 922 samples = ~52 µs
SAMPLES_PER_US = SAMPLE_RATE / 1e6  # ~17.73 samples/µs

def samples_to_us(samples):
    """Convert sample count to microseconds."""
    return samples / SAMPLES_PER_US

def analyze_line(line_data, line_number, field_number):
    """Analyze signal transitions in a line."""
    # Find transitions in luminance data
    transitions = []
    threshold = 128  # Midpoint
    prev_above = False
    
    for i in range(len(line_data)):
        above = line_data[i] >= threshold
        if above != prev_above:
            transitions.append((i, above, line_data[i]))
            prev_above = above
    
    print(f"\nLine {line_number} (Field {field_number}) Transitions:")
    print(f"  Sample offset (us): Relative to active_start ({ACTIVE_START}) | Relative to line start (0)")
    
    for sample, direction, level in transitions:
        if sample >= ACTIVE_START - 20:  # Show a few samples before active
            rel_active = sample - ACTIVE_START
            rel_line = sample
            direction_str = "↑" if direction else "↓"
            
            print(f"    Sample {sample:4d} {direction_str} (L:{rel_line:3d} µs, A:{rel_active:3.1f} µs) Level:{level:3d}")

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_vits_timing.py <tbc_file>")
        sys.exit(1)
    
    tbc_file = sys.argv[1]
    
    try:
        with open(tbc_file, 'rb') as f:
            # Each TBC line is 1820 samples (16-bit values)
            samples_per_line = 1820
            bytes_per_line = samples_per_line * 2
            
            # Read first 40 lines to find VITS (lines 19, 20)
            f.seek(0)
            
            for line_in_file in range(40):
                line_data_bytes = f.read(bytes_per_line)
                if len(line_data_bytes) < bytes_per_line:
                    break
                
                # Unpack 16-bit samples
                line_data = struct.unpack(f'>{samples_per_line}H', line_data_bytes)
                
                # Calculate which PAL line this is (assuming frame starts at line 1)
                line_number = (line_in_file % 625) + 1
                field_number = 1 if line_in_file < 312 else 2
                
                if line_number == 19 or line_number == 20:
                    analyze_line(line_data, line_number, field_number)
    
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
