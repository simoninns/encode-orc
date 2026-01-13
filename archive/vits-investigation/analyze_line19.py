#!/usr/bin/env python3
"""
Analyze Line 19 VITS signal to verify it matches IEC 60856 spec.

Line 19 should contain (in order):
1. B2: White Reference Bar (0.70 Vpp ± 0.5%)
2. B1: 2T Sine-Squared Pulse (200 ± 6 ns)
3. F: 20T Carrier-Borne Sine-Squared Pulse (2000 ± 60 ns)
4. D1: 6-Level Staircase (0%, 20%, 40%, 60%, 80%, 100%)
"""

import struct
import sys
import os

def read_tbc_file_line(filename, field_num, line_in_field):
    """Read a specific line from a TBC file."""
    with open(filename, 'rb') as f:
        data = f.read()
    
    samples_per_line = 1135
    lines_per_field = 313
    sample_size = 2  # 16-bit
    
    # Calculate byte offset
    if field_num == 0:
        # First field
        offset = line_in_field * samples_per_line * sample_size
    else:
        # Second field
        offset = (lines_per_field * samples_per_line * sample_size) + (line_in_field * samples_per_line * sample_size)
    
    # Read line data
    line_data = data[offset:offset + samples_per_line * sample_size]
    samples = struct.unpack(f'{len(line_data)//2}H', line_data)
    
    return samples

def analyze_line19(samples):
    """Analyze Line 19 structure."""
    # Active video region: samples 185-1107 (922 samples)
    active_start = 185
    active_end = 1107
    active_samples = samples[active_start:active_end]
    
    blanking_level = 0x4000  # ~16384 (typical blanking)
    white_level = 0xD000    # ~53248 (typical white)
    
    print("="*80)
    print("Line 19 Analysis - IEC 60856 VITS")
    print("="*80)
    print(f"Total samples in active region: {len(active_samples)}")
    print(f"Sample range: {min(active_samples):5d} - {max(active_samples):5d}")
    print(f"Average: {sum(active_samples)//len(active_samples):5d}")
    print()
    
    # Segment the active region into expected components
    # B2: ~10% = ~92 samples
    # B1: ~4 samples
    # F: ~36 samples
    # D1: remaining ~790 samples (6 levels)
    
    b2_end = 92
    b1_end = b2_end + 4
    f_end = b1_end + 36
    d1_start = f_end
    
    print("Component Analysis:")
    print("-" * 80)
    
    # B2: White Reference Bar
    b2_samples = active_samples[:b2_end]
    print(f"B2 (White Reference Bar):")
    print(f"  Samples 0-{b2_end}: {min(b2_samples):5d} - {max(b2_samples):5d} (avg: {sum(b2_samples)//len(b2_samples):5d})")
    print(f"  Expected: ~{white_level:5d} (white level)")
    print(f"  Spec: 0.70 Vpp ± 0.5%")
    print()
    
    # B1: 2T Sine-Squared Pulse
    b1_samples = active_samples[b2_end:b1_end]
    print(f"B1 (2T Sine-Squared Pulse):")
    print(f"  Samples {b2_end}-{b1_end}: {min(b1_samples):5d} - {max(b1_samples):5d}")
    print(f"  Sample values: {[f'{s:5d}' for s in b1_samples]}")
    print(f"  Expected: Smooth rise from blanking to white and back")
    print(f"  Spec: 200 ± 6 ns, sin²(πt) envelope, ≤0.3% undershoot")
    print()
    
    # F: 20T Carrier-Borne Pulse
    f_samples = active_samples[b1_end:f_end]
    print(f"F (20T Carrier-Borne Sine-Squared Pulse):")
    print(f"  Samples {b1_end}-{f_end}: {min(f_samples):5d} - {max(f_samples):5d}")
    print(f"  Ripple (max-min): {max(f_samples) - min(f_samples)}")
    print(f"  Expected: Sine-squared envelope with subcarrier ripple")
    print(f"  Spec: 2000 ± 60 ns, ≤1% subcarrier distortion, ≤3 mVpp modulation unbalance")
    print()
    
    # D1: 6-Level Staircase
    d1_samples = active_samples[d1_start:]
    print(f"D1 (6-Level Staircase):")
    print(f"  Samples {d1_start}-end: {len(d1_samples)} samples")
    print(f"  Range: {min(d1_samples):5d} - {max(d1_samples):5d}")
    
    # Try to identify staircase levels
    level_height = len(d1_samples) // 6
    print(f"  Expected {level_height} samples per level")
    print()
    print("  Level analysis:")
    for level in range(6):
        start = level * level_height
        end = start + level_height if level < 5 else len(d1_samples)
        level_samples = d1_samples[start:end]
        if level_samples:
            level_avg = sum(level_samples) // len(level_samples)
            expected_level = int(blanking_level + (white_level - blanking_level) * level / 5)
            print(f"    Level {level}: avg={level_avg:5d} samples, expected={expected_level:5d}")
    
    print()
    print("  Spec: 6 steps (0%, 20%, 40%, 60%, 80%, 100%)")
    print("        Rise/Fall: 235 ± 15 ns, Step inequality < 0.5%")
    print("="*80)

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_line19.py <tbc_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    if not os.path.exists(filename):
        print(f"Error: File not found: {filename}")
        sys.exit(1)
    
    # Read Frame 0, Field 1, Line 19 (field index 18)
    print(f"Reading: {filename}")
    print(f"Frame 0, Field 1 (first field), Frame Line 19 (field index 18)")
    print()
    
    samples = read_tbc_file_line(filename, 0, 18)
    analyze_line19(samples)

if __name__ == "__main__":
    main()
