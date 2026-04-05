---
description: "deep link system — URL-based session launching, claude:// protocol handler, task handoff via URLs, external trigger integration"
---

# Deep Link System -- Arcanum Wiki

## What Is This?

The Deep Link system enables external applications (browsers, other tools) to launch Claude Code sessions via `claude-cli://` URI scheme links. Clicking a link like `claude-cli://open?q=fix+tests&repo=owner/repo` will open a new terminal window running Claude Code with the specified prompt pre-filled, working directory set, and repository context loaded. The system handles protocol registration across macOS, Linux, and Windows, terminal emulator detection, and extensive security hardening against prompt injection.

## How It Works

### URI Parsing (parseDeepLink.ts)

The `claude-cli://open` scheme supports three parameters:
- `q` -- prompt text to pre-fill (not auto-submitted)
- `cwd` -- absolute path for working directory
- `repo` -- GitHub `owner/name` slug, resolved against known clone paths

**Security hardening is extensive:**

```typescript
const MAX_QUERY_LENGTH = 5000
const MAX_CWD_LENGTH = 4096
```

- **ASCII control character rejection**: Newlines, carriage returns, and other control chars (0x00-0x1F, 0x7F) are blocked in all parameters -- they can act as command separators in shells
- **Unicode sanitization**: Hidden Unicode characters stripped via `partiallySanitizeUnicode()` to prevent ASCII smuggling / hidden prompt injection
- **Path validation**: cwd must be an absolute path; repo must match `^[\w.-]+\/[\w.-]+$`
- **Length limits**: 5000 char query max, 4096 char cwd max -- the 5000 limit accounts for Windows cmd.exe's 8191-char command string limit after quoting
- **No truncation**: Reject, don't truncate -- truncation changes meaning

### Protocol Registration (registerProtocol.ts)

Registration runs automatically on startup via `ensureDeepLinkProtocolRegistered()` with self-healing:

**macOS**: Creates a minimal `.app` bundle at `~/Applications/Claude Code URL Handler.app` with:
- `Info.plist` containing `CFBundleURLTypes` for the `claude-cli` scheme
- A symlink to the `claude` binary (avoids needing separate signing)
- LaunchServices registration via `lsregister -R`

**Linux**: Creates a `.desktop` file at `$XDG_DATA_HOME/applications/claude-code-url-handler.desktop` and registers with `xdg-mime`. Handles headless environments (WSL, Docker, CI) where xdg-utils is not installed.

**Windows**: Writes registry keys under `HKCU\Software\Classes\claude-cli` with reg.exe.

The registration is **artifact-based**: it reads back the actual OS artifacts (symlink target, .desktop content, registry value) to check currency, rather than using a cached flag. This means stale paths self-heal when the install method changes.

**Failure backoff**: If registration fails with EACCES/ENOSPC (deterministic failures), a marker file is written to throttle retries to once per 24 hours.

Gated by: `tengu_lodestone_enabled` GrowthBook flag and `disableDeepLinkRegistration` setting.

### Terminal Launcher (terminalLauncher.ts)

When launched from the OS (headless context, no TTY), the handler detects the user's terminal and opens Claude Code inside it. Terminal detection is thorough:

**macOS** (6 terminals): iTerm2, Ghostty, Kitty, Alacritty, WezTerm, Terminal.app
**Linux** (10 terminals): ghostty, kitty, alacritty, wezterm, gnome-terminal, konsole, xfce4-terminal, mate-terminal, tilix, xterm
**Windows** (3 terminals): Windows Terminal, PowerShell 7+/5.1, cmd.exe

Two launch paths exist:

1. **Pure argv** (no shell injection risk): Ghostty, Alacritty, Kitty, WezTerm (macOS), all Linux terminals, Windows Terminal. User input travels as distinct argv elements end-to-end.

2. **Shell-string** (user input is shell-quoted): iTerm2 and Terminal.app (AppleScript `write text` has no argv interface), PowerShell `-Command`, cmd.exe `/k`.

