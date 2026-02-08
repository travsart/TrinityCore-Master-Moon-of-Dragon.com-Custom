/*
 * Copyright (C) 2024-2026 TrinityCore <https://www.trinitycore.org/>
 *
 * EnchantGemDatabase: Static enchant/gem recommendation database.
 */

#include "EnchantGemDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// Static Member Definitions
// ============================================================================

std::unordered_map<EnchantKey, std::vector<EnchantRecommendation>, EnchantKeyHash> EnchantGemDatabase::_enchants;
std::unordered_map<EnchantSlotType, std::vector<EnchantRecommendation>> EnchantGemDatabase::_enchantsBySlot;
std::unordered_map<GemKey, std::vector<GemRecommendation>, GemKeyHash> EnchantGemDatabase::_gems;
std::unordered_map<GemColor, std::vector<GemRecommendation>> EnchantGemDatabase::_gemsByColor;
bool EnchantGemDatabase::_initialized = false;

// ============================================================================
// Public API
// ============================================================================

void EnchantGemDatabase::Initialize()
{
    if (_initialized)
        return;

    TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: Loading enchant and gem recommendations...");

    LoadFromDatabase();

    // If DB is empty, use hardcoded defaults
    if (_enchants.empty())
    {
        TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: No DB data found, loading defaults...");
        LoadDefaultEnchants();
    }

    if (_gems.empty())
    {
        TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: No DB gem data found, loading defaults...");
        LoadDefaultGems();
    }

    _initialized = true;

    TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: Loaded {} enchant recommendations, {} gem recommendations",
        GetEnchantCount(), GetGemCount());
}

void EnchantGemDatabase::Reload()
{
    _enchants.clear();
    _enchantsBySlot.clear();
    _gems.clear();
    _gemsByColor.clear();
    _initialized = false;
    Initialize();
}

void EnchantGemDatabase::EnsureInitialized()
{
    if (!_initialized)
        Initialize();
}

EnchantRecommendation const* EnchantGemDatabase::GetBestEnchant(
    uint8 classId, uint8 specId, EnchantSlotType slotType, uint32 itemLevel)
{
    EnsureInitialized();

    // Try class/spec specific first
    auto it = _enchants.find({classId, specId, slotType});
    if (it != _enchants.end() && !it->second.empty())
    {
        for (auto const& e : it->second)
            if (itemLevel == 0 || itemLevel >= e.minItemLevel)
                return &e;
    }

    // Try class-specific, any spec
    it = _enchants.find({classId, 255, slotType});
    if (it != _enchants.end() && !it->second.empty())
    {
        for (auto const& e : it->second)
            if (itemLevel == 0 || itemLevel >= e.minItemLevel)
                return &e;
    }

    // Fall back to generic (class=0)
    it = _enchants.find({0, 255, slotType});
    if (it != _enchants.end() && !it->second.empty())
    {
        for (auto const& e : it->second)
            if (itemLevel == 0 || itemLevel >= e.minItemLevel)
                return &e;
    }

    return nullptr;
}

std::vector<EnchantRecommendation> const* EnchantGemDatabase::GetEnchantsBySlot(EnchantSlotType slotType)
{
    EnsureInitialized();
    auto it = _enchantsBySlot.find(slotType);
    return it != _enchantsBySlot.end() ? &it->second : nullptr;
}

