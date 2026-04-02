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

$$f_{SC} = \frac{283 + \frac{3}{4} + \frac{1}{625 \times 4}}{1} \times f_H
         \approx 4{,}433{,}618.75\ \text{Hz}$$

where $f_H$ is the line rate.  This gives:

$$\text{cycles per line} = \frac{f_{SC}}{f_H} \approx 283.7516$$

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
a fixed-height buffer of **313 lines** (plus line 0 as a header), regardless of the
nominal 312.5-line duration:

- **First fields** (odd fields): carry 312 active lines; line 313 is padded with a
  blanking-level line.
- **Second fields** (even fields): carry 313 active lines (the half-line "extra" is
  absorbed into the first active line).

This is a purely representational choice by ld-decode to give every field a uniform
buffer size.  It does **not** mean that a PAL second field is actually 313 lines long in
the broadcast signal.

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

The PAL colour-carrier applies a line-by-line $\pm 135°$ V-component phase alternation
(the "V-switch") to suppress cross-colour patterning.  Over the 8-field cycle the
V-switch polarity, combined with the accumulated subcarrier phase, produces eight distinct
burst phasors that ld-decode votes on to identify the field position within the sequence.

### Phase IDs and V-switch polarity

The table below gives the V-switch polarity required at the ld-decode vote lines for each
PAL phase ID.  "Rising" means `Re(burst phasor) > 0` under ld-decode's internal fsc
reference; "Falling" means `Re(burst phasor) < 0`.  The `m4 == 2` inversion applied
inside `determine_field_number()` is already accounted for.

| phase_id | Field type | Vote rows (TBC index) | Expected polarity | V-switch |
|----------|------------|-----------------------|-------------------|----------|
| 1 | First  | 9, 13, 17, 21  | Rising  | +1 |
| 2 | Second | 10, 14, 18, 22 | Falling | −1 |
| 3 | First  | 9, 13, 17, 21  | Rising  | +1 |
| 4 | Second | 10, 14, 18, 22 | Falling | −1 |
| 5 | First  | 9, 13, 17, 21  | Falling | −1 |
| 6 | Second | 10, 14, 18, 22 | Rising  | +1 |
| 7 | First  | 9, 13, 17, 21  | Falling | −1 |
| 8 | Second | 10, 14, 18, 22 | Rising  | +1 |

The V-switch polarity is **constant for the entire duration of a field** — there is no
mid-field polarity transition.

---

## Why a Line-Counting Formula Fails

A naive implementation counts half-lines to determine where in the V-switch cycle each
line falls:

```cpp
// Incorrect — do not use
int32_t field_id   = field_number % 8;
int32_t prev_lines = ((field_id / 2) * 625)
                   + ((field_id % 2) * 313)   // ← should be 312.5, not 313
                   + (frame_line / 2);
int32_t v_switch   = (prev_lines % 2 == 0) ? 1 : -1;
```

The term `(field_id % 2) * 313` attempts to add 313 whole lines for second fields in
each frame pair.  A PAL second field is only **312.5** lines long, so this introduces a
$+0.5$-line error for every second field.  At the subcarrier rate this corresponds to:

$$\Delta\phi = 2\pi \times \frac{f_{SC}}{f_S} \times 0.5\ \text{lines in samples}
\approx 2\pi \times 141.9\ \text{cycles}
\approx -36°\ \text{net burst phase error}$$

Combined with an additional sign correction term that was added to compensate for an
unrelated earlier bug, four of the eight phase IDs (3, 4, 7, and 8) end up with inverted
V-switch polarity, causing ld-decode to report a "phaseID sequence mismatch" on every
field.  Phase 4 and phase 8 also exhibit a single vote-line anomaly (rows 17 and 18
respectively) where the burst polarity is ≈180° opposite to the other three vote rows,
indicating that the V-switch transition was occurring mid-field rather than at the field
boundary.

---

## How encode-orc Handles the V-Switch

Because the V-switch polarity is a per-field constant that is fully defined by the PAL
standard and verified against real ld-decode TBCs, encode-orc uses a direct lookup table
(LUT) keyed on `field_number % 8` rather than accumulating line counts:

```cpp
// v_switch_lut[field_number % 8]  →  +1 (Rising) or −1 (Falling)
// Indices 0–7 correspond to phase IDs 1–8.
static const int32_t pal_v_switch_lut[8] = {
//  ph1  ph2  ph3  ph4  ph5  ph6  ph7  ph8
     +1,  -1,  +1,  -1,  -1,  +1,  -1,  +1
};
int32_t v_switch = pal_v_switch_lut[field_number % 8];
double  burst_phase_offset = v_switch * (135.0 * PI / 180.0);
```

This LUT is applied uniformly in:

| Location | Purpose |
|----------|---------|
| `ColorBurstGenerator::generate_pal_burst()` | Colour-burst generation in VBlank lines |
| `ColorBurstGenerator::get_pal_v_switch()`   | V-switch query used by burst field-structure code |
| `PALActiveEncoder::calculate_v_switch()`    | V-switch for active-picture chroma lines |

By fixing the V-switch at the field level the mid-field polarity transition anomaly is
also eliminated: every line in a field uses the same V-switch sign, matching the
behaviour of a real PAL encoder.

---

## Expected TBC Output

### Composite TBC (`.tbc`)

- File contains interleaved first and second field buffers.
- Each field buffer has **313 lines × field_width samples**, stored as little-endian
  16-bit unsigned integers (0x0000 = sync tip, 0xFFFF = peak white).
- Line 0 is a header/blanking line; active video begins at line 1.
- Colour burst appears in lines 9–22 (covering both the VBlank vote region and the
  pre-active area); the burst V-switch polarity follows the LUT above.

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
- [GitHub issue #26](https://github.com/simoninns/encode-orc/issues/26) — full
  investigation log including phasor angle tables, root-cause analysis, and the derivation
  of the LUT above.
- `lddecode/core.py` (`compute_line_bursts`, `determine_field_number`) — ld-decode
  source defining the voting convention that encode-orc must satisfy.