For shell-string paths, three separate quoting functions handle injection:
- `shellQuote()` -- POSIX single-quote escaping for AppleScript paths
- `psQuote()` -- PowerShell single-quoted strings (only `''` for literal quote)
- `cmdQuote()` -- cmd.exe quoting that strips `"` entirely (cannot be safely represented), escapes `%` as `%%`, and doubles trailing backslashes

### Terminal Preference Capture (terminalPreference.ts)

On interactive startup, the current terminal is saved from `TERM_PROGRAM` env var to `config.deepLinkTerminal`. This is the only signal that survives into the headless LaunchServices context -- when macOS opens Claude from a browser link, TERM_PROGRAM is not set.

### Banner (banner.ts)

A warning banner appears when a session was opened by an external deep link:

```
This session was opened by an external deep link in ~/project
The prompt below (1,234 chars) was supplied by the link — review carefully before pressing Enter.
```

For long prompts (>1000 chars), the warning escalates to "scroll to review the entire prompt" since a malicious tail could be hidden past the visible screen.

Repo staleness is checked by reading `.git/FETCH_HEAD` mtime -- if over 7 days old, a warning about stale CLAUDE.md is shown.

### URL Scheme Launch (protocolHandler.ts)

On macOS, `handleUrlSchemeLaunch()` detects if Claude was launched by LaunchServices (vs from a terminal) by checking `__CFBundleIdentifier`. If it matches the bundle ID, it uses the `url-handler-napi` NAPI module to receive the URL from the Apple Event with a 5-second timeout.

## Feature Gating

| Gate | Type | Notes |
|------|------|-------|
| `tengu_lodestone_enabled` | GrowthBook flag | Master switch for auto-registration |
| `disableDeepLinkRegistration` | Settings | User opt-out |

## User-Facing Behavior

1. After first launch, the protocol handler is auto-registered with the OS
2. Clicking `claude-cli://open?q=hello` in any browser opens Claude Code in a new terminal
3. A security banner warns the user about the external origin
4. The prompt is pre-filled but NOT auto-submitted -- user must press Enter
5. Working directory can be set via cwd or resolved from repo slug via known clone paths

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/deepLink/parseDeepLink.ts` | URI parser with security validation |
| `src/utils/deepLink/registerProtocol.ts` | OS protocol handler registration (macOS/Linux/Windows) |
| `src/utils/deepLink/protocolHandler.ts` | Entry point for `--handle-uri`, Apple Event handling |
| `src/utils/deepLink/terminalLauncher.ts` | Terminal detection and cross-platform launch (558 lines) |
| `src/utils/deepLink/terminalPreference.ts` | TERM_PROGRAM capture for headless context |
| `src/utils/deepLink/banner.ts` | Security warning banner and FETCH_HEAD staleness |

## Configuration

- `config.deepLinkTerminal` -- saved terminal preference for macOS
- `disableDeepLinkRegistration` -- disable auto-registration
- `githubRepoPaths` -- MRU clone paths for repo slug resolution

## Interesting Findings

1. **The cmd.exe quoting** is the most paranoid: embedded `"` is STRIPPED entirely because cmd.exe toggles quoting state on every raw `"` and there is no safe escape sequence. The comment explains this is because cmd.exe does NOT use CommandLineToArgvW-style backslash escaping.

2. **Three separate injection boundaries** exist: POSIX shell for AppleScript, PowerShell single quotes, and cmd.exe quoting. Each has different escape rules and different failure modes. The code documents which paths are "pure argv" (safe) vs "shell-string" (injection-possible).

3. **The long prompt warning threshold** (1000 chars) is calibrated to ~12-15 lines on an 80-column terminal. The defense model is that a user can't scan a 5000-char prompt at a glance -- they need an explicit nudge to scroll.

4. **FETCH_HEAD staleness detection** checks both the worktree's FETCH_HEAD and the main repo's FETCH_HEAD (via commonDir), returning whichever is newer. This handles the case where a deep link lands in a git worktree that was never directly fetched.

5. **The Apple Event timeout** is 5 seconds -- if the NAPI module doesn't receive the URL within that window, it gives up. This is generous enough for LaunchServices but prevents hanging forever if the launch wasn't URL-triggered.