std::vector<EnchantRecommendation> EnchantGemDatabase::GetEnchantsForSpec(uint8 classId, uint8 specId)
{
    EnsureInitialized();
    std::vector<EnchantRecommendation> result;
    for (auto const& [key, entries] : _enchants)
    {
        if ((key.classId == classId || key.classId == 0) &&
            (key.specId == specId || key.specId == 255))
        {
            for (auto const& e : entries)
                result.push_back(e);
        }
    }

    std::sort(result.begin(), result.end(),
        [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
    return result;
}

GemRecommendation const* EnchantGemDatabase::GetBestGem(
    uint8 classId, uint8 specId, GemColor socketColor)
{
    EnsureInitialized();

    // Try class/spec specific
    auto it = _gems.find({classId, specId, socketColor});
    if (it != _gems.end() && !it->second.empty())
        return &it->second.front();

    // Try class-specific, any spec
    it = _gems.find({classId, 255, socketColor});
    if (it != _gems.end() && !it->second.empty())
        return &it->second.front();

    // Fall back to generic
    it = _gems.find({0, 255, socketColor});
    if (it != _gems.end() && !it->second.empty())
        return &it->second.front();

    return nullptr;
}

std::vector<GemRecommendation> EnchantGemDatabase::GetGemsForColor(GemColor socketColor)
{
    EnsureInitialized();
    auto it = _gemsByColor.find(socketColor);
    if (it != _gemsByColor.end())
        return it->second;
    return {};
}

std::vector<GemRecommendation> EnchantGemDatabase::GetGemsForSpec(uint8 classId, uint8 specId)
{
    EnsureInitialized();
    std::vector<GemRecommendation> result;
    for (auto const& [key, entries] : _gems)
    {
        if ((key.classId == classId || key.classId == 0) &&
            (key.specId == specId || key.specId == 255))
        {
            for (auto const& e : entries)
                result.push_back(e);
        }
    }

    std::sort(result.begin(), result.end(),
        [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
    return result;
}

uint32 EnchantGemDatabase::GetEnchantCount()
{
    uint32 count = 0;
    for (auto const& [_, entries] : _enchants)
        count += static_cast<uint32>(entries.size());
    return count;
}

uint32 EnchantGemDatabase::GetGemCount()
{
    uint32 count = 0;
    for (auto const& [_, entries] : _gems)
        count += static_cast<uint32>(entries.size());
    return count;
}

// ============================================================================
// Database Loading
// ============================================================================

void EnchantGemDatabase::LoadFromDatabase()
{
    // Load enchant recommendations
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT enchant_id, slot_type, class_id, spec_id, min_item_level, priority_weight, enchant_name "
        "FROM playerbot_enchant_recommendations ORDER BY priority_weight DESC"))
    {
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            EnchantRecommendation rec;
            rec.enchantId = fields[0].GetUInt32();
            rec.slotType = static_cast<EnchantSlotType>(fields[1].GetUInt8());
            rec.classId = fields[2].GetUInt8();
            rec.specId = fields[3].GetUInt8();
            rec.minItemLevel = fields[4].GetUInt32();
            rec.priorityWeight = fields[5].GetFloat();
            rec.enchantName = fields[6].GetString();

            EnchantKey key{rec.classId, rec.specId, rec.slotType};
            _enchants[key].push_back(rec);
            _enchantsBySlot[rec.slotType].push_back(rec);
            ++count;
        } while (result->NextRow());

        TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: Loaded {} enchant recommendations from DB", count);
    }

    // Load gem recommendations
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT gem_item_id, socket_color, class_id, spec_id, stat_priority, priority_weight, gem_name "
        "FROM playerbot_gem_recommendations ORDER BY priority_weight DESC"))
    {
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            GemRecommendation rec;
            rec.gemItemId = fields[0].GetUInt32();
            rec.socketColor = static_cast<GemColor>(fields[1].GetUInt8());
            rec.classId = fields[2].GetUInt8();
            rec.specId = fields[3].GetUInt8();
            rec.statType = static_cast<GemStatPriority>(fields[4].GetUInt8());
            rec.priorityWeight = fields[5].GetFloat();
            rec.gemName = fields[6].GetString();

            GemKey key{rec.classId, rec.specId, rec.socketColor};
            _gems[key].push_back(rec);
            _gemsByColor[rec.socketColor].push_back(rec);
            ++count;
        } while (result->NextRow());

        TC_LOG_INFO("module.playerbot", "EnchantGemDatabase: Loaded {} gem recommendations from DB", count);
    }
}

// ============================================================================
// Default Enchants (WoW 12.0 The War Within)
// ============================================================================

static void AddEnchant(
    std::unordered_map<EnchantKey, std::vector<EnchantRecommendation>, EnchantKeyHash>& enchants,
    std::unordered_map<EnchantSlotType, std::vector<EnchantRecommendation>>& bySlot,
    uint32 id, EnchantSlotType slot, uint8 cls, uint8 spec, uint32 minIlvl, float weight, std::string name)
{
    EnchantRecommendation rec(id, slot, cls, spec, minIlvl, weight, std::move(name));
    EnchantKey key{cls, spec, slot};
    enchants[key].push_back(rec);
    bySlot[slot].push_back(rec);
}

