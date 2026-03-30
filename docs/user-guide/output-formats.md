# Output Formats

encode-orc writes field-based video in either TBC format with SQLite metadata or a simpler standard raw format without metadata.

## Output Writer Types

Configure the writer with `output.writer` in your YAML project.

### `tbc` Writer (Default)

TBC output is intended for decode-orc and related workflows.

**Features:**
- Field-based video files
- SQLite metadata database written alongside the video
- Embedded VBI/VITC/VITS metadata reflected in the `.tbc.db` database
- Best choice for regression tests and decoder development

```yaml
output:
  filename: "output/example"
  format: "pal-composite"
  writer: "tbc"
```

Output files:
- Composite: `output/example.tbc` and `output/example.tbc.db`
- Y/C: `output/example.tbcy`, `output/example.tbcc`, and `output/example.tbc.db`

### `standard` Writer

Standard output writes the generated fields without the TBC metadata database.

**Features:**
- Raw field data only
- No SQLite sidecar database
- Useful for direct inspection or alternative processing pipelines

```yaml
output:
  filename: "output/example"
  format: "ntsc-composite"
  writer: "standard"
```

## Video Format Families

The `output.format` setting selects both the signal family and whether the result is composite or Y/C.

### Composite Output

- `pal-composite`
- `ntsc-composite`
- `palm-composite`

Composite output produces a single `.tbc` file for either writer.

### Y/C Output

- `pal-yc`
- `ntsc-yc`
- `palm-yc`

Y/C output produces `.tbcy` and `.tbcc` files.

## PAL-M Notes

- `palm-composite` and `palm-yc` produce PAL-M output using 525-line, 59.94-field geometry
- PAL-M uses PAL-style chroma encoding with PAL-M subcarrier timing
- PAL-M projects typically use NTSC-sized 720×480 source assets
- PAL-M supports `vitc` and `vits-pal`, but not `biphase-vbi`