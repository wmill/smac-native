# Native replay format

SMAC Native replay logs are UTF-8, line-oriented files owned by this project. They are not an
original SMACX format. Version 1 begins with `SMAC_REPLAY 1`, records the initial authoritative-state
hash, and ends with `END`.

Each command is followed by its emitted events and resulting state hash:

```text
SMAC_REPLAY 1
INITIAL 4469cf00895bd653
COMMAND MOVE 7 6 0
EVENT UNIT_MOVED 7 0 0 6 0 3
STATE fe1fe813f2a90bd6
COMMAND END_TURN
EVENT TURN_ADVANCED 2
STATE 079bf6dc2b82843c
END
```

Command forms in version 1 are `MOVE <unit> <x> <y>` and `END_TURN`. Event forms are
`UNIT_MOVED <unit> <from-x> <from-y> <to-x> <to-y> <cost>`, `TURN_ADVANCED <turn>`, and
`COMMAND_REJECTED <hex-encoded-reason>`. Hex encoding makes arbitrary rejection text safe inside a
single canonical line; `-` represents an empty reason.

The parser rejects unknown versions, malformed ordering, oversized logs, excessive entries, and
excessive events. Playback requires the supplied initial state to match `INITIAL`, then compares both
the exact event list and state hash after every command. A format or authoritative-hash schema change
must update its version and retain explicit compatibility handling rather than silently accepting an
old log.
