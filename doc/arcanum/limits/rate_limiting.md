---
description: "rate limiting — policyLimits service, API rate limits, /rate-limit-options /passes /mock-limits /reset-limits, backoff queue behavior"
---

# Rate Limiting
> Source: `services/policyLimits/`, rate-limit commands
> Status: STUB — needs research

## Key Questions
- What rate limits exist? (API calls, tokens, messages per minute?)
- How does CC handle hitting limits? (backoff, queue, error?)
- The `/rate-limit-options` command
- The `/passes` command — what are passes?
- `/mock-limits` — testing tool for limit behavior
- `/reset-limits` — what does it reset?
