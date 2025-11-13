# TrinityCore MCP Server Project

## Project Location
- **Repository**: https://github.com/agatho/trinitycore-mcp
- **Local Path**: C:\TrinityBots\trinitycore-mcp
- **Working Directory**: TrinityCore is at C:\TrinityBots\TrinityCore

## Project Overview
TypeScript-based Model Context Protocol (MCP) server providing 21 enterprise-grade tools for TrinityCore bot development with World of Warcraft 11.2 (The War Within).

## Current Status (as of 2025-10-29)
- **Version**: v1.2.2
- **Status**: ✅ Production Ready
- **Last Commit**: 84a16d5
- **Branch**: master

## Recent Critical Fix (v1.2.2)
**Issue**: MCP server was not appearing in Claude Code's server list despite full implementation.

**Root Cause**:
1. Missing `bin` entry in package.json (CRITICAL)
2. Path documentation inconsistencies (trinity-mcp-server vs trinitycore-mcp)
3. TypeScript compilation errors in questchain.ts

**Solution**:
- Added `"bin": { "trinitycore-mcp": "./dist/index.js" }` to package.json
- Fixed all path references throughout documentation
- Fixed TypeScript type assertions in questchain.ts
- Created comprehensive MCP_CONFIGURATION.md guide (200+ lines)

## Key Configuration Requirement
MCP servers MUST have a `bin` entry in package.json for Claude Code to discover them:

```json
{
  "bin": {
    "trinitycore-mcp": "./dist/index.js"
  }
}
```

## Claude Code Configuration
Located in `.claude/mcp-servers-config.json`:

```json
{
  "trinitycore-mcp": {
    "command": "node",
    "args": ["C:\\TrinityBots\\trinitycore-mcp\\dist\\index.js"],
    "env": {
      "TRINITY_DB_HOST": "localhost",
      "TRINITY_DB_USER": "trinity",
      "TRINITY_DB_PASSWORD": "..."
    }
  }
}
```

**IMPORTANT**: Always use double backslashes (`\\`) in Windows paths in JSON configuration.

## Project Structure
```
trinitycore-mcp/
├── src/
│   ├── index.ts              # Main MCP server entry (has shebang)
│   ├── tools/                # 21 MCP tool implementations
│   │   ├── spell.ts          # Spell data queries
│   │   ├── item.ts           # Item data queries
│   │   ├── quest.ts          # Quest data queries
│   │   ├── talent.ts         # Talent optimization
│   │   ├── combatmechanics.ts # Combat calculations
│   │   ├── pvptactician.ts   # PvP strategies
│   │   ├── questroute.ts     # Quest routing
│   │   └── ... (13+ more)
│   └── database/
├── dist/                     # Compiled JavaScript (generated)
├── package.json              # Has bin entry
├── tsconfig.json
├── README.md
├── MCP_CONFIGURATION.md      # Comprehensive setup guide
└── MCP_SERVER_FIX_COMPLETE.md # Fix documentation
```

## Available Tools (21 Total)

### Phase 1: Foundation (6 tools)
1. get-spell-info - Query spell data from database
2. get-item-info - Query item data
3. get-quest-info - Query quest information
4. query-dbc - Query DBC/DB2 client files
5. get-trinity-api - Get TrinityCore C++ API documentation
6. get-opcode-info - Network packet opcode info

### Phase 2: Core Systems (7 tools)
7. get-talent-build - Recommended talent builds
8. calculate-melee-damage - Damage calculations
9. get-buff-recommendations - Buff optimization
10. get-boss-mechanics - Dungeon/raid strategies
11. get-item-pricing - Economy/auction house
12. get-reputation-standing - Reputation calculations
13. coordinate-cooldowns - Multi-bot coordination

### Phase 3: Advanced (8 tools)
14. analyze-arena-composition - PvP team analysis
15. get-battleground-strategy - BG tactics
16. get-pvp-talent-build - PvP talent optimization
17. optimize-quest-route - Quest route optimization
18. get-leveling-path - Multi-zone leveling paths
19. get-collection-status - Collection tracking
20. find-missing-collectibles - Pet/mount/toy finder
21. get-farming-route - Farming optimization

