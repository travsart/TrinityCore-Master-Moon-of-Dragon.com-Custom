# Task Completion Workflow

## When a Task is Completed

### 1. Code Quality Checks
Before considering a task complete, ensure:
- [ ] No compilation errors or warnings
- [ ] Full implementation (no TODOs, no stubs, no placeholders)
- [ ] Comprehensive error handling
- [ ] Performance within targets (<0.1% CPU per bot, <10MB memory)
- [ ] Thread-safe operations (if applicable)
- [ ] TrinityCore API compliance

### 2. Build Verification
```powershell
# Build the affected components
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -maxcpucount:2 `
  "src\modules\Playerbot\playerbot.vcxproj"

# If core integration, build worldserver
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -maxcpucount:2 `
  "src\server\worldserver\worldserver.vcxproj"
```

### 3. Testing
```bash
# Run unit tests (if available)
cmake --build . --target playerbot_tests
./playerbot_tests

# Windows batch
src\modules\Playerbot\Tests\RUN_BEHAVIORMANAGER_TESTS.bat

# Integration tests (manual)
# - Start worldserver
# - Test bot spawning
# - Verify bot behavior
# - Check performance metrics
```

### 4. Code Review Checklist
- [ ] Follows file modification hierarchy (module-first approach)
- [ ] No shortcuts or simplified implementations
- [ ] Uses appropriate design patterns (Manager, Strategy, Hook)
- [ ] Smart pointer usage (`std::unique_ptr`, `std::shared_ptr`)
- [ ] Proper memory management (no leaks)
- [ ] Code style compliance (4 spaces, 160 char lines)
- [ ] Documentation added/updated

### 5. Integration Points Documentation
If the task involved core integration:
- Document all core file modifications
- Justify why module-only approach wasn't possible
- List all hook points added
- Verify backward compatibility
- Document TrinityCore API usage

### 6. Git Commit
```bash
# Stage changes
git add .

# Commit with descriptive message
git commit -m "[PlayerBot] PHASE X COMPLETE: Feature Description

- Bullet point list of changes
- Performance metrics if applicable
- Integration points documented
- Test results summary

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>"

# Push to remote
git push
```

### 7. Performance Validation
For performance-critical changes:
- Profile CPU usage per bot
- Measure memory consumption
- Check database query performance
- Validate against targets:
  - <0.1% CPU per bot
  - <10MB memory per bot
  - 100-500 concurrent bots supported

### 8. Documentation Updates
Update relevant documentation:
- API reference if public interfaces changed
- Developer guide for new patterns
- User guide for new features
- Architecture docs for system changes

## Quality Gates (Must Pass)

### Compilation
- ✅ Zero errors
- ✅ Zero warnings (treat warnings as errors)
- ✅ All targets build successfully

### Testing
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ No regressions in existing functionality

### Performance
- ✅ Within CPU budget (<0.1% per bot)
- ✅ Within memory budget (<10MB per bot)
- ✅ No memory leaks detected
- ✅ Thread-safe operations verified

### Code Quality
- ✅ No TODOs or placeholder code
- ✅ Comprehensive error handling
- ✅ Follows coding standards
- ✅ Appropriate comments and documentation

## Prohibited Actions
❌ Never skip quality checks "to save time"
❌ Never commit with compilation errors
❌ Never bypass TrinityCore APIs without documentation
❌ Never modify core files without justification
❌ Never break backward compatibility
❌ Never commit performance regressions