---
description: "developer debug commands — /doctor /debug-tool-call /heapdump /ant-trace /perf-issue /mock-limits /break-cache /reset-limits /version /bridge-kick"
---

# Developer Tools Commands -- Arcanum Wiki

## Overview

These commands are diagnostic and debugging tools primarily used by Anthropic engineers ("ants") and for troubleshooting installations. Most are hidden from regular users or stubbed out entirely. They provide low-level access to internals like heap dumps, performance profiling, and debug tracing.

## Commands

### /doctor
- **Arguments**: None
- **What it does**: Diagnoses and verifies the Claude Code installation and settings. Runs a series of health checks including:
  - Authentication status
  - API connectivity
  - Model availability
  - Tool configuration
  - Settings validity
  - MCP server status

  Renders the `Doctor` screen component which provides a comprehensive diagnostic view.
- **Feature gating**: Can be disabled via `DISABLE_DOCTOR_COMMAND` environment variable.
- **Key code**:
```typescript
const doctor: Command = {
  name: 'doctor',
  description: 'Diagnose and verify your Claude Code installation and settings',
  isEnabled: () => !isEnvTruthy(process.env.DISABLE_DOCTOR_COMMAND),
  type: 'local-jsx',
}
```
- **Notes**: This is the go-to command when something is not working. Available to all users.

---

### /debug-tool-call
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely used for debugging individual tool call execution.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /heapdump
- **Arguments**: None
- **What it does**: Dumps the JavaScript heap to `~/Desktop`. Used for diagnosing memory leaks and excessive memory usage.
- **Feature gating**: Permanently hidden (`isHidden: true`). Available in non-interactive mode.
- **Key code**:
```typescript
const heapDump = {
  type: 'local',
  name: 'heapdump',
  description: 'Dump the JS heap to ~/Desktop',
  isHidden: true,
  supportsNonInteractive: true,
}
```
- **Notes**: This generates a V8 heap snapshot file that can be loaded into Chrome DevTools for analysis.

---

### /ant-trace
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely used for Anthropic-internal tracing of API calls and performance.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /perf-issue
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely a performance issue reporting tool.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /mock-limits
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely used for simulating rate limits and quota exhaustion during development.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /break-cache
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely used for forcing prompt cache invalidation during debugging.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /reset-limits
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Exports both `resetLimits` and `resetLimitsNonInteractive` but both are stubs.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /version
- **Arguments**: None
- **What it does**: Prints the exact version this session is running, including build time. Useful for debugging since auto-update may have downloaded a newer version that has not yet been loaded.
- **Feature gating**: Only enabled for Anthropic employees (`USER_TYPE === 'ant'`). Available in non-interactive mode.
- **Key code**:
```typescript
const call: LocalCommandCall = async () => {
  return {
    type: 'text',
    value: MACRO.BUILD_TIME
      ? `${MACRO.VERSION} (built ${MACRO.BUILD_TIME})`
      : MACRO.VERSION,
  }
}

const version = {
  type: 'local',
  name: 'version',
  description: 'Print the version this session is running ' +
    '(not what autoupdate downloaded)',
  isEnabled: () => process.env.USER_TYPE === 'ant',
}
```
- **Notes**: Uses compile-time macros (`MACRO.VERSION`, `MACRO.BUILD_TIME`) injected during the build process.

---

### /bridge-kick
- **Arguments**: `<subcommand>` (many subcommands)
- **What it does**: Ant-only debugging command for injecting failure states into the bridge (remote control) connection to test recovery paths. Subcommands include:
  - `close 1002` / `close 1006` -- Simulate WebSocket close events
  - `poll 404` / `poll 401` / `poll transient` -- Simulate polling failures
  - `register fail [N]` -- Make next N registration attempts fail
  - `register fatal` -- Simulate fatal 403 on registration
  - `reconnect-session fail` -- Simulate reconnection failure
  - `heartbeat 401` -- Simulate expired JWT
  - `reconnect` -- Trigger manual reconnection
  - `status` -- Print current bridge state
- **Feature gating**: Not explicitly gated in the source but requires bridge mode to be meaningful.
- **Notes**: Designed for composite failure testing -- queue multiple faults then fire a trigger to test recovery chains. Extremely useful for debugging the remote control reconnection logic.

## Hidden/Undocumented Commands

Nearly every command in this group is hidden:
- **/debug-tool-call** -- Stubbed out
- **/heapdump** -- Hidden, functional
- **/ant-trace** -- Stubbed out
- **/perf-issue** -- Stubbed out
- **/mock-limits** -- Stubbed out
- **/break-cache** -- Stubbed out
- **/reset-limits** -- Stubbed out
- **/version** -- Ant-only
- **/bridge-kick** -- Ant-only debugging tool, not registered as a standard command

Only `/doctor` is universally visible in this group.
