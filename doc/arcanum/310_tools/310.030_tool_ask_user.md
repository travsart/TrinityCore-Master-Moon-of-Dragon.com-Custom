---
description: "AskUserQuestionTool — interactive clarification, multi-question support, option previews, multiSelect, annotations, SDK structured IO, permission component"
title: "AskUserQuestionTool -- Arcanum Wiki"
tags: [tools, multi-question-support, option-previews, multiselect, annotations, sdk-structured, permission-component]
---

# AskUserQuestionTool -- Arcanum Wiki

## Purpose

AskUserQuestionTool allows the model to ask the user a question and receive their response. It is the primary mechanism for interactive clarification during task execution, supporting both terminal UI and SDK/print mode.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `question` | string | Yes | The question to ask the user |

## Execution Flow

1. The tool presents the question via the terminal UI (or SDK structured IO).
2. Waits for the user's typed response.
3. Returns the response as a new user message injected into the conversation.

## Key Implementation Details

- The tool uses `setToolJSX` to render the question in the terminal UI with a styled prompt
- In non-interactive sessions (SDK mode), it uses `requestPrompt` callback
- The user's response is returned as `newMessages` in the tool result, injecting a user message into the conversation
- `requiresUserInteraction()` returns true, which affects how the tool is handled in background/agent contexts

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| Concurrency safe | No |
| Read only | Yes |
| Requires user interaction | Yes |

## Permission Requirements

No special permissions needed -- the tool itself IS the permission interaction (asking the user).

## Interesting Findings

1. AskUserQuestionTool is one of the few tools that returns `newMessages` to inject content into the conversation history, rather than just returning data.

2. The `requiresUserInteraction()` flag is checked by agent contexts to determine whether the tool can be used in background or non-interactive modes.
