# TrinityCore PlayerBot Project Overview

## Project Purpose
Transform TrinityCore into a single-player capable MMORPG by integrating AI-controlled player bots, enabling children and offline players to experience World of Warcraft without internet connectivity or other human players.

## What is TrinityCore?
TrinityCore is an open-source MMORPG server framework written in C++20, forked from MaNGOS. It emulates World of Warcraft servers with:
- **bnetserver**: Battle.net authentication server for login and account management
- **worldserver**: Main game world server managing gameplay, NPCs, quests, and instances
- **Database layer**: MySQL 9.4 with auth, characters, and world databases
- **Scripting system**: Dynamic content loading for game mechanics and events

## Technical Specifications
- **Language**: C++20
- **Build System**: CMake 3.24+
- **Database**: MySQL 9.4
- **Key Dependencies**: Boost 1.74+, Intel TBB, parallel-hashmap
- **Platform Support**: Windows, Linux, macOS (currently developed on Windows)
- **Performance Target**: 100-500 concurrent bots with <10% server performance impact
- **Development Timeline**: 7-10 months estimated completion
- **Module Location**: src/modules/Playerbot/

## Codebase Structure
- **src/modules/Playerbot/**: All playerbot code (module-first approach)
- **src/server/**: TrinityCore server components
- **src/common/**: Shared utilities
- **cmake/**: Build system configuration
- **sql/**: Database schemas and migrations
- **tests/**: Unit and integration tests
- **docs/**: Documentation files

## Development Phases
1. **Phase 1**: Core Bot Framework (6-8 weeks) - Bot account/session management
2. **Phase 2**: Foundation Infrastructure (4-6 weeks) - Lifecycle, performance optimization
3. **Phase 3**: Game System Integration (8-10 weeks) - Combat, movement, quests
4. **Phase 4**: Advanced Features (6-8 weeks) - Groups, economy, social
5. **Phase 5**: Performance Optimization (4-6 weeks) - Multi-threading, query optimization
6. **Phase 6**: Integration and Polish (3-4 weeks) - Final testing and deployment

## Current Status
The project is in active development with extensive combat AI, lifecycle management, and game system integration already implemented. The codebase contains approximately 1000+ files across multiple subsystems.