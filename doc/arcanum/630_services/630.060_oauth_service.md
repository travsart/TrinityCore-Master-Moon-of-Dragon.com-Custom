---
description: "OAuth service — OAuth PKCE flow, token storage refresh, MCP server authentication, device authorization, code exchange"
title: "OAuth Service -- Arcanum Wiki"
tags: [services, oauth-pkce, token-storage, device-authorization, code-exchange]
---

# OAuth Service -- Arcanum Wiki

## What Is This?

The OAuth service implements the OAuth 2.0 Authorization Code flow with PKCE for authenticating Claude AI subscribers (Max, Pro, Team, Enterprise). It handles browser-based login, token exchange, refresh, and profile retrieval.

## How It Works

The `OAuthService` class manages the full flow:

1. **Code generation**: Generates a PKCE code verifier, code challenge (S256), and state parameter
2. **Auth URL construction**: Builds URLs for both automatic (localhost redirect) and manual (copy-paste) flows
3. **Local listener**: `AuthCodeListener` starts an HTTP server on a random port to capture the redirect callback
4. **Browser open**: Opens the auth URL in the user's browser via `openBrowser()`
5. **Code capture**: Waits for either the localhost redirect (automatic) or user paste (manual)
6. **Token exchange**: Sends the authorization code to the token endpoint with the PKCE verifier
7. **Token storage**: OAuth tokens (access, refresh, expiry) are stored in the global config

**Token refresh**: `checkAndRefreshOAuthTokenIfNeeded()` (in `utils/auth.ts`) proactively refreshes expired tokens before API calls. The client constructor in `client.ts` calls this on every `getAnthropicClient()` invocation.

**Profile retrieval**: `getOauthProfile.ts` fetches the user's account info (account UUID, org UUID, subscription type, rate limit tier) used for permission decisions and analytics.

**Crypto**: Uses Node.js `crypto.randomBytes` for code verifier and state, SHA-256 for the code challenge.

## Key Source Files

| File | Purpose |
|------|---------|
| `index.ts` | `OAuthService` class -- full flow orchestration |
| `client.ts` | Token exchange, refresh, URL building, expiry checks |
| `crypto.ts` | PKCE code verifier/challenge generation |
| `auth-code-listener.ts` | Local HTTP server for redirect capture |
| `getOauthProfile.ts` | Profile and account info retrieval |

## Configuration

- OAuth config (client ID, endpoints): `src/constants/oauth.ts`
- `OAUTH_BETA_HEADER` required on auth-related API calls
- `USE_STAGING_OAUTH` for Ant staging environment
- Supports `loginHint`, `loginMethod`, `orgUUID` parameters
- `skipBrowserOpen` for SDK control protocol (caller manages display)

## Interesting Findings

1. **Dual flow support**: The service generates two URLs -- one for automatic (localhost redirect) and one for manual (copy-paste). The manual flow is used in non-browser environments (SSH, containers).

2. **Token expiry checking** in `isOAuthTokenExpired()` is used proactively by services like `fetchUtilization()` to skip API calls when tokens are known-expired, avoiding unnecessary 401 errors.

3. **The SDK path uses `skipBrowserOpen`** so the SDK client (not Claude Code) controls how/where the auth URL is opened. Both URLs are passed to the caller via `authURLHandler`.
