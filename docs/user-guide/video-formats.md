# Supported Video Formats

## PAL (50i, 576 lines)
- Resolution: 720×576 interlaced
- Frame rate: 25 fps (50 fields/sec)
- Aspect ratio: 4:3 or 16:9

## NTSC (59.94i, 480 lines)
- Resolution: 720×480 interlaced
- Frame rate: 29.97 fps (59.94 fields/sec)
- Aspect ratio: 4:3 or 16:9

## PAL-M (59.94i, 480 lines)
- Resolution: 720×480 interlaced source material
- Frame rate: 29.97 fps (59.94 fields/sec)
- Aspect ratio: 4:3 or 16:9
- Output formats: `palm-composite` and `palm-yc`
- Timing: 525-line geometry like NTSC
- Color encoding: PAL-style chroma modulation with PAL-M subcarrier timing

## PAL-M Metadata Notes

- `vitc` is supported for consumer-tape style PAL-M workflows
- `vits-pal` is supported for PAL-M test-signal insertion
- `vits` is ambiguous for PAL-M projects; use `vits-pal` explicitly
- `biphase-vbi` is not currently supported for PAL-M
