---
allowed-tools: Bash, Read
description: Run WowPacketParser on the server's packet log to produce human-readable text and SQL output
paths: out/**/PacketLog/*.pkt, out/**/PacketLog/**/*.pkt
---

# Decode Packet Log

## Arguments

$ARGUMENTS — Optional: path to a specific .pkt file. Defaults to the server's PacketLog/World.pkt.

## Instructions

Decode a `.pkt` packet capture file using WowPacketParser (WPP).

### Setup

- **WPP executable**: `C:/Users/atayl/VoxCore/ExtTools/WowPacketParser/WowPacketParser.exe`
- **Default .pkt location**: `C:/Users/atayl/VoxCore/out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/PacketLog/World.pkt`
- **Config**: `C:/Users/atayl/VoxCore/ExtTools/WowPacketParser/WowPacketParser.dll.config` (TargetedDatabase=10 Midnight, all SQL outputs enabled, SplitSQLFile=true)

### Procedure

1. **Determine input file**: Use $ARGUMENTS if provided, otherwise use the default PacketLog/World.pkt path
2. **Verify the .pkt file exists** and is non-empty. If it doesn't exist or is 0 bytes, tell the user
3. **Check for file locks**: The worldserver holds an exclusive lock on World.pkt while running. If WPP fails with "being used by another process", tell the user to stop the worldserver first (or offer to copy the file to a temp location)
4. **Remove stale output files**: Delete any existing `*_parsed.txt`, `*_errors.txt`, and `*.sql` files in the same directory as the input .pkt to avoid "file in use" errors
5. **Run WPP**:
   ```
   cd "C:/Users/atayl/VoxCore/ExtTools/WowPacketParser" && ./WowPacketParser.exe "<path_to_pkt>" 2>&1
   ```
   Use a 5-minute timeout (300000ms) — large sniffs take time
6. **List output files**: Show all generated files with sizes:
   - `*_parsed.txt` — human-readable packet decode (main output)
   - `*_errors.txt` — packets that failed to parse (if any)
   - `*.sql` / `*_world.sql` / `*_hotfixes.sql` / `*_WPP.sql` — extracted SQL data (if SplitSQLFile=true)
7. **Summarize**: Report total packets parsed, any errors, and what SQL tables had data

### Troubleshooting

- **"being used by another process"** on input: Worldserver is running. Stop it or copy .pkt first
- **"Save file ... is in use"** on output: A text editor has the output file open. Close it or delete the file
- **NullReferenceException in LoadBroadcastText**: The `wpp` database may be missing — non-fatal, parsing still works
- **"DBC folder not found"**: DBC/DB2 files not extracted to `C:/Users/atayl/VoxCore/ExtTools/WowPacketParser/dbc/enUS/` — non-fatal, just means no DBC name resolution
- **Empty output**: The sniff may have been captured while idle — need actual gameplay packets
