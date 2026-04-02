# NTSC Field Structure and Phase Calculations

This document covers the 262.5-line-per-field reality of the NTSC 525/59.94 standard,
how ld-decode represents fields as 263-line buffers, the resulting impact on NTSC colour
subcarrier phase accumulation, and how encode-orc handles all of this to produce correct
TBC output.

---

## The 262.5-Line NTSC Field

An NTSC frame is exactly 525 lines at 30000/1001 ≈ 29.97 fields per second (two fields
per frame gives 60000/1001 ≈ 59.94 fields per second).  Because 525 is odd, the two
interlaced fields cannot each be exactly 262 or 263 whole lines:

| Field | Line content | Duration |
|-------|-------------|----------|
| First field  | Lines 1–262 + one half-line equalising pulse at the end  | 262.5 line-periods |
| Second field | One half-line equalising pulse at the start + lines 263–525 | 262.5 line-periods |

The half-line offset is a fundamental property of interlaced NTSC: the second field
starts exactly half a line-period after the first field ends, placing its active lines
spatially between the active lines of the first field.

### Colour subcarrier accumulation

The NTSC subcarrier frequency is defined as an exact rational multiple of the line rate:

$$f_{SC} = \frac{455}{2} \times f_H \approx 3{,}579{,}545.45\ \text{Hz}$$

which gives exactly:

$$\text{cycles per line} = \frac{455}{2} = 227.5$$

Over **262.5 line-periods** (half the 525-line frame) the subcarrier accumulates:

$$262.5 \times 227.5 = 59{,}718.75\ \text{cycles}$$

The 0.75 fractional cycle is exactly $270°$.  Each successive field therefore advances
the accumulated subcarrier phase by $270°$, and after four fields the total advance is
$4 \times 270° = 1080° = 3 \times 360°$, returning to the starting phase.  This is the
**4-field NTSC colour framing sequence**, and it only works out cleanly when
**525 half-lines** (i.e., 262.5 whole lines) are used as the field period.

Using 263 whole lines instead of 262.5 introduces a $0.5 \times 227.5 = 113.75$-cycle
($\approx 0.75 \times 360°$, i.e., $270°$) error which would completely break the
4-field sequence.

---

## The ld-decode 263-Line Field Buffer

ld-decode stores every decoded NTSC field — whether it is a first field or a second
field — in a fixed-height buffer of **263 lines** (plus line 0 as a header), regardless
of the nominal 262.5-line duration:

- **First fields** (odd fields): carry 262 active lines; line 263 is padded with a
  blanking-level line.
- **Second fields** (even fields): carry 263 active lines (the half-line "extra" is
  absorbed into the first active line).

This is a purely representational choice by ld-decode to give every field a uniform
buffer size.  It does **not** mean that an NTSC second field is actually 263 lines long
in the broadcast signal.

### TBC line indexing

ld-decode's TBC buffer lines are **0-indexed** internally.  The `compute_line_bursts()`
function in `lddecode/core.py` locates the colour burst by adding a `lineoffset` to its
line cursor before indexing `linelocs`:

| Field type | `lineoffset` | First burst vote line (TBC buffer index) |
|------------|-------------|------------------------------------------|
| First field  | 2 | begins at TBC line 9 |
| Second field | 3 | begins at TBC line 10 |

Encode-orc must therefore produce correctly-phased colour burst starting from those TBC
buffer indices for ld-decode's phase-ID measurement to work correctly.

---

## NTSC 4-Field Colour Phase Sequence

Unlike PAL there is no V-switch in NTSC — the burst phase is fixed at **180°** relative
to the subcarrier reference on every line.  The 4-field colour framing is carried
entirely by the accumulated subcarrier phase: the relationship between the burst and the
active-picture chroma shifts by $270°$ each field, cycling through four distinct
positions.

| phase_id | Field type | Accumulated subcarrier phase offset |
|----------|------------|-------------------------------------|
| 1 | First  | 0° |
| 2 | Second | 270° |
| 3 | First  | 180° |
| 4 | Second | 90° |

Because the 4-field cycle is driven purely by the accumulated subcarrier phase (not by
a separate polarity flag as in PAL), the phase_id is implicit in the continuous
subcarrier oscillator, and the burst phase offset of 180° is applied uniformly on every
line of every field.

---

