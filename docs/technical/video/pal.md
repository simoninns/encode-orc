# PAL Field Structure and Phase Calculations

This document covers the 312.5-line-per-field reality of the PAL 625/50 standard, how
ld-decode represents fields as 313-line buffers, the resulting impact on PAL colour-burst
V-switch phase calculations, and how encode-orc handles all of this to produce correct TBC
output.

---

## The 312.5-Line PAL Field

A PAL frame is exactly 625 lines at 50 fields per second.  Because 625 is odd, the two
interlaced fields cannot each be exactly 312 or 313 whole lines:

| Field | Line content | Duration |
|-------|-------------|----------|
| First field  | Lines 1–312 + one half-line equalising pulse at the end  | 312.5 line-periods |
| Second field | One half-line equalising pulse at the start + lines 313–625 | 312.5 line-periods |

The half-line offsets are a fundamental property of interlaced PAL: the second field
starts exactly half a line-period after the first field ends, which is what places its
active lines spatially between the active lines of the first field.

### Colour subcarrier accumulation

PAL uses a subcarrier frequency of:

$$f_{SC} = \left(\frac{1135}{4} + \frac{1}{625}\right) \times f_H
         \approx 4{,}433{,}618.75\ \text{Hz}$$

where $f_H$ is the line rate.  This gives:

$$\text{cycles per line} = \frac{f_{SC}}{f_H} = \frac{1135}{4} + \frac{1}{625} \approx 283.7516$$

Over 312 whole lines the subcarrier does not land on a repeating phase, but over
**312.5 line-periods** (half the 625-line frame) it accumulates:

$$312.5 \times 283.7516 \approx 88{,}661.75\ \text{cycles}$$

The 0.75 fractional cycle is exactly $270°$, giving the $\pm 90°$ per-field phase
increment that encodes the PAL sequencing.  This only works out cleanly when **625
half-lines** (i.e., 312.5 whole lines) are used as the field period — using 313 whole
lines introduces a $0.5 \times 283.7516 \approx 141.9$-cycle ($\approx 0.9 \times 360°$,
i.e., $−36°$) error.

---

## The ld-decode 313-Line Field Buffer

ld-decode stores every decoded field — whether it is a first field or a second field — in
a fixed-height buffer of **313 lines**, regardless of the nominal 312.5-line duration:

- **Both first and second fields** are stored with exactly 313 lines.
- **Line 313** (the last line) of the first field buffer contains the half-line at the
  PAL field boundary: the first half is the end of field 1's equalising pulse, and the
  second half is the start of the field 2 broad vsync pulse.  ld-decode captures the
  raw signal as-is, so this interfield half-line appears naturally in both adjacent field
  buffers.
- There is **no blanking padding** inserted between field 1 and field 2 — the 625-line
  frame content is stored continuously across both field buffers.

### TBC line indexing

ld-decode's TBC buffer lines are **0-indexed** internally.  When `compute_line_bursts()`
in `lddecode/core.py` votes on colour-burst polarity to determine the PAL 8-field
phase ID, it is called with `l ∈ {7, 11, 15, 19}` and adds a `lineoffset` before
indexing into the `linelocs` array:

| Field type | `lineoffset` | Vote lines (TBC buffer index) |
|------------|-------------|-------------------------------|
| First field  | 2 | 9, 13, 17, 21 |
| Second field | 3 | 10, 14, 18, 22 |

Encode-orc must therefore place correctly-phased colour burst at those TBC buffer indices
for the phase-ID determination to work.

---

## PAL 8-Field Colour Phase Sequence and V-Switch

The PAL colour-carrier applies a line-by-line $\pm V$ phase alternation (the "V-switch")
to suppress cross-colour patterning.  On each successive line the sign of the encoded V
component is flipped: the burst phasor moves between $+135°$ and $-135°$ from the U-axis,
and the active-picture chroma modulation follows the same alternation.

The V-switch sequence repeats every two lines, but because the 8-field cycle is 5,000
lines long (8 × 625), the starting polarity at any given field depends on the field's
position in the 8-field sequence.  ld-decode identifies that position by voting on the
burst polarity at four fixed lines per field.

### Phase IDs and burst polarity at ld-decode vote lines

| phase_id | Field type | Vote rows (TBC index) | Burst phasor |
|----------|------------|-----------------------|--------------|
| 1 | First  | 9, 13, 17, 21  | $+135°$ |
| 2 | Second | 10, 14, 18, 22 | $-135°$ |
| 3 | First  | 9, 13, 17, 21  | $+135°$ |
| 4 | Second | 10, 14, 18, 22 | $-135°$ |
| 5 | First  | 9, 13, 17, 21  | $-135°$ |
| 6 | Second | 10, 14, 18, 22 | $+135°$ |
| 7 | First  | 9, 13, 17, 21  | $-135°$ |
| 8 | Second | 10, 14, 18, 22 | $+135°$ |

The vote-line polarity is consistent within each field because the V-switch alternates
line-by-line starting from a well-defined position at the top of each field, and the four
vote lines are all even-spaced (every 4 lines) so they all fall on the same half of the
alternation cycle within a given field.

