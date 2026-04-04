#!/usr/bin/env python3
"""UserPromptSubmit hook: inject VoxCore-specific context based on what the
user is actually asking about.

PROBLEM: The community pattern injects git status on EVERY prompt. That's noise.
"yes" and "continue" don't need git status. What they need is targeted context
when the user mentions something VoxCore-specific.

APPROACH: Pattern-match the user's prompt for VoxCore keywords. Only inject
context when it's relevant, and inject the RIGHT context — not generic git
status but actual project state.

DESIGN DECISIONS:
- Short prompts (<10 chars) get NO injection (it's "yes", "ok", "continue")
- Keyword detection is case-insensitive substring matching — fast, no regex needed
- Each detector can inject additionalContext via stdout JSON
- Multiple detectors can fire; context is concatenated
- Everything is non-blocking — a detector failure silently skips
"""
import json
import os
import sys
from datetime import datetime, timezone

PROJECT_DIR = os.environ.get("CLAUDE_PROJECT_DIR", "C:/Users/atayl/VoxCore")


def detect_and_inject(prompt: str) -> list[str]:
    """Return list of context strings to inject based on prompt keywords."""
    context_parts = []
    prompt_lower = prompt.lower()

    # Short prompts: no injection
    if len(prompt.strip()) < 10:
        return []

    # --- TRANSMOG (ARCHIVED — reimplemented externally) ---
    if any(kw in prompt_lower for kw in ["transmog", "outfit", "wardrobe", "viewedoutfit", "adt", "idt"]):
        context_parts.append(
            "CONTEXT: Transmog UI work is ARCHIVED — reimplemented externally. "
            "Historical docs in doc/archive/transmog.md. No active transmog commands or agents."
        )

    # --- BUILD / COMPILE ---
    if any(kw in prompt_lower for kw in ["build error", "compile", "linker", "ninja", "cmake", "lnk2"]):
        context_parts.append(
            "CONTEXT: Build issue detected. Building from Claude Code IS allowed (ninja -j32). "
            "Use /parse-errors to categorize build output. Use /build-loop for iterative build+fix cycles."
        )

    # --- SERVER CRASH / LOGS ---
    if any(kw in prompt_lower for kw in ["crash", "server log", "dberror", "assertion", "restart"]):
        context_parts.append(
            "CONTEXT: Server issue detected. Run /check-logs immediately (it's auto-approved, read-only). "
            "Follow the 4-gate debugging pipeline in .claude/rules/debugging.md"
        )

    # --- SQL / DATABASE ---
    if any(kw in prompt_lower for kw in ["sql", "insert into", "update ", "delete from", "alter table",
                                          "creature_template", "hotfixes.", "world.", "characters."]):
        context_parts.append(
            "CONTEXT: SQL work detected. DESCRIBE tables before writing SQL. "
            "Use /new-sql-update for proper filename. Use /apply-sql to apply. "
            "Check doc/session_state.md for multi-tab DB locking."
        )

    # --- COMPANION SYSTEM ---
    if any(kw in prompt_lower for kw in ["companion", "companionai", "scompanionmgr", ".comp "]):
        context_parts.append(
            "CONTEXT: Companion system. Files: src/server/game/Companion/, "
            "src/server/scripts/Custom/Companion/. Memory: companion-system.md"
        )

    # --- SPELL WORK ---
    if any(kw in prompt_lower for kw in ["spell ", "spellid", "spell_name", "spell_effect", "aura"]):
        context_parts.append(
            "CONTEXT: Spell work. Custom spells use hotfix tables (spell_name, spell_misc, spell_effect + hotfix_data). "
            "Range 1900003+. Use /lookup-spell to resolve names to IDs. "
            "Don't also add serverside_spell if ID is in spell_name."
        )

    # --- PACKET / OPCODE ---
    if any(kw in prompt_lower for kw in ["packet", "opcode", "cmsg", "smsg", "sniff"]):
        context_parts.append(
            "CONTEXT: Packet analysis. Use /decode-pkt for raw .pkt files, /parse-packet for parsed output. "
            "Delegate to packet-analyzer agent (haiku, read-only) for heavy analysis."
        )

    # --- NPC / CREATURE ---
    if any(kw in prompt_lower for kw in ["creature", "npc ", "cnpc", "customnpc", "spawn"]):
        context_parts.append(
            "CONTEXT: NPC work. Custom NPCs: CreatureTemplateIdStart=400000. "
            "Use /lookup-creature. creature_template col is 'faction' (not FactionID), "
            "'npcflag' (bigint), spells in creature_template_spell."
        )

    # --- EFFECT / DISPLAY ---
    if any(kw in prompt_lower for kw in [".effect", ".display", "spellvisualkit", "morph", "wmorph"]):
        context_parts.append(
            "CONTEXT: Visual system. Effects: Noblegarden::EffectsHandler. "
            "Display: RoleplayCore::DisplayHandler. Morph: player_morph_scripts.cpp"
        )

    return context_parts


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    prompt = data.get("prompt", "")
    if not prompt:
        sys.exit(0)

    injections = detect_and_inject(prompt)

    if injections:
        # Return additionalContext — Claude sees this as extra context for the prompt
        result = {
            "additionalContext": "\n".join(injections)
        }
        json.dump(result, sys.stdout)


if __name__ == "__main__":
    main()