## Why 263 Would Break the Phase Sequence

A naive implementation that uses 263 whole lines for the field duration instead of
262.5 line-periods would accumulate:

$$263 \times 227.5 = 59{,}832.5\ \text{cycles per field}$$

The fractional part is $0.5\ \text{cycle} = 180°$.  Each successive field would then
advance by $180°$ rather than $270°$, producing only a 2-field sequence
($2 \times 180° = 360°$) instead of the correct 4-field sequence.  This would cause
chrominance phase reversals between frames and incorrect `field_phase_id` assignment in
the ld-decode metadata.

---

## How encode-orc Handles NTSC Phase

Encode-orc models the NTSC field duration as a **`double`** with the value `262.5`,
preserving the half-line offset between fields:

```cpp
double ColorBurstGenerator::calculate_ntsc_phase(
    int32_t field_number, int32_t line_number, int32_t sample) const
{
    // 262.5 lines per field — the 0.5 preserves the half-line offset that
    // produces the 4-field colour framing sequence (270° advance per field).
    const double lines_per_field = 262.5;
    const double cycles_per_line = 227.5;  // fSC = 455/2 * fH (exact)

    // Accumulated lines before this line in the full sequence
    double prev_lines = static_cast<double>(field_number) * lines_per_field
                      + static_cast<double>(line_number);

    // Total subcarrier cycles before this sample
    double prev_cycles = prev_lines * cycles_per_line;

    // Phase contribution from this sample's position within the line
    double time_phase = 2.0 * PI * subcarrier_freq_
                      * static_cast<double>(sample) / sample_rate_;
    return 2.0 * PI * prev_cycles + time_phase;
}
```

The burst phase offset is then added as a constant:

```cpp
// NTSC burst phase is fixed at 180° on every line
double burst_phase_offset = PI;  // 180°
```

The same `262.5`-based accumulation is used for active-picture chroma encoding
to keep the burst and chroma phases coherent.

---

## Expected TBC Output

### Composite TBC (`.tbc`)

- File contains interleaved first and second field buffers.
- Each field buffer has **263 lines × field_width samples**, stored as little-endian
  16-bit unsigned integers (0x0000 = sync tip, 0xFFFF = peak white).
- Line 0 is a header/blanking line; active video begins at line 1.
- Colour burst appears in the back porch of every active line, with phase fixed at 180°.

### Y/C TBC (`.tbcc` and `.tbcy`)

- Two separate files: luma (`.tbcy`) and chroma (`.tbcc`).
- Same 263-line × field_width layout per file; chroma is the separated subcarrier signal.
- Burst in the chroma file follows the same 180° reference.

### Metadata (`.tbc.db`)

The SQLite metadata database contains a `field_record` row for each field.  The
`field_phase_id` column stores the 1-indexed NTSC phase ID (1–4):

```
field_phase_id = (field_number % 4) + 1
```

The `is_first_field` column is `1` for even `field_number` values (phase IDs 1 and 3)
and `0` for odd `field_number` values (phase IDs 2 and 4).

A correctly encoded 4-field NTSC sequence therefore produces the following repeating
pattern in the database:

| field_id (0-indexed) | field_phase_id | is_first_field |
|---------------------|----------------|----------------|
| 0 | 1 | 1 |
| 1 | 2 | 0 |
| 2 | 3 | 1 |
| 3 | 4 | 0 |

#### NTSC-specific metadata fields

The `field_record` table also carries several NTSC-only columns (NULL for PAL/PAL-M):

| Column | Description |
|--------|-------------|
| `ntsc_field_flag` | White flag — set when a white-level reference line is detected |
| `ntsc_is_fm_code_data_valid` | Whether FM code data was decoded |
| `ntsc_fm_code_data` | FM code data value (if valid) |
| `ntsc_is_video_id_data_valid` | Whether Video ID data was decoded |
| `ntsc_video_id_data` | Video ID data value (if valid) |
| `ntsc_white_flag` | Raw white-flag sample from VBI |

---

## References

- ITU-R BT.470 / SMPTE 170M — NTSC colour framing and subcarrier frequency specification.
- EIA RS-170A — detailed NTSC signal timing.
- `lddecode/core.py` (`compute_line_bursts`, `determine_field_number`) — ld-decode source
  defining the colour framing convention that encode-orc must satisfy.