---

## How encode-orc Handles the V-Switch

The V-switch is computed identically for both the colour burst and the active-picture
chroma modulation.  Both must use the same per-line sequence — any mismatch between them
causes the chroma decoder's delay-line cancellation to fail, producing visible
line-by-line colour errors in the decoded output.

### Per-line formula

The frame line number is derived from the field-relative line number and the
`is_first_field` flag, then used to count the total lines elapsed since the start of the
8-field sequence:

```cpp
// frame_line: 1-indexed PAL frame line number within the 8-field cycle
int32_t frame_line = is_first_field ? (line_number * 2 + 1)
                                    : (line_number * 2 + 2);

// Count half-lines elapsed before this frame line across the 8-field sequence.
// Each 8-field cycle covers 4 frames × 625 lines = 5000 lines total.
// Within the 8 fields: fields pair up into frames; field_id/2 gives the frame index.
int32_t field_id   = field_number % 8;
int32_t prev_lines = ((field_id / 2) * 625)      // lines from whole frames
                   + ((field_id % 2) * 313)       // lines from the current first field
                   + (frame_line / 2);            // lines before this frame line

// V-switch alternates every line
int32_t v_switch = (prev_lines % 2 == 0) ? +1 : -1;
```

This formula is applied in:

| Location | Purpose |
|----------|---------|
| `PALActiveEncoder::calculate_v_switch()`    | V-switch for active-picture chroma encoding |
| `ColorBurstGenerator::generate_pal_burst()` | Burst phasor angle ($\pm135°$) for each line |
### Why burst and active-video must use the same formula

The PAL chroma decoder (e.g. the delay-line decoder in decode-orc) recovers U and V by
adding and subtracting adjacent lines:

$$U = \frac{C_n + C_{n-1}}{2}, \quad V = \frac{C_n - C_{n-1}}{2}$$

This cancellation only works when the burst reference used by the decoder tracks the same
V-switch sequence as the modulated chroma.  If the burst and the active-video chroma use
different V-switch sequences, the decoder sees a spurious alternating-line error that
manifests as horizontal colour stripes in the decoded image.

---

## Expected TBC Output

### Composite TBC (`.tbc`)

- File contains interleaved first and second field buffers.
- Each field buffer has **313 lines × field_width samples**, stored as little-endian
  16-bit unsigned integers (0x0000 = sync tip, 0xFFFF = peak white).
- Line 0 is a header/blanking line; active video begins at line 1.
- Colour burst appears in lines 9–22 (covering both the VBlank vote region and the
  pre-active area); the burst phasor alternates per-line using the same V-switch
  formula as the active-picture encoder.
- On line 6 of field phases 1, 4, 5, and 8 the colour burst is suppressed entirely.
  ld-decode uses the absence of burst on line 6 as an additional signal for determining
  the PAL 8-field phase ID.

### Y/C TBC (`.tbcc` and `.tbcy`)

- Two separate files: luma (`.tbcy`) and chroma (`.tbcc`).
- Same 313-line × field_width layout per file; chroma is the separated subcarrier signal.
- V-switch polarity applies equally to the chroma channel.

### Metadata (`.tbc.db`)

The SQLite metadata database contains a `field_record` row for each field.  The
`field_phase_id` column stores the 1-indexed PAL phase ID (1–8):

```
field_phase_id = (field_number % 8) + 1
```

The `is_first_field` column is `1` for even `field_number` values (phase IDs 1, 3, 5, 7)
and `0` for odd `field_number` values (phase IDs 2, 4, 6, 8).

A correctly encoded 8-field PAL sequence therefore produces the following repeating
pattern in the database:

| field_id (0-indexed) | field_phase_id | is_first_field |
|---------------------|----------------|----------------|
| 0 | 1 | 1 |
| 1 | 2 | 0 |
| 2 | 3 | 1 |
| 3 | 4 | 0 |
| 4 | 5 | 1 |
| 5 | 6 | 0 |
| 6 | 7 | 1 |
| 7 | 8 | 0 |

---

## Verification

The `tools/compare_burst_phase.py` script from the
[ld-recode](https://github.com/simoninns/ld-recode) repository can be used to
measure the burst phasor angle at the correct vote rows from any encode-orc `.tbc` +
`.tbc.db` output.  For a correctly encoded PAL TBC, all eight "ok?" columns in the
script's output should show `✓`.

Alternatively, passing the TBC through ld-recode and then ld-decode should produce no
"phaseID sequence mismatch" warnings in the ld-decode log.

---

## References

- EBU Tech 3299 / ITU-R BT.470 — PAL colour framing and 8-field sequence specification.
- [GitHub issue #26](https://github.com/simoninns/encode-orc/issues/26) — investigation
  log covering V-switch polarity, burst phasor angles, and field-phase identification.
- `lddecode/core.py` (`compute_line_bursts`, `determine_field_number`) — ld-decode
  source defining the voting convention that encode-orc must satisfy.
