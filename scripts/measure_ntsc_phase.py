#!/usr/bin/env python3
import sys
import struct
import sqlite3
import math

def load_capture_params(db_path):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("SELECT system, video_sample_rate, field_width, field_height, colour_burst_start, colour_burst_end, blanking_16b_ire FROM capture LIMIT 1;")
    row = cur.fetchone()
    if not row:
        raise RuntimeError("capture table not found in DB")
    system, sample_rate, field_width, field_height, burst_start, burst_end, blanking_ire = row
    conn.close()
    return system, sample_rate, field_width, field_height, burst_start, burst_end, blanking_ire


def read_field_line_segment(tbc_path, field_idx, line_idx, start, end, width, height):
    with open(tbc_path, 'rb') as f:
        field_samples = width * height
        offset_samples = field_idx * field_samples + line_idx * width + start
        f.seek(offset_samples * 2)
        segment_len = end - start
        data = f.read(segment_len * 2)
        if len(data) != segment_len * 2:
            raise RuntimeError("short read from TBC file")
        seg = list(struct.unpack('<' + 'H'*segment_len, data))
        return seg


def measure_phase(segment, blanking_ire, sample_rate, fsc):
    # subtract blanking level to center around 0
    values = [x - blanking_ire for x in segment]
    L = len(values)
    omega = 2.0 * math.pi * fsc / sample_rate
    # build reference sin/cos
    sin_ref = [math.sin(omega * n) for n in range(L)]
    cos_ref = [math.cos(omega * n) for n in range(L)]
    # correlations
    a_sin = sum(v * s for v, s in zip(values, sin_ref))
    a_cos = sum(v * c for v, c in zip(values, cos_ref))
    phi = math.atan2(a_sin, a_cos)
    deg = (phi * 180.0 / math.pi) % 360.0
    # map to quadrant 1..4
    quadrant = int(((deg + 45.0) % 360.0) // 90.0) + 1
    return deg, quadrant


def main():
    if len(sys.argv) < 2:
        print("Usage: measure_ntsc_phase.py <tbc_file> [fields_to_measure] [line_idx]")
        sys.exit(1)
    tbc_path = sys.argv[1]
    fields_to_measure = int(sys.argv[2]) if len(sys.argv) > 2 else 16
    line_idx = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    db_path = tbc_path + ".db"
    system, sample_rate, width, height, burst_start, burst_end, blanking_ire = load_capture_params(db_path)
    if system == 'PAL' or system == 'PAL_M':
        fsc = 4433618.75
    else:
        fsc = 315.0e6 / 88.0
    print(f"system={system}, sample_rate={sample_rate}, width={width}, height={height}, burst=[{burst_start},{burst_end})")
    print(f"Measuring phases for first {fields_to_measure} fields at line {line_idx}:")
    for i in range(fields_to_measure):
        seg = read_field_line_segment(tbc_path, i, line_idx, burst_start, burst_end, width, height)
        deg, quad = measure_phase(seg, blanking_ire, sample_rate, fsc)
        if system == 'PAL' or system == 'PAL_M':
            # PAL burst swings ±135°, indicate sign and closest angle
            # Normalize to [-180, 180)
            d = ((deg + 180.0) % 360.0) - 180.0
            sign = '+' if d > 0 else '-'
            print(f"field {i}: phase≈{sign}135°, measured={deg:7.2f} deg")
        else:
            print(f"field {i}: phase={deg:7.2f} deg, quadrant={quad}")

if __name__ == '__main__':
    main()
