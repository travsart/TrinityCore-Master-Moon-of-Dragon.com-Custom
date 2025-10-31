#!/usr/bin/env python3
"""Fix BotSession_StateIntegration.cpp"""

filepath = "src/modules/Playerbot/Session/BotSession_StateIntegration.cpp"

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

old = '''        if (Player* player = GetPlayer())
        {
            player->m_spellModTakingSpell = nullptr;  // CRITICAL: Clear before KillAllEvents to prevent Spell::~Spell assertion
            player->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

new = '''        if (Player* player = GetPlayer())
        {
            // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
            player->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen'''

content = content.replace(old, new)

with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print("DONE: Fixed BotSession_StateIntegration.cpp")
