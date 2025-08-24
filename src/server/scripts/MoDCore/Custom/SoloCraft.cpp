// SPDX-License-Identifier: GPL-2.0-or-later
// TrinityCore external module: mod-solocraft (Retail/master compatible)

#include "ScriptMgr.h"
#include "Config.h"
#include "Player.h"
#include "Creature.h"
#include "Chat.h"
#include "Group.h"
#include "Map.h"
#include "ScriptedCreature.h"

// ------------------------------
// Konfiguration + Utilities
// ------------------------------
namespace
{
    struct SoloCraftConfig
    {
        bool   enabled = true;
        uint32 targetGroupSize = 5;     // auf diese Gruppengröße "simulieren"
        bool   scaleHealth = true;  // HP-Scaling bei Kampfbeginn
        bool   scaleDmgDealt = true;  // Spieler -> NPC
        bool   scaleDmgTaken = true;  // NPC -> Spieler
        bool   onlyInInstances = true;  // nur Dungeons/Scenarios
        bool   ignoreRaids = false; // auch in Raids zulassen
        float  maxScale = 10.0f; // harte Obergrenze
        bool   debug = false; // Chat/Log Meldungen
    };

    SoloCraftConfig sCfg;

    void LoadConfig()
    {
        // Aktuelle API: GetBoolDefault/GetIntDefault/GetFloatDefault
        sCfg.enabled = sConfigMgr->GetBoolDefault("SoloCraft.Enabled", true);
        sCfg.targetGroupSize = static_cast<uint32>(sConfigMgr->GetIntDefault("SoloCraft.TargetGroupSize", 5));
        sCfg.scaleHealth = sConfigMgr->GetBoolDefault("SoloCraft.ScaleHealth", true);
        sCfg.scaleDmgDealt = sConfigMgr->GetBoolDefault("SoloCraft.ScaleDamageDealt", true);
        sCfg.scaleDmgTaken = sConfigMgr->GetBoolDefault("SoloCraft.ScaleDamageTaken", true);
        sCfg.onlyInInstances = sConfigMgr->GetBoolDefault("SoloCraft.OnlyInInstances", true);
        sCfg.ignoreRaids = sConfigMgr->GetBoolDefault("SoloCraft.IgnoreRaids", false);
        sCfg.maxScale = sConfigMgr->GetFloatDefault("SoloCraft.MaxScale", 10.0f);
        sCfg.debug = sConfigMgr->GetBoolDefault("SoloCraft.Debug", false);
    }

    inline bool IsInstanceMap(Map const* map)
    {
        if (!map) return false;
        // Dungeon/Scenario als "Instance"; Raid separat behandelbar
        return map->IsDungeon() || map->IsScenario();
    }

    inline bool IsRaidMap(Map const* map)
    {
        return map && map->IsRaid();
    }

    inline bool ScopeAllows(Player const* player)
    {
        if (!player || !sCfg.enabled)
            return false;

        Map const* map = player->GetMap();
        if (sCfg.onlyInInstances && !IsInstanceMap(map))
            return false;

        if (!sCfg.ignoreRaids && IsRaidMap(map))
            return false;

        return true;
    }

    // Zähle nur Spieler in derselben Instanz (Map/InstanceId), nicht ganze Raid/Gilde
    uint32 CountPlayersInSameInstance(Player* player)
    {
        if (!player)
            return 1;

        Map* map = player->GetMap();
        if (!map)
            return 1;

        uint32 count = 0;
        Map::PlayerList const& players = map->GetPlayers();
        for (auto const& ref : players)
        {
            if (ref.GetSource())
                ++count;
        }
        return count > 0 ? count : 1;
    }

    // Effektive Skalierung berechnen
    float ComputeScale(Player* player)
    {
        if (!player)
            return 1.0f;

        uint32 count = CountPlayersInSameInstance(player);
        if (count >= sCfg.targetGroupSize)
            return 1.0f;

        float scale = float(sCfg.targetGroupSize) / float(count);
        if (scale > sCfg.maxScale)
            scale = sCfg.maxScale;

        return scale;
    }
}

// ------------------------------
// WorldScript: Config laden
// ------------------------------
class SoloCraft_World : public WorldScript
{
public:
    SoloCraft_World() : WorldScript("SoloCraft_World") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        LoadConfig();
        // Sicherer Logger-Kanal (vermeidet fehlende Makros)
        TC_LOG_INFO("server.loading", "SoloCraft: enabled={} target={} onlyInstances={} ignoreRaids={} maxScale={}",
            sCfg.enabled, sCfg.targetGroupSize, sCfg.onlyInInstances, sCfg.ignoreRaids, sCfg.maxScale);
    }
};

