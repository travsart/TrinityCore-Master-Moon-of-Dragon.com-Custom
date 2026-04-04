---
allowed-tools: Read, Bash(python3:*), Grep, Glob
description: Parse WowPacketParser output or raw packet hex and map to codebase handlers
paths: src/**/Packets/*.h, src/**/Packets/*.cpp, src/**/Handlers/*.cpp, out/**/PacketLog/**
---

# Parse Packet Dump

## Instructions

Parse packet capture data for the VoxCore 12.x (Midnight) client.

### Input formats

1. **File path** — read a WowPacketParser `.txt` output file or raw hex dump file
2. **Opcode name** — look up a specific opcode (e.g., `CMSG_TRANSMOG_OUTFIT_NEW`)
3. **Opcode number** — look up by hex/decimal opcode ID
4. **Inline hex** — user pastes hex bytes directly

### Procedure

1. **Identify the opcode**: Extract the opcode name and/or number from the input
2. **Find the handler**: Search the codebase for the opcode:
   - Opcode enum: `src/server/game/Server/Protocol/Opcodes.h`
   - Handler registration: Grep for the opcode name in `src/server/game/Server/Protocol/OpcodeTable.cpp` or similar
   - Handler implementation: `src/server/game/Handlers/*.cpp`
   - Packet structure: `src/server/game/Server/Packets/*.h` and `*.cpp`
3. **Map packet fields**: Read the packet structure definition and list each field with:
   - Name, type, size, read order
   - Whether it's optional/conditional
4. **Parse the data** (if hex/binary provided): Decode byte-by-byte using the structure definition
5. **Cross-reference**: Compare parsed data against what the handler expects

### Key directories
- Handlers: `src/server/game/Handlers/`
- Packet defs: `src/server/game/Server/Packets/`
- Opcode enums: `src/server/game/Server/Protocol/Opcodes.h`

### Transmog-specific packets (high priority)
- `TransmogrificationPackets.h` / `.cpp` — outfit CRUD structures
- `TransmogrificationHandler.cpp` — server-side handling
- Known opcodes:
  - `CMSG_TRANSMOG_OUTFIT_NEW` — create outfit
  - `CMSG_TRANSMOG_OUTFIT_DELETE` — delete outfit
  - `CMSG_TRANSMOG_OUTFIT_RENAME` — rename outfit
  - `CMSG_TRANSMOG_OUTFIT_UPDATE` — modify existing outfit
  - `CMSG_TRANSMOGRIFY_ITEMS` — apply transmog to equipped items

### Output format

```
## Opcode: CMSG_EXAMPLE (0x1234)

### Handler
- File: src/server/game/Handlers/FooHandler.cpp:123
- Function: WorldSession::HandleExample(WorldPackets::Foo::Example& packet)

### Packet Structure (WorldPackets::Foo::Example)
| # | Field          | Type     | Size  | Notes              |
|---|----------------|----------|-------|--------------------|
| 1 | ExampleId      | uint32   | 4     |                    |
| 2 | Name           | string   | var   | Length-prefixed     |
| ...                                                        |

### Parsed Data (if hex provided)
| Field     | Raw Bytes  | Decoded Value |
|-----------|------------|---------------|
| ...       | ...        | ...           |
```
