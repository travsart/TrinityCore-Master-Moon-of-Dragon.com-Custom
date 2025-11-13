#!/usr/bin/env python3
"""Remove private m_spellModTakingSpell accesses from Playerbot module"""

import re

def fix_botai():
    """Fix BotAI.cpp:339"""
    filepath = "src/modules/Playerbot/AI/BotAI.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace the private member access with explanation
    old = '''    if (!_firstUpdateComplete)
    {
        _bot->m_spellModTakingSpell = nullptr;  // CRITICAL: Clear before KillAllEvents to prevent Spell::~Spell assertion
        _bot->m_Events.KillAllEvents(false);  // false = graceful shutdown, not forced'''

    new = '''    if (!_firstUpdateComplete)
    {
        // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
        // No longer need to manually clear - KillAllEvents() will properly clean up spell mods
        _bot->m_Events.KillAllEvents(false);  // false = graceful shutdown, not forced'''

    content = content.replace(old, new)

    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

    print("DONE: Fixed BotAI.cpp")

def fix_deathrecovery():
    """Fix DeathRecoveryManager.cpp:903"""
    filepath = "src/modules/Playerbot/Lifecycle/DeathRecoveryManager.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    old = '''    m_bot->m_spellModTakingSpell = nullptr;  // CRITICAL: Clear before KillAllEvents to prevent Spell::~Spell assertion
    m_bot->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

    new = '''    // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
    // No longer need to manually clear - KillAllEvents() will properly clean up spell mods
    m_bot->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

    content = content.replace(old, new)

    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

    print("DONE: Fixed DeathRecoveryManager.cpp")

def fix_botsession_destructor():
    """Fix BotSession.cpp:436"""
    filepath = "src/modules/Playerbot/Session/BotSession.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    old = '''    if (Player* player = GetPlayer()) {
        try {
            player->m_spellModTakingSpell = nullptr;
            player->m_Events.KillAllEvents(false);'''

    new = '''    if (Player* player = GetPlayer()) {
        try {
            // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
            player->m_Events.KillAllEvents(false);'''

    content = content.replace(old, new)

    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

    print("DONE: Fixed BotSession.cpp destructor")

def fix_botsession_login():
    """Fix BotSession.cpp:1137"""
    filepath = "src/modules/Playerbot/Session/BotSession.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    old = '''        if (Player* player = GetPlayer())
        {
            // CRITICAL FIX: Clear m_spellModTakingSpell BEFORE killing events
            // When KillAllEvents destroys a delayed spell, the spell destructor checks this pointer
            // If we don't clear it first, we get: ASSERT(m_caster->ToPlayer()->m_spellModTakingSpell != this)
            player->m_spellModTakingSpell = nullptr;

            player->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

    new = '''        if (Player* player = GetPlayer())
        {
            // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
            // No longer need to manually clear - KillAllEvents() will properly clean up spell mods
            player->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

    content = content.replace(old, new)

    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

    print("DONE: Fixed BotSession.cpp login")

if __name__ == "__main__":
    fix_botai()
    fix_deathrecovery()
    fix_botsession_destructor()
    fix_botsession_login()
    print("\nAll 4 private member accesses removed successfully!")