## Implementation Quality
- **All Phase 1 + Phase 2 complete**: Enterprise-grade implementations
- **No shortcuts**: Full implementations with comprehensive error handling
- **WoW 11.2 accurate**: The War Within expansion mechanics
- **Database-driven**: Queries TrinityCore world database
- **Type-safe**: TypeScript strict mode compliant

## Recent Enhancements (v1.1.0 - v1.2.1)

### Phase 1 (v1.1.0)
- Spell attribute flag parsing
- Quest reward best choice logic
- Combat mechanics diminishing returns
- Economy market value estimation
- Gear optimizer stat weights
- Talent system build database

### Phase 2 (v1.2.0)
- Spell range DBC lookup (68 entries)
- Quest routing XP calculations (level 1-80)
- Reputation spell effect parsing
- Group coordination formulas (DPS/HPS/Threat)
- Economy trend analysis (time-series)
- Combat spirit/mana regeneration
- Talent comparison with synergies
- PvP counter logic (8 compositions)

### Phase 2.1 (v1.2.1)
- Fixed TypeScript compilation errors in pvptactician.ts
- Fixed TypeScript compilation errors in talent.ts
- Resolved null safety issues

### Critical Fix (v1.2.2)
- Added bin entry to package.json
- Fixed path documentation inconsistencies
- Fixed questchain.ts type errors
- Created MCP_CONFIGURATION.md

## Build Process
```bash
cd C:\TrinityBots\trinitycore-mcp
npm install
npm run build  # Compiles TypeScript to dist/
```

## Common Issues & Solutions

### Issue: MCP server not appearing in Claude Code
**Solution**: Ensure package.json has bin entry and rebuild:
```bash
npm run build
```

### Issue: TypeScript compilation errors
**Solution**: Check for type assertions, especially with Object.entries():
```typescript
// Use type assertions
const value = (unknownValue as number) * factor;
```

### Issue: Path errors in Windows
**Solution**: Always use double backslashes in JSON configs:
```json
"C:\\TrinityBots\\trinitycore-mcp\\dist\\index.js"
```

## Dependencies
- @modelcontextprotocol/sdk: ^1.0.0
- mysql2: ^3.6.5
- dotenv: ^16.3.1
- TypeScript: ^5.3.3
- Node.js: >=18.0.0

## Database Requirements
- TrinityCore world database must be populated
- Read-only SELECT permissions required
- Default connection: localhost:3306

## Documentation Files
1. **README.md** - Project overview, quick start
2. **MCP_CONFIGURATION.md** - Comprehensive Claude Code setup guide (200+ lines)
3. **MCP_SERVER_FIX_COMPLETE.md** - v1.2.2 fix documentation
4. **IMPLEMENTATION_COMPLETE_2025-10-29.md** - Phase 1 implementation details
5. **IMPLEMENTATION_COMPLETE_PHASE_2_2025-10-29.md** - Phase 2 implementation details

## Release History
- v1.0.0 - Initial foundation
- v1.1.0 - Phase 1 enhancements (6 implementations)
- v1.2.0 - Phase 2 enhancements (8 implementations)
- v1.2.1 - TypeScript fixes (pvptactician.ts, talent.ts)
- v1.2.2 - **CRITICAL** MCP configuration fix (bin entry added)

## GitHub Issues Closed
- #2 - Spell Range Lookup from DBC
- #3 - Spell Attribute Flag Parsing
- #4 - Quest Reward Best Choice Logic
- #5 - Combat Mechanics Diminishing Returns
- #6 - Economy Market Value Estimation
- #7 - Gear Optimizer Stat Weights
- #8 - Talent System Build Database
- #11 - Release announcement (pinned)

## Next Session Priorities
1. User testing of v1.2.2 MCP configuration
2. Feedback collection on Claude Code integration
3. Potential npm registry publication
4. Performance monitoring and optimization
