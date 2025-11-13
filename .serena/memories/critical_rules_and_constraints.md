# Critical Rules and Constraints

## MANDATORY WORKFLOW (NO EXCEPTIONS)

### Phase 1: PLANNING (Required before any code)
1. **Acknowledge Rules**: "I acknowledge the no-shortcuts rule and file modification hierarchy"
2. **File Strategy**: Determine if solution can be module-only or requires core integration
3. **Detail Implementation**: Complete approach with specific TrinityCore APIs
4. **Database Dependencies**: List all database/DBC/DB2 dependencies
5. **Integration Points**: Identify minimal core touchpoints if needed
6. **Wait for Approval**: Explicit GO/NO-GO from developer

### Phase 2: IMPLEMENTATION (Only after approval)
- **Module-First**: Implement in `src/modules/Playerbot/` when possible
- **Minimal Core**: If core modification needed, use hooks/events pattern
- **Complete Solution**: No TODOs, no placeholders
- **Error Handling**: Full error handling and edge cases
- **Performance**: Built-in performance considerations

### Phase 3: VALIDATION (Before delivery)
- **Self-Review**: Against quality requirements
- **Integration Check**: Verify minimal core impact
- **API Compliance**: Confirm TrinityCore API usage
- **Documentation**: Document all integration points

## FILE MODIFICATION HIERARCHY

### 1. PREFERRED: Module-Only Implementation
- **Location**: `src/modules/Playerbot/`
- **Scope**: All new functionality
- **Goal**: Zero core modifications
- **Use When**: Pure bot logic, data structures, algorithms, configuration, AI

### 2. ACCEPTABLE: Minimal Core Hooks/Events
- **Location**: Strategic core integration points
- **Scope**: Observer/hook pattern only
- **Goal**: Minimal, well-defined integration
- **Use When**: Need to observe core events (player login, combat, etc.)

### 3. CAREFULLY: Core Extension Points
- **Location**: Core files with justification
- **Scope**: Extending existing systems
- **Goal**: Maintain core stability
- **Requirements**: Document WHY module-only wasn't possible

### 4. FORBIDDEN: Core Refactoring
- ❌ Wholesale changes to core game logic
- ❌ Security system modifications
- ❌ Network layer changes
- ❌ Breaking existing functionality

## FORBIDDEN ACTIONS (IMMEDIATE STOP)
- ❌ Implementing simplified/stub solutions
- ❌ Using placeholder comments instead of real code
- ❌ Skipping comprehensive error handling
- ❌ Bypassing TrinityCore APIs without justification
- ❌ Wholesale core file refactoring
- ❌ Suggesting "quick fixes" or "temporary solutions"
- ❌ Breaking backward compatibility

## REQUIRED ACTIONS (MANDATORY)
- ✅ Full, complete implementation every time
- ✅ Comprehensive testing approach
- ✅ Performance optimization from start
- ✅ TrinityCore API usage validation
- ✅ Database/DBC/DB2 research before coding
- ✅ Follow file modification hierarchy
- ✅ Document integration points

## QUALITY REQUIREMENTS (FUNDAMENTAL RULES)
- **NEVER take shortcuts** - Full implementation, no simplified approaches, no stubs
- **FOLLOW file modification hierarchy** - Module-first, minimal core integration
- **ALWAYS use TrinityCore APIs** - Never bypass existing systems
- **ALWAYS evaluate DBC/DB2/SQL** - No redundant work, research existing data
- **ALWAYS maintain performance** - <0.1% CPU per bot, <10MB memory per bot
- **ALWAYS test thoroughly** - Unit tests for every component
- **ALWAYS aim for quality and completeness** - Quality first, always
- **ALWAYS document integration** - Every core modification documented
- **ALWAYS maintain backward compatibility** - No breaking changes
- **NO time constraints** - No need to hurry, quality over speed

## STOP CONDITIONS

If ANY of the following are suggested, STOP immediately:

### Shortcut Indicators
- "For now, let's implement a simple..."
- "We can start with a basic version..."
- "Here's a quick solution..."
- "TODO: Implement proper..."
- "This is a simplified approach..."
- "Let's skip [X] for now..."

**Required Response**: "This violates the no-shortcuts rule. Please provide complete implementation following the file modification hierarchy."

### Core Violation Indicators
- "Let's modify the core login system..."
- "We need to change the database schema..."
- "Let's refactor the session management..."

**Required Response**: "Please justify core modification and explore module-only alternatives first."

## CONFIGURATION NOTES
- Git and build timeout: 30 minutes (as specified in c:\TrinityBots\CLAUDE.md)
- All configuration in `playerbots.conf`, not worldserver.conf
- Module compilation optional via `-DBUILD_PLAYERBOT=1`
- TrinityCore must work with or without playerbot