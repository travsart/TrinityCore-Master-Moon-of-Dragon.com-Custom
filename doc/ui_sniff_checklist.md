# Missing Retail UI — Sniff & Fix Checklist

Interactions where the 12.x client should show a UI panel but currently doesn't.
Pattern: sniff retail with Ymir → decode with /decode-pkt → compare opcodes → implement server-side.

## Priority Targets

| # | Interaction | Expected UI | GO/NPC | Status |
|---|------------|-------------|--------|--------|
| 1 | Hero's Call Board | Quest list panel | GO 206111 (type 2) | INVESTIGATED — gossip_menu empty, quest data present. Need retail sniff to confirm opcode. |
| 2 | Warchief's Command Board | Quest list panel (Horde) | GO 206109 (type 2) | Same as above, Horde equivalent |
| 3 | Adventure Board | Zone quest hub | GO 207303/207304 (type 2) | Untested |
| 4 | Adventurer's Taskboard | Task list | GO 278492 (type 10) | Untested |
| 5 | Transmog NPC | Wardrobe window | Various NPCs | Partially working via custom system |
| 6 | Adventure Guide | Dungeon/raid browser | Shift+J keybind | Untested — may be entirely client-side DB2 |
| 7 | Barbershop | Customization UI | Barber NPCs | Untested |
| 8 | Chromie Time | Expansion picker | Chromie NPC | Untested |

## Sniff Workflow

1. Launch Ymir (`ExtTools/ymir_retail_12.0.1.66709/ymir_retail.exe`)
2. Log into retail, go to the interaction
3. Interact — capture the packet exchange
4. `/decode-pkt` to parse the .pkt file
5. Identify the SMSG opcode the server sends
6. Search TrinityCore source for that opcode handler
7. Implement or fix the server response

## Key Findings (Hero's Call Board)

- GO type 2 (QUESTGIVER), gossipID 15807
- gossip_menu and gossip_menu_option for 15807 are EMPTY in DB
- 50+ quests linked via gameobject_queststarter — data is present
- C++ handler calls PrepareGossipMenu → PrepareQuestMenu → SendGossipMenu
- Quest eligibility (PrevQuestID, ContentTuningID, AllowableRaces) may filter all quests out
- OR: 12.x client may use a different opcode entirely (not gossip-based)
- NEXT: Retail packet sniff will confirm which case