void EnchantGemDatabase::LoadDefaultEnchants()
{
    // NOTE: These are representative enchant IDs for WoW 12.0.
    // The actual SpellItemEnchantment IDs should be verified against DBC/DB2 data.
    // Format: ID, SlotType, ClassId (0=any), SpecId (255=any), MinIlvl, Weight, Name

    // ---- Weapon Enchants (class=0 = generic, by role) ----

    // Melee DPS - Strength
    AddEnchant(_enchants, _enchantsBySlot, 7446, EnchantSlotType::MAIN_HAND, 1, 255, 0, 1.0f, "Authority of Fiery Resolve"); // Warrior
    AddEnchant(_enchants, _enchantsBySlot, 7446, EnchantSlotType::MAIN_HAND, 6, 255, 0, 1.0f, "Authority of Fiery Resolve"); // DK
    AddEnchant(_enchants, _enchantsBySlot, 7446, EnchantSlotType::MAIN_HAND, 2, 2, 0, 1.0f, "Authority of Fiery Resolve"); // Ret Paladin

    // Melee DPS - Agility
    AddEnchant(_enchants, _enchantsBySlot, 7448, EnchantSlotType::MAIN_HAND, 4, 255, 0, 1.0f, "Authority of the Depths"); // Rogue
    AddEnchant(_enchants, _enchantsBySlot, 7448, EnchantSlotType::MAIN_HAND, 12, 0, 0, 1.0f, "Authority of the Depths"); // DH Havoc
    AddEnchant(_enchants, _enchantsBySlot, 7448, EnchantSlotType::MAIN_HAND, 10, 2, 0, 1.0f, "Authority of the Depths"); // WW Monk
    AddEnchant(_enchants, _enchantsBySlot, 7448, EnchantSlotType::MAIN_HAND, 11, 1, 0, 1.0f, "Authority of the Depths"); // Feral Druid
    AddEnchant(_enchants, _enchantsBySlot, 7448, EnchantSlotType::MAIN_HAND, 3, 2, 0, 1.0f, "Authority of the Depths"); // Surv Hunter

    // Caster DPS - Intellect
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 8, 255, 0, 1.0f, "Authority of Radiant Power"); // Mage
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 9, 255, 0, 1.0f, "Authority of Radiant Power"); // Warlock
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 5, 2, 0, 1.0f, "Authority of Radiant Power"); // Shadow Priest
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 7, 0, 0, 1.0f, "Authority of Radiant Power"); // Ele Shaman
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 11, 0, 0, 1.0f, "Authority of Radiant Power"); // Balance Druid
    AddEnchant(_enchants, _enchantsBySlot, 7450, EnchantSlotType::MAIN_HAND, 13, 0, 0, 1.0f, "Authority of Radiant Power"); // Devastation Evoker

    // Healer - Intellect
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 5, 0, 0, 1.0f, "Authority of Storms"); // Disc Priest
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 5, 1, 0, 1.0f, "Authority of Storms"); // Holy Priest
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 2, 0, 0, 1.0f, "Authority of Storms"); // Holy Paladin
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 11, 3, 0, 1.0f, "Authority of Storms"); // Resto Druid
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 7, 2, 0, 1.0f, "Authority of Storms"); // Resto Shaman
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 10, 1, 0, 1.0f, "Authority of Storms"); // MW Monk
    AddEnchant(_enchants, _enchantsBySlot, 7452, EnchantSlotType::MAIN_HAND, 13, 1, 0, 1.0f, "Authority of Storms"); // Pres Evoker

    // Tank - generic weapon enchant
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 1, 2, 0, 1.0f, "Authority of Nerubian Fortification"); // Prot Warrior
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 2, 1, 0, 1.0f, "Authority of Nerubian Fortification"); // Prot Paladin
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 6, 0, 0, 1.0f, "Authority of Nerubian Fortification"); // Blood DK
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 12, 1, 0, 1.0f, "Authority of Nerubian Fortification"); // Veng DH
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 11, 2, 0, 1.0f, "Authority of Nerubian Fortification"); // Guard Druid
    AddEnchant(_enchants, _enchantsBySlot, 7454, EnchantSlotType::MAIN_HAND, 10, 0, 0, 1.0f, "Authority of Nerubian Fortification"); // BrM Monk

    // ---- Back (Cloak) Enchants - Generic ----
    AddEnchant(_enchants, _enchantsBySlot, 7401, EnchantSlotType::BACK, 0, 255, 0, 1.0f, "Chant of Winged Grace");
    AddEnchant(_enchants, _enchantsBySlot, 7403, EnchantSlotType::BACK, 0, 255, 0, 0.8f, "Chant of Leeching Fangs");

    // ---- Chest Enchants - Generic ----
    AddEnchant(_enchants, _enchantsBySlot, 7405, EnchantSlotType::CHEST, 0, 255, 0, 1.0f, "Crystalline Radiance");

    // ---- Wrist (Bracer) Enchants - Generic ----
    AddEnchant(_enchants, _enchantsBySlot, 7391, EnchantSlotType::WRIST, 0, 255, 0, 1.0f, "Chant of Armored Avoidance");
    AddEnchant(_enchants, _enchantsBySlot, 7393, EnchantSlotType::WRIST, 0, 255, 0, 0.8f, "Chant of Armored Leech");

    // ---- Ring Enchants - by role ----
    // DPS rings
    AddEnchant(_enchants, _enchantsBySlot, 7330, EnchantSlotType::RING_1, 0, 255, 0, 1.0f, "Radiant Critical Strike");
    AddEnchant(_enchants, _enchantsBySlot, 7332, EnchantSlotType::RING_1, 0, 255, 0, 0.9f, "Radiant Haste");
    AddEnchant(_enchants, _enchantsBySlot, 7334, EnchantSlotType::RING_1, 0, 255, 0, 0.85f, "Radiant Mastery");
    AddEnchant(_enchants, _enchantsBySlot, 7336, EnchantSlotType::RING_1, 0, 255, 0, 0.8f, "Radiant Versatility");
    AddEnchant(_enchants, _enchantsBySlot, 7330, EnchantSlotType::RING_2, 0, 255, 0, 1.0f, "Radiant Critical Strike");
    AddEnchant(_enchants, _enchantsBySlot, 7332, EnchantSlotType::RING_2, 0, 255, 0, 0.9f, "Radiant Haste");
    AddEnchant(_enchants, _enchantsBySlot, 7334, EnchantSlotType::RING_2, 0, 255, 0, 0.85f, "Radiant Mastery");
    AddEnchant(_enchants, _enchantsBySlot, 7336, EnchantSlotType::RING_2, 0, 255, 0, 0.8f, "Radiant Versatility");

    // ---- Legs Enchants - by armor type ----
    // Leather (plate/mail armor patches, cloth spellthread)
    AddEnchant(_enchants, _enchantsBySlot, 7531, EnchantSlotType::LEGS, 0, 255, 0, 1.0f, "Stormbound Armor Kit");
    AddEnchant(_enchants, _enchantsBySlot, 7534, EnchantSlotType::LEGS, 0, 255, 0, 0.9f, "Defender's Armor Kit");

    // ---- Feet (Boot) Enchants - Generic ----
    AddEnchant(_enchants, _enchantsBySlot, 7416, EnchantSlotType::FEET, 0, 255, 0, 1.0f, "Defender's March");
    AddEnchant(_enchants, _enchantsBySlot, 7418, EnchantSlotType::FEET, 0, 255, 0, 0.9f, "Scout's March");

    // Sort all lists by priority weight (descending)
    for (auto& [key, list] : _enchants)
        std::sort(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
    for (auto& [slot, list] : _enchantsBySlot)
        std::sort(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
}

// ============================================================================
// Default Gems (WoW 12.0 The War Within)
// ============================================================================

static void AddGem(
    std::unordered_map<GemKey, std::vector<GemRecommendation>, GemKeyHash>& gems,
    std::unordered_map<GemColor, std::vector<GemRecommendation>>& byColor,
    uint32 id, GemColor color, uint8 cls, uint8 spec, GemStatPriority stat, float weight, std::string name)
{
    GemRecommendation rec(id, color, cls, spec, stat, weight, std::move(name));
    GemKey key{cls, spec, color};
    gems[key].push_back(rec);
    byColor[color].push_back(rec);
}

void EnchantGemDatabase::LoadDefaultGems()
{
    // NOTE: These gem item IDs are representative for WoW 12.0.
    // Actual item entries should be verified against the world database.

    // ---- Prismatic Gems (fit any socket) ----
    // Primary stat gems (generic, any class/spec)
    AddGem(_gems, _gemsByColor, 213746, GemColor::PRISMATIC, 0, 255, GemStatPriority::PRIMARY, 1.0f, "Masterful Ruby");
    AddGem(_gems, _gemsByColor, 213743, GemColor::PRISMATIC, 0, 255, GemStatPriority::SECONDARY_BEST, 0.9f, "Quick Sapphire");
    AddGem(_gems, _gemsByColor, 213744, GemColor::PRISMATIC, 0, 255, GemStatPriority::SECONDARY_SECOND, 0.85f, "Deadly Emerald");
    AddGem(_gems, _gemsByColor, 213745, GemColor::PRISMATIC, 0, 255, GemStatPriority::STAMINA, 0.7f, "Solid Onyx");

    // ---- Role-specific gems ----

    // Strength DPS (Warrior Arms/Fury, DK Frost/Unholy, Ret Paladin)
    AddGem(_gems, _gemsByColor, 213755, GemColor::PRISMATIC, 1, 0, GemStatPriority::PRIMARY, 1.0f, "Bold Amber Crit"); // Arms
    AddGem(_gems, _gemsByColor, 213755, GemColor::PRISMATIC, 1, 1, GemStatPriority::PRIMARY, 1.0f, "Bold Amber Haste"); // Fury
    AddGem(_gems, _gemsByColor, 213755, GemColor::PRISMATIC, 6, 1, GemStatPriority::PRIMARY, 1.0f, "Bold Amber Mastery"); // Frost DK
    AddGem(_gems, _gemsByColor, 213755, GemColor::PRISMATIC, 6, 2, GemStatPriority::PRIMARY, 1.0f, "Bold Amber Haste"); // Unholy DK
    AddGem(_gems, _gemsByColor, 213755, GemColor::PRISMATIC, 2, 2, GemStatPriority::PRIMARY, 1.0f, "Bold Amber Haste"); // Ret

    // Agility DPS (Rogue, DH Havoc, Feral, WW Monk, Surv Hunter)
    AddGem(_gems, _gemsByColor, 213757, GemColor::PRISMATIC, 4, 255, GemStatPriority::PRIMARY, 1.0f, "Delicate Amber"); // Rogue all
    AddGem(_gems, _gemsByColor, 213757, GemColor::PRISMATIC, 12, 0, GemStatPriority::PRIMARY, 1.0f, "Delicate Amber"); // Havoc DH
    AddGem(_gems, _gemsByColor, 213757, GemColor::PRISMATIC, 11, 1, GemStatPriority::PRIMARY, 1.0f, "Delicate Amber"); // Feral
    AddGem(_gems, _gemsByColor, 213757, GemColor::PRISMATIC, 10, 2, GemStatPriority::PRIMARY, 1.0f, "Delicate Amber"); // WW

    // Intellect casters
    AddGem(_gems, _gemsByColor, 213759, GemColor::PRISMATIC, 8, 255, GemStatPriority::PRIMARY, 1.0f, "Brilliant Amber"); // Mage
    AddGem(_gems, _gemsByColor, 213759, GemColor::PRISMATIC, 9, 255, GemStatPriority::PRIMARY, 1.0f, "Brilliant Amber"); // Warlock
    AddGem(_gems, _gemsByColor, 213759, GemColor::PRISMATIC, 5, 255, GemStatPriority::PRIMARY, 1.0f, "Brilliant Amber"); // Priest
    AddGem(_gems, _gemsByColor, 213759, GemColor::PRISMATIC, 7, 255, GemStatPriority::PRIMARY, 1.0f, "Brilliant Amber"); // Shaman
    AddGem(_gems, _gemsByColor, 213759, GemColor::PRISMATIC, 13, 255, GemStatPriority::PRIMARY, 1.0f, "Brilliant Amber"); // Evoker

    // Tank gems (stamina focus)
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 1, 2, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // Prot Warrior
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 2, 1, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // Prot Paladin
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 6, 0, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // Blood DK
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 12, 1, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // Veng DH
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 11, 2, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // Guard Druid
    AddGem(_gems, _gemsByColor, 213761, GemColor::PRISMATIC, 10, 0, GemStatPriority::STAMINA, 1.0f, "Solid Amber Stam"); // BrM Monk

    // Sort by priority
    for (auto& [key, list] : _gems)
        std::sort(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
    for (auto& [color, list] : _gemsByColor)
        std::sort(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.priorityWeight > b.priorityWeight; });
}

} // namespace Playerbot
