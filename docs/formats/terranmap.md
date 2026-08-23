# TERRANMAP evidence notes

Evidence fixture: user-owned `maps/xplanet.MP`, not distributed. The observed container is:

| Offset | Size | Current interpretation |
|---:|---:|---|
| 0 | 11 | `TERRANMAP\0` followed by DOS EOF `0x1a` |
| 11 | 4 | envelope/version bytes (observed value 5) |
| 15 | 2724 | legacy header block used by the original engine |
| 15 | 4 | logical staggered-coordinate width |
| 19 | 4 | height |
| 23 | 4 | random seed |
| 47 | 4 | landmark count |
| 51 | 40 each | landmark x, y, and 32 raw name bytes |
| 2739 | width/2 × height × 44 | tile records |
| after tiles | remainder | abstract region grid, retained verbatim |

The 44-byte tile record is climate, contour, site/owner, region, visibility, rock/lock/user, unknown byte, signed territory, 32-bit improvements, 32-bit landmark/code, then seven faction-visible improvement masks. Readers retain both decoded values and exact raw bytes. All integers are bounded little-endian reads; dimensions above 2048 are rejected.

The legacy header layout beyond these fields remains a hypothesis. Its 2,724-byte extent comes from OpenSMACX `map_read`/`map_write` and is shifted by the 15-byte file envelope.

