# MCP Server Configuration Requirements

## Critical Requirement: bin Entry in package.json

**ALWAYS** include a `bin` field in package.json for MCP servers to be discovered by Claude Code.

### Why This is Critical
- Claude Code scans package.json for `bin` field during MCP server discovery
- Without `bin` entry, the server will NOT appear in Claude Code's MCP servers list
- The `main` field alone is NOT sufficient for MCP server registration

### Correct Configuration
```json
{
  "name": "@trinitycore/mcp-server",
  "version": "1.0.0",
  "main": "dist/index.js",
  "bin": {
    "trinitycore-mcp": "./dist/index.js"
  }
}
```

### What Happens Without bin Entry
❌ Server builds successfully  
❌ Server runs when executed directly  
❌ BUT: Server does NOT appear in Claude Code  
❌ All tools are inaccessible from Claude AI

## Entry Point Requirements

### 1. Shebang in Source File
```typescript
#!/usr/bin/env node

// Rest of index.ts
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
// ...
```

### 2. File Must Be Executable (Unix/Linux/Mac)
```bash
chmod +x dist/index.js
```

### 3. TypeScript Compilation Must Preserve Shebang
Ensure tsconfig.json doesn't strip comments:
```json
{
  "compilerOptions": {
    "removeComments": false
  }
}
```

## Claude Code Configuration

### Configuration File Location
`.claude/mcp-servers-config.json` in project root

### Windows Path Format
**CRITICAL**: Use double backslashes (`\\`) in JSON:

```json
{
  "trinitycore-mcp": {
    "command": "node",
    "args": ["C:\\TrinityBots\\trinitycore-mcp\\dist\\index.js"],
    "env": {
      "TRINITY_DB_HOST": "localhost"
    }
  }
}
```

### Common Path Errors
```json
// ❌ WRONG - Single backslash (JSON escape error)
"args": ["C:\TrinityBots\trinitycore-mcp\dist\index.js"]

// ❌ WRONG - Forward slashes on Windows (may not work)
"args": ["C:/TrinityBots/trinitycore-mcp/dist/index.js"]

// ✅ CORRECT - Double backslashes
"args": ["C:\\TrinityBots\\trinitycore-mcp\\dist\\index.js"]
```

## Three Configuration Methods

### Method 1: Direct Node Execution (Recommended)
```json
{
  "trinitycore-mcp": {
    "command": "node",
    "args": ["C:\\TrinityBots\\trinitycore-mcp\\dist\\index.js"]
  }
}
```

**Pros**: 
- ✅ Works immediately after build
- ✅ No npm global install required
- ✅ Most reliable

**Cons**:
- ⚠️ Requires absolute path
- ⚠️ Must rebuild after changes

### Method 2: npx Execution
```json
{
  "trinitycore-mcp": {
    "command": "npx",
    "args": ["-y", "@trinitycore/mcp-server"]
  }
}
```

**Pros**:
- ✅ Automatic version updates
- ✅ No local path needed

**Cons**:
- ❌ Requires npm registry publication
- ⚠️ Network dependency

### Method 3: Global Binary
```json
{
  "trinitycore-mcp": {
    "command": "trinitycore-mcp"
  }
}
```

**Prerequisites**:
```bash
npm install -g .
```

**Pros**:
- ✅ Simplest configuration
- ✅ No paths needed

**Cons**:
- ⚠️ Requires global install
- ⚠️ Manual updates needed

## Environment Variables

### Passing to MCP Server
```json
{
  "trinitycore-mcp": {
    "command": "node",
    "args": ["C:\\path\\to\\dist\\index.js"],
    "env": {
      "TRINITY_DB_HOST": "localhost",
      "TRINITY_DB_PORT": "3306",
      "TRINITY_DB_USER": "trinity",
      "TRINITY_DB_PASSWORD": "your_password"
    }
  }
}
```

### Security: Never Commit Passwords
```bash
# Add to .gitignore
.claude/mcp-servers-config.json
```

Or use environment variable references:
```json
{
  "env": {
    "TRINITY_DB_PASSWORD": "${TRINITY_DB_PASSWORD}"
  }
}
```

## Troubleshooting Checklist

### MCP Server Not Appearing
1. ✅ Check package.json has `bin` field
2. ✅ Verify dist/index.js exists and has shebang
3. ✅ Ensure paths use double backslashes on Windows
4. ✅ Check .claude/mcp-servers-config.json syntax is valid JSON
5. ✅ Restart Claude Code after configuration changes
6. ✅ Run `npm run build` to ensure dist/ is current

### Testing MCP Server Directly
```bash
# Should start server (will exit after stdio setup in test env)
node C:\TrinityBots\trinitycore-mcp\dist\index.js

# Check for shebang
head -1 C:\TrinityBots\trinitycore-mcp\dist\index.js
# Should output: #!/usr/bin/env node

# Validate package.json
node -e "console.log(require('./package.json').bin)"
# Should output: { 'trinitycore-mcp': './dist/index.js' }
```

### Common Build Issues
```bash
# TypeScript compilation errors
npm run build

# If errors, check:
# 1. Type assertions for Object.entries() results
# 2. Null safety checks for optional properties
# 3. Import paths are correct
```

## Verification After Configuration

### Steps to Verify
1. Open Claude Code
2. Look for MCP servers section
3. Should see `trinitycore-mcp` listed
4. Test with simple query:
   ```
   Use get-trinity-api to show Player class methods
   ```
5. Should return TrinityCore API documentation

### Success Indicators
✅ Server appears in MCP server list  
✅ No error messages in console  
✅ Tools respond to queries  
✅ Database connections work

## Historical Context: trinitycore-mcp Issue

### The Problem (2025-10-29)
User reported: "trinitycore-mcp is not listed in claude code"

Despite having:
- ✅ 21 fully implemented tools
- ✅ Successful TypeScript builds
- ✅ Comprehensive documentation

The server was invisible to Claude Code.

### The Solution
Added missing `bin` entry to package.json in v1.2.2.

### Lesson Learned
**NEVER assume `main` field is sufficient for MCP servers.**  
**ALWAYS include `bin` field for executable registration.**

## Pre-Release Checklist for MCP Servers

Before releasing any MCP server:

- [ ] package.json has `bin` field
- [ ] Entry point has shebang (`#!/usr/bin/env node`)
- [ ] TypeScript builds without errors
- [ ] dist/index.js is executable (Unix systems)
- [ ] Tested in Claude Code with configuration
- [ ] Server appears in MCP servers list
- [ ] At least one tool tested successfully
- [ ] Documentation includes configuration examples
- [ ] Example config uses proper Windows path format
- [ ] .gitignore includes config with secrets

## Additional Best Practices

### 1. Provide Multiple Config Examples
Show all three methods (node, npx, global) in documentation.

### 2. Include Troubleshooting Section
Document common issues and solutions.

### 3. Validate Configuration
Provide command to test MCP server directly:
```bash
node dist/index.js
```

### 4. Environment Variables
Document all required and optional variables.

### 5. Security Notes
Warn against committing credentials.

## References

- TrinityCore MCP Server: C:\TrinityBots\trinitycore-mcp
- Configuration Guide: MCP_CONFIGURATION.md
- Fix Documentation: MCP_SERVER_FIX_COMPLETE.md
- GitHub Release: v1.2.2
