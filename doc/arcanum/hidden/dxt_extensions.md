---
description: "DXT extension system — developer extensions, extension manifest format, tool augmentation, third-party capability injection"
---

# DXT (Extension System) -- Arcanum Wiki

## What Is This?

DXT is Claude Code's extension packaging format -- the `.dxt` file format for distributing MCP servers and plugins. The `src/utils/dxt/` module handles manifest validation, zip extraction with security hardening, and extension ID generation. DXT files are essentially zip archives containing an MCP server bundle with a `manifest.json` descriptor validated against the `@anthropic-ai/mcpb` schema.

## How It Works

### Manifest Validation (helpers.ts)

DXT manifests are validated against `McpbManifestSchema` from the `@anthropic-ai/mcpb` package. The import is lazy to avoid allocating ~700KB of bound closures from zod v3 at startup:

```typescript
export async function validateManifest(manifestJson: unknown): Promise<McpbManifest> {
  const { McpbManifestSchema } = await import('@anthropic-ai/mcpb')
  const parseResult = McpbManifestSchema.safeParse(manifestJson)
  // ...
}
```

Three parsing entry points:
- `validateManifest(json)` -- from parsed JSON object
- `parseAndValidateManifestFromText(string)` -- from JSON string
- `parseAndValidateManifestFromBytes(Uint8Array)` -- from raw binary data

**Extension ID generation** produces a consistent identifier from `author.name` and `manifest.name`, sanitized to lowercase alphanumerics/hyphens/dots/underscores:

```typescript
export function generateExtensionId(manifest: McpbManifest, prefix?: 'local.unpacked' | 'local.dxt'): string {
  // "john-doe.my-extension" or "local.dxt.john-doe.my-extension"
}
```

### Zip Extraction with Security (zip.ts)

DXT files are zip archives extracted with extensive security validation:

**Limits:**
| Limit | Value | Purpose |
|-------|-------|---------|
| MAX_FILE_SIZE | 512MB | Per-file limit |
| MAX_TOTAL_SIZE | 1GB | Total uncompressed limit |
| MAX_FILE_COUNT | 100,000 | File count limit |
| MAX_COMPRESSION_RATIO | 50:1 | Zip bomb detection |
| MIN_COMPRESSION_RATIO | 0.5:1 | Suspicious pre-compressed content |

**Path traversal protection**: `isPathSafe()` rejects absolute paths and `../` traversal.

**Zip bomb detection**: Tracks running compression ratio during extraction; anything above 50:1 is flagged as suspicious.

**Unix file mode preservation**: `parseZipModes()` manually parses the zip central directory to extract Unix file permissions (st_mode from `externalAttr`). This is needed because `fflate`'s `unzipSync` does not surface file attributes -- everything would become 0644, losing executable bits. Only applies to entries created on Unix hosts (`versionMadeBy` high byte === 3).

```typescript
export function parseZipModes(data: Uint8Array): Record<string, number> {
  // Manual PKZIP APPNOTE.TXT §4.3.12 parsing
  // Returns name → mode for Unix-origin entries
}
```

The fflate library is also lazy-imported to avoid its ~196KB of top-level lookup tables being allocated at startup.

## Feature Gating

No feature flags -- DXT is part of the plugin/extension system that is generally available.

## User-Facing Behavior

Users interact with DXT through:
- Installing extensions from the marketplace
- Installing local `.dxt` files
- Installing unpacked extensions from directories
- The extension ID appears in configuration and plugin management

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/dxt/helpers.ts` | Manifest validation, extension ID generation |
| `src/utils/dxt/zip.ts` | Secure zip extraction, zip bomb detection, mode parsing |

## Configuration

No user-configurable settings for the DXT format itself.

## Interesting Findings

1. **The zip bomb detection** is a real security concern for an extension system -- a malicious .dxt file with a 50:1 compression ratio could exhaust disk space or memory during extraction.

2. **Manual PKZIP central directory parsing** to preserve Unix file modes is unusual for a JavaScript application. The comment explains: the git-clone path preserves +x natively, but the GCS/zip download path needs this helper. ZIP64 is explicitly not handled but noted as fine for marketplace zips (~3.5MB).

3. **The lazy imports** in both files (mcpb for manifest validation, fflate for extraction) show attention to startup performance. The plugin loader chain could reach these modules on every startup, but the lazy imports keep ~900KB of allocations out of the heap for sessions that never touch .dxt files.

4. **The `local.unpacked` and `local.dxt` prefixes** in extension ID generation distinguish between development (unpacked directory) and packaged (.dxt file) local installations.
