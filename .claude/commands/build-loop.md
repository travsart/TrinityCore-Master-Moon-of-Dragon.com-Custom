---
allowed-tools: Bash(powershell*), Bash(ninja:*), Bash(cd:*), Read, Edit, Grep, Glob
description: Iteratively build the project and fix compilation errors until the build succeeds
paths: src/**/*.cpp, src/**/*.h, src/**/*.hpp, CMakeLists.txt, CMakePresets.json, _build_ps.ps1
---

# Build-Fix Loop

## Instructions

Run an iterative build-fix cycle on the VoxCore project.

### Build preset selection
Parse the user's argument (if any) to pick the preset:
- `debug`, `d`, or no argument → `debug`
- `rel`, `r`, `relwithdebinfo` → `rel`
- `scripts`, `s` → `debug` with target `scripts`

### How to build

**ALWAYS use the PowerShell build script.** Never use cmd.exe batch files from bash — they fail silently.

```bash
powershell.exe -ExecutionPolicy Bypass -File "C:\\Users\\atayl\\VoxCore\\_build_ps.ps1" [preset] [target] 2>&1
```

Examples:
- Full debug build: `powershell.exe -ExecutionPolicy Bypass -File "C:\\Users\\atayl\\VoxCore\\_build_ps.ps1" debug 2>&1`
- Scripts only: `powershell.exe -ExecutionPolicy Bypass -File "C:\\Users\\atayl\\VoxCore\\_build_ps.ps1" debug scripts 2>&1`
- RelWithDebInfo: `powershell.exe -ExecutionPolicy Bypass -File "C:\\Users\\atayl\\VoxCore\\_build_ps.ps1" rel 2>&1`
- Configure only: `powershell.exe -ExecutionPolicy Bypass -File "C:\\Users\\atayl\\VoxCore\\_build_ps.ps1" debug configure 2>&1`

The script handles MSVC environment setup (vcvarsall.bat), CMake configuration (auto-detects if needed), and the ninja build. It outputs `BUILD_SUCCESS` or `BUILD_FAILED`.

### Loop procedure

1. **Build**: Run the PowerShell build script with the selected preset
2. **Parse**: Extract compiler errors from the output. Ignore warnings unless the user asked to fix them.
3. **Fix**: For each error:
   - Read the source file at the reported line
   - Understand the error in context (check headers, related code)
   - Apply the minimal fix using Edit
4. **Rebuild**: Run the build again
5. **Repeat**: Continue until the build succeeds or you've hit 5 consecutive iterations without progress on the error count
6. **Report**: Summarize all changes made, errors fixed, and any remaining issues

### Rules
- Only fix actual compilation errors — not warnings, not style issues
- If an error is ambiguous or requires an architectural decision, STOP and ask the user
- Never modify files outside `src/` unless explicitly told to
- If the exact same error persists after 2 fix attempts, stop and report it — don't keep guessing
- Group related errors (e.g., all caused by the same missing include) and fix them together
- After a successful build, do a final `git diff --stat` to show what changed