// ------------------------------
// PlayerScript: optionaler Hinweis
// ------------------------------
class SoloCraft_Player : public PlayerScript
{
public:
    SoloCraft_Player() : PlayerScript("SoloCraft_Player") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (sCfg.enabled && sCfg.debug && player)
            ChatHandler(player->GetSession()).PSendSysMessage(
                "SoloCraft aktiv: Target=%u, OnlyInstances=%u, IgnoreRaids=%u, MaxScale=%.1f",
                sCfg.targetGroupSize, sCfg.onlyInInstances, sCfg.ignoreRaids, sCfg.maxScale);
    }

    void OnMapChanged(Player* player) override
    {
        if (!player || !sCfg.enabled)
            return;

        Map* map = player->GetMap();
        if (!map)  // Schutz gegen nullptr
            return;

        if (!ScopeAllows(player))
            return;

        uint32 count = CountPlayersInSameInstance(player);
        float scale = ComputeScale(player);

        std::string mapName = map ? map->GetMapName() : "Unknown";

        ChatHandler ch(player->GetSession());
        ch.PSendSysMessage("|cff4CFF00[SoloCraft]|r Map: %s (ID: %u)", mapName.c_str(), map ? map->GetId() : 0);
        ch.PSendSysMessage("|cff4CFF00[SoloCraft]|r Players in instance: %u", count);
        ch.PSendSysMessage("|cff4CFF00[SoloCraft]|r Target group size: %u", sCfg.targetGroupSize);
        ch.PSendSysMessage("|cff4CFF00[SoloCraft]|r Effective scaling factor: x%.2f", scale);
        ch.PSendSysMessage("|cff4CFF00[SoloCraft]|r Features: HP=%u, DmgDealt=%u, DmgTaken=%u",
            sCfg.scaleHealth, sCfg.scaleDmgDealt, sCfg.scaleDmgTaken);
    }
};

// ------------------------------
// CreatureScript + AI: HP-Scaling
// ------------------------------
class SoloCraft_Creature : public CreatureScript
{
public:
    SoloCraft_Creature() : CreatureScript("SoloCraft_Creature") {}

    struct SoloCraftAI : public ScriptedAI
    {
        using ScriptedAI::ScriptedAI;

        bool   scaledOnce = false;
        uint64 baseMaxHP = 0;

        void Reset() override
        {
            // Beim Reset ursprüngliche HP wiederherstellen (falls vorher skaliert)
            if (scaledOnce && baseMaxHP > 0)
            {
                me->SetMaxHealth(baseMaxHP);
                me->SetHealth(baseMaxHP);
            }
            scaledOnce = false;
            baseMaxHP = 0;
        }

        // Retail/master: Kampfbeginn-Hook
        void JustEngagedWith(Unit* who) override
        {
            if (!sCfg.enabled || !sCfg.scaleHealth || scaledOnce)
                return;

            if (!who || !who->IsPlayer())
                return;

            Player* p = who->ToPlayer();
            if (!ScopeAllows(p))
                return;

            float scale = ComputeScale(p);
            if (scale <= 1.0f)
                return;

            baseMaxHP = me->GetMaxHealth();
            uint64 newMaxHP = static_cast<uint64>(double(baseMaxHP) * double(scale));
            if (newMaxHP < 1u) newMaxHP = 1u;

            me->SetMaxHealth(newMaxHP);
            me->SetHealth(newMaxHP);
            scaledOnce = true;

            if (sCfg.debug)
                TC_LOG_INFO("server.loading", "SoloCraft HP: entry={} base={} new={} scale={:.2f}",
                    me->GetEntry(), baseMaxHP, newMaxHP, scale);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new SoloCraftAI(creature);
    }
};

// ------------------------------
// UnitScript: Damage-Scaling
// ------------------------------
class SoloCraft_Unit : public UnitScript
{
public:
    SoloCraft_Unit() : UnitScript("SoloCraft_Unit") {}

    // Aktuelle Retail-Hook: OnDamage(attacker, victim, damage)
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!sCfg.enabled || damage == 0 || !attacker || !victim)
            return;

        // Spieler -> Kreatur : Buff den ausgehenden Schaden
        if (sCfg.scaleDmgDealt && attacker->IsPlayer() && victim->IsCreature())
        {
            Player* p = attacker->ToPlayer();
            if (ScopeAllows(p))
            {
                float scale = ComputeScale(p);
                if (scale > 1.0f)
                {
                    damage = static_cast<uint32>(double(damage) * double(scale));
                }
            }
        }

        // Kreatur -> Spieler : Reduziere eingehenden Schaden
        if (sCfg.scaleDmgTaken && attacker->IsCreature() && victim->IsPlayer())
        {
            Player* p = victim->ToPlayer();
            if (ScopeAllows(p))
            {
                float scale = ComputeScale(p);
                if (scale > 1.0f)
                {
                    damage = static_cast<uint32>(double(damage) / double(scale));
                }
            }
        }
    }
};

// ------------------------------
// Modul-Entry
// ------------------------------
void Add_MoDCore_SoloCraftScripts()
{
    new SoloCraft_World();
    new SoloCraft_Player();
    new SoloCraft_Creature();
    new SoloCraft_Unit();
}
