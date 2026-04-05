---
description: "swarm backends — tmux iTerm2 in-process, multi-agent execution environments, terminal multiplexing, process isolation, backend selection logic"
---

# Swarm Backends -- Arcanum Wiki

## Overview

The swarm system abstracts three execution backends behind the `TeammateExecutor` interface: tmux (external terminal sessions), iTerm2 (native split panes), and in-process (same Node.js process with `AsyncLocalStorage` isolation). Backend selection follows a strict priority chain, and the choice is frozen at session startup.

## How It Works

### Backend Detection Priority

`detectAndGetBackend()` in `registry.ts` implements this chain:

```
1. Inside tmux?              --> TmuxBackend (native, even if in iTerm2)
2. In iTerm2 + it2 CLI?      --> ITermBackend (native)
3. In iTerm2, no it2?        --> TmuxBackend (fallback) + needsIt2Setup flag
4. Tmux available?           --> TmuxBackend (external session)
5. None available?           --> Error with platform-specific install instructions
```

In-process mode activates when: the session is non-interactive (`-p` flag), the setting is explicitly `in-process`, auto mode detects no pane backend, or a prior spawn fell back to in-process.

### TmuxBackend

Operates in two modes:

**Inside tmux**: Splits the leader's window, giving the leader 30% width and teammates the remaining 70%. Uses `main-vertical` layout. First teammate gets a horizontal split; subsequent teammates alternate vertical/horizontal.

**Outside tmux**: Creates a dedicated `claude-swarm` session on a per-PID socket (`claude-swarm-${process.pid}`). All teammate panes are equally distributed in `tiled` layout.

A promise-based lock (`paneCreationLock`) prevents race conditions when spawning teammates in parallel. A 200ms delay after pane creation allows shell initialization.

Tmux panes support hide/show: `hidePane()` uses `break-pane` to move a pane to a `claude-hidden` detached session; `showPane()` uses `join-pane` to bring it back.

### ITermBackend

Uses the `it2` CLI tool (Python-based) to manage iTerm2 native panes. First teammate gets a vertical split from the leader's session; subsequent teammates stack horizontally from the last teammate's session.

Limitations: `setPaneBorderColor()`, `setPaneTitle()`, `enablePaneBorderStatus()`, and `rebalancePanes()` are all no-ops because each `it2` call spawns a Python process, making them too slow for cosmetic features. `supportsHideShow` is `false` (no equivalent to tmux break-pane).

### InProcessBackend

Runs teammates in the same Node.js process using `AsyncLocalStorage` for context isolation. Always returns `isAvailable() === true`.

Spawn flow:
1. Create identity, AbortController, and task state
2. Launch agent execution loop fire-and-forget
3. Leader's `ToolUseContext` passed with messages stripped

The in-process runner wraps `runAgent()` inside dual context isolation:
```typescript
await runWithTeammateContext(teammateContext, async () => {
    return runWithAgentContext(agentContext, async () => {
        // runAgent() call
    })
})
```

Auto-compaction runs within in-process teammates independently. The main while loop: build prompt -> try claim task -> run agent -> send idle notification -> wait for next prompt (poll 500ms) -> handle result.

### Environment Inheritance

Pane-based teammates inherit CLI flags and environment variables:

**CLI flags**: Permission mode (but NOT if plan mode required), --model, --settings, --plugin-dir, --teammate-mode, --chrome/--no-chrome.

**Env vars**: Provider configs (Bedrock/Vertex/Foundry), proxy settings (HTTP_PROXY, HTTPS_PROXY, SSL_CERT_FILE, etc.), config dirs. Always includes `CLAUDECODE=1` and `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1`.

**Model fallback**: When no model is specified, teammates default to Opus 4.6 (provider-aware via `getHardcodedTeammateModelFallback()`).

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/swarm/backends/TmuxBackend.ts` | Tmux pane management |
| `src/utils/swarm/backends/ITermBackend.ts` | iTerm2 native panes |
| `src/utils/swarm/backends/InProcessBackend.ts` | Same-process isolation |
| `src/utils/swarm/backends/registry.ts` | Detection and selection |
| `src/utils/swarm/backends/teammateModeSnapshot.ts` | Mode freeze at startup |
| `src/utils/swarm/spawnUtils.ts` | CLI flag and env var inheritance |
| `src/utils/swarm/inProcessRunner.ts` | In-process agent execution loop |

## Cross-References

- [Swarm Overview](swarm_overview.md) -- High-level architecture
- [Swarm Messaging](swarm_messaging.md) -- How backends communicate

## Interesting Findings

**Plan mode overrides bypass permissions for safety.** When `planModeRequired` is true, `--dangerously-skip-permissions` is never propagated to pane-based teammates, even if the leader has it set.

**In-process kill uses dual abort controllers.** `abortController` kills the WHOLE teammate. `currentWorkAbortController` only aborts the current turn (allowing the teammate to survive and pick up new work). The user pressing Escape triggers the per-turn abort, not the per-teammate one.
