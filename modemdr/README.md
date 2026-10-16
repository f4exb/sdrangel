# modemdr

Digital radio: the data services and coding shared by the DAB (`modemdab`) and
DRM (`modemdrm`) modems. It has no Qt dependency. Namespace `modemdr`, export macro
`MODEMDR_API`.

| Files | Responsibility |
| --- | --- |
| `crc` | CRC-16 CCITT (as DAB and DRM use it: preset ones, inverted) and gzip's CRC-32 |
| `inflate` | Raw deflate and gzip members |
| `text` | Labels to UTF-8: the EBU Latin character set (ETSI TS 101 756 annex C), ISO 8859-1, UCS-2 and UTF-8; MOT ContentNames |
| `mot` | MOT (ETSI EN 301 234): header mode (slideshows) and directory mode (Broadcast Websites and SPI), from MSC data groups |
| `journaline` | Journaline (ETSI TS 102 979) objects |
| `spi` | Service and Programme Information (ETSI TS 102 818, binary encoding TS 102 371): station names, descriptions, logos and schedules, for DAB or DRM bearers |
| `viterbi` | The K=7 soft Viterbi decoder: DAB's four generators or DRM's six, with an SSE2 (x86) or NEON (AArch64) forward pass and a scalar one (`MODEMDR_SCALAR` selects it). All three make the same decisions |

The modems keep their own packet-mode demultiplexing and pass MSC data groups
in. The Qt widgets that show these services (the Journaline and website
browsers and the programme guide) are in `sdrgui/gui`.
