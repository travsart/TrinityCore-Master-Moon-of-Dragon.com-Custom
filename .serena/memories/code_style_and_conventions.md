# Code Style and Conventions

## EditorConfig Settings
- **Charset**: UTF-8 (latin1 for C/C++ files)
- **Indent Style**: Spaces
- **Indent Size**: 4 spaces
- **Tab Width**: 4
- **Max Line Length**: 160 characters
- **Final Newline**: Required
- **Trim Trailing Whitespace**: Yes

## C++ Coding Standards
- **Standard**: C++20 features encouraged
- **Extensions**: Disabled (CXX_EXTENSIONS OFF)
- **Naming Conventions**:
  - Classes: PascalCase (e.g., `BotSessionMgr`)
  - Functions: PascalCase for public methods
  - Variables: camelCase with m_ prefix for members (e.g., `m_customManager`)
  - Constants: UPPER_SNAKE_CASE

## Design Patterns
### Mandatory Patterns
1. **Module-First**: All code in `src/modules/Playerbot/` when possible
2. **Hook Pattern**: For core integration (observer/hook pattern only)
3. **Smart Pointers**: Use `std::unique_ptr` and `std::shared_ptr`
   - Example: `std::make_unique<WarriorAI>(bot)`
4. **Manager Pattern**: BehaviorManager base class for coordinated systems
5. **Strategy Pattern**: For AI behaviors and decisions

### Forbidden Patterns
- Direct core file refactoring
- Security system modifications
- Network layer changes
- Bypassing TrinityCore APIs without justification

## Architecture Principles
1. **Zero Shortcuts**: Full implementation, no stubs, no TODOs
2. **Error Handling**: Comprehensive error handling required
3. **Performance First**: <0.1% CPU per bot, <10MB memory per bot
4. **TrinityCore API Usage**: Never bypass existing systems
5. **Backward Compatibility**: Core changes must not break existing functionality

## Memory Management
- Prefer stack allocation when possible
- Use smart pointers for heap allocation
- Object pooling for frequently allocated objects
- No raw `new`/`delete` without justification

## Threading
- Thread-safe operations required
- Use Intel TBB for parallel algorithms
- Minimize lock contention
- Prefer lock-free data structures (parallel-hashmap)