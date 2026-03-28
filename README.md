# encode-orc

![](docs/assets/encode-orc_logotype.png)

Encode-Orc is a PAL and NTSC encoder capable of producing ld-decode and vhs-decode compatible TBC files (and SQLite metadata) to assist with testing.

## PAL-M Support

Encode-Orc also supports PAL-M output via the `palm-composite` and `palm-yc` output formats.

The current PAL-M implementation uses:

- NTSC-like 525-line / 59.94 field/s geometry and timing
- PAL-M subcarrier timing at 4fSC sample rate
- PAL-style chroma encoding and burst behavior
- NTSC-sized source assets such as 720x480 test material

The first PAL-M release has an explicit metadata policy:

- `vitc` is supported for consumer-tape style PAL-M workflows
- `vits-pal` is supported for PAL-M test-signal insertion
- `biphase-vbi` is not supported for PAL-M
- `vits` must be written explicitly as `vits-pal` for PAL-M projects
- `vitc` and VITS generators cannot be mixed in the same pipeline

The PAL-M comprehensive and GGV fixtures in this repository use NTSC source assets intentionally to validate that PAL-M accepts NTSC-like source dimensions while still producing PAL-M output metadata.

Please see the [Encode-Orc Documentation](https://simoninns.github.io/decode-orc-docs/encode-orc/) for details.

Note that encode-orc is designed to be a sub-module to the decode-orc project.

## Contributing

Contributions are welcome! Please discuss in the Domesday86 discord, raise issues or submit PRs.

## License

See [LICENSE](LICENSE) file for details.

## Related Projects

- [decode-orc](https://github.com/simoninns/decode-orc) - Encode-Orc is designed as a sub-module to decode-orc.

## Acknowledgments

This project builds upon the work of the ld-decode community and the video preservation community at large.

encode-orc's encoding is based on the ld-chroma-encoder component from the ld-decode project:
https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder