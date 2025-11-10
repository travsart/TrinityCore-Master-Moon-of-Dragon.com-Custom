/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BotWorldPositioner.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "World.h"
#include "MapManager.h"
#include <algorithm>
#include <random>
#include <sstream>

namespace Playerbot
{

// ====================================================================
// SINGLETON INSTANCE
// ====================================================================

BotWorldPositioner* BotWorldPositioner::instance()
{
    static BotWorldPositioner instance;
    return &instance;
}

// ====================================================================
// INITIALIZATION
// ====================================================================

bool BotWorldPositioner::LoadZones()
{
    if (_initialized.load(std::memory_order_acquire))
    {
        TC_LOG_WARN("playerbot", "BotWorldPositioner::LoadZones() - Already initialized, skipping");
        return true;
    }

    TC_LOG_INFO("playerbot", "BotWorldPositioner::LoadZones() - Loading zone placements...");

    // Clear existing data
    _zones.clear();
    _zoneById.clear();
    _zonesByLevel.clear();
    _starterZonesByRace.clear();
    _allianceCapitals.clear();
    _hordeCapitals.clear();
    _stats = PositionerStats();

    // Load from configuration
    LoadZonesFromConfig();

    // If no zones loaded, build defaults
    if (_zones.empty())
    {
        TC_LOG_WARN("playerbot", "BotWorldPositioner::LoadZones() - No zones in config, building defaults");
        BuildDefaultZones();
    }

    // Validate zones
    ValidateZones();

    // Build lookup structures
    BuildZoneCache();

    // Update statistics
    _stats.totalZones = static_cast<uint32>(_zones.size());
    for (auto const& zone : _zones)
    {
        if (zone.isStarterZone)
            ++_stats.starterZones;
        else if (zone.maxLevel <= 60)
            ++_stats.levelingZones;
        else
            ++_stats.endgameZones;
    }
    _stats.capitalCities = static_cast<uint32>(_allianceCapitals.size() + _hordeCapitals.size());

    _initialized.store(true, std::memory_order_release);

    TC_LOG_INFO("playerbot", "BotWorldPositioner::LoadZones() - Loaded {} zones ({} starter, {} leveling, {} endgame, {} capitals)",
        _stats.totalZones, _stats.starterZones, _stats.levelingZones, _stats.endgameZones, _stats.capitalCities);

    return true;
}

void BotWorldPositioner::ReloadZones()
{
    _initialized.store(false, std::memory_order_release);
    LoadZones();
}

void BotWorldPositioner::LoadZonesFromConfig()
{
    // Future: Load from playerbots.conf
    // Format: Playerbot.Zones.Alliance.Zone1 = "ZoneID,MapID,X,Y,Z,O,MinLevel,MaxLevel,ZoneName"
    // For now, rely on BuildDefaultZones()
}

void BotWorldPositioner::BuildDefaultZones()
{
    // ====================================================================
    // ALLIANCE STARTER ZONES (L1-10)
    // ====================================================================

    // Human - Elwynn Forest (Northshire Abbey)
    _zones.push_back({12, 0, -8949.95f, -132.493f, 83.5312f, 0.0f, 1, 10, TEAM_ALLIANCE, "Elwynn Forest", true});

    // Dwarf/Gnome - Dun Morogh (Coldridge Valley)
    _zones.push_back({132, 0, -6240.32f, 331.033f, 382.758f, 0.0f, 1, 10, TEAM_ALLIANCE, "Dun Morogh", true});

    // Night Elf - Teldrassil (Shadowglen)
    _zones.push_back({188, 1, 10311.3f, 832.463f, 1326.41f, 5.69632f, 1, 10, TEAM_ALLIANCE, "Teldrassil", true});

    // Draenei - Azuremyst Isle (Ammen Vale)
    _zones.push_back({3524, 530, -4192.62f, -12576.7f, 36.7598f, 0.0f, 1, 10, TEAM_ALLIANCE, "Azuremyst Isle", true});

    // Worgen - Gilneas (Starting zone, phased)
    _zones.push_back({4755, 654, -1676.07f, 1345.55f, 15.1353f, 0.0f, 1, 10, TEAM_ALLIANCE, "Gilneas", true});

    // Pandaren (Alliance) - Stormwind City (post-tutorial)
    _zones.push_back({1519, 0, -8833.38f, 628.628f, 94.0066f, 1.06465f, 1, 10, TEAM_ALLIANCE, "Stormwind City", true});

    // ====================================================================
    // HORDE STARTER ZONES (L1-10)
    // ====================================================================

    // Orc/Troll - Durotar (Valley of Trials)
    _zones.push_back({14, 1, -602.608f, -4262.17f, 38.9529f, 0.0f, 1, 10, TEAM_HORDE, "Durotar", true});

    // Undead - Tirisfal Glades (Deathknell)
    _zones.push_back({85, 0, 1676.71f, 1678.31f, 121.67f, 2.70526f, 1, 10, TEAM_HORDE, "Tirisfal Glades", true});

    // Tauren - Mulgore (Red Cloud Mesa)
    _zones.push_back({215, 1, -2917.58f, -257.98f, 52.9968f, 0.0f, 1, 10, TEAM_HORDE, "Mulgore", true});

    // Blood Elf - Eversong Woods (Sunstrider Isle)
    _zones.push_back({3430, 530, 10349.6f, -6357.29f, 33.4026f, 5.31605f, 1, 10, TEAM_HORDE, "Eversong Woods", true});

    // Goblin - Kezan/Lost Isles (post-tutorial -> Orgrimmar)
    _zones.push_back({1637, 1, 1574.0f, -4439.0f, 15.4449f, 1.84061f, 1, 10, TEAM_HORDE, "Orgrimmar", true});

    // Pandaren (Horde) - Orgrimmar (post-tutorial)
    _zones.push_back({1637, 1, 1574.0f, -4439.0f, 15.4449f, 1.84061f, 1, 10, TEAM_HORDE, "Orgrimmar", true});

    // ====================================================================
    // LEVELING ZONES (L10-60)
    // ====================================================================

    // Alliance Leveling
    _zones.push_back({40, 0, -9449.06f, 64.8392f, 56.3581f, 0.0f, 10, 20, TEAM_ALLIANCE, "Westfall", false});
    _zones.push_back({3, 0, -10531.7f, -1281.91f, 38.8647f, 1.56959f, 15, 25, TEAM_ALLIANCE, "Redridge Mountains", false});
    _zones.push_back({38, 0, -11209.6f, 1666.54f, 24.6974f, 1.42053f, 20, 30, TEAM_ALLIANCE, "Duskwood", false});
    _zones.push_back({4, 0, -14297.2f, 518.269f, 8.77916f, 4.4586f, 30, 40, TEAM_ALLIANCE, "Stranglethorn Vale", false});

    // Horde Leveling
    _zones.push_back({17, 1, 304.614f, -4741.87f, 10.1027f, 0.0f, 10, 20, TEAM_HORDE, "The Barrens", false});
    _zones.push_back({406, 1, 6860.03f, -4767.11f, 696.833f, 5.31605f, 15, 25, TEAM_HORDE, "Stonetalon Mountains", false});
    _zones.push_back({16, 1, 2243.0f, -2487.0f, 97.05f, 0.72f, 20, 30, TEAM_HORDE, "Ashenvale", false});
    _zones.push_back({331, 1, -7176.38f, -3782.57f, 8.36981f, 6.00393f, 30, 40, TEAM_HORDE, "Desolace", false});

    // Neutral Leveling
    _zones.push_back({8, 0, -4919.88f, -3650.25f, 301.797f, 3.926991f, 40, 50, TEAM_NEUTRAL, "Searing Gorge", false});
    _zones.push_back({28, 0, -7179.0f, -921.0f, 165.377f, 5.09599f, 45, 55, TEAM_NEUTRAL, "Western Plaguelands", false});
    _zones.push_back({139, 0, 3352.92f, -3379.03f, 144.782f, 6.25562f, 50, 60, TEAM_NEUTRAL, "Eastern Plaguelands", false});

    // ====================================================================
    // ENDGAME ZONES (L60-80) - The War Within Content
    // ====================================================================

    // Dragonflight (60-70)
    _zones.push_back({13644, 2444, 4701.0f, 4679.0f, 55.0f, 0.0f, 60, 70, TEAM_NEUTRAL, "The Waking Shores", false});
    _zones.push_back({13645, 2444, -1695.0f, 2460.0f, 293.0f, 0.0f, 60, 70, TEAM_NEUTRAL, "Ohn'ahran Plains", false});

    // The War Within (70-80)
    _zones.push_back({14771, 2601, 2400.0f, -2800.0f, 180.0f, 0.0f, 70, 80, TEAM_NEUTRAL, "Isle of Dorn", false});
    _zones.push_back({14772, 2601, 1800.0f, -3200.0f, 150.0f, 0.0f, 70, 80, TEAM_NEUTRAL, "The Ringing Deeps", false});

    // ====================================================================
    // CAPITAL CITIES (All Levels)
    // ====================================================================

    // Alliance Capitals
    _zones.push_back({1519, 0, -8833.38f, 628.628f, 94.0066f, 1.06465f, 1, 80, TEAM_ALLIANCE, "Stormwind City", false});
    _zones.push_back({1537, 0, -4918.88f, -970.009f, 501.564f, 5.42347f, 1, 80, TEAM_ALLIANCE, "Ironforge", false});
    _zones.push_back({1657, 1, 9869.91f, 2493.58f, 1315.88f, 2.42346f, 1, 80, TEAM_ALLIANCE, "Darnassus", false});
    _zones.push_back({3557, 530, -3864.92f, -11643.7f, -137.644f, 5.50862f, 1, 80, TEAM_ALLIANCE, "The Exodar", false});

    // Horde Capitals
    _zones.push_back({1637, 1, 1574.0f, -4439.0f, 15.4449f, 1.84061f, 1, 80, TEAM_HORDE, "Orgrimmar", false});
    _zones.push_back({1638, 1, -1278.0f, 71.0f, 128.159f, 2.80623f, 1, 80, TEAM_HORDE, "Thunder Bluff", false});
    _zones.push_back({1497, 0, 1633.75f, 240.167f, -43.1034f, 6.26128f, 1, 80, TEAM_HORDE, "Undercity", false});
    _zones.push_back({3487, 530, 9738.28f, -7454.19f, 13.5605f, 0.043914f, 1, 80, TEAM_HORDE, "Silvermoon City", false});

    // Neutral Capitals
    _zones.push_back({4395, 571, 5804.15f, 624.771f, 647.767f, 1.64f, 1, 80, TEAM_NEUTRAL, "Dalaran (Northrend)", false});
    _zones.push_back({6134, 870, 867.965f, 226.952f, 503.159f, 3.93849f, 1, 80, TEAM_NEUTRAL, "Vale of Eternal Blossoms", false});

    TC_LOG_INFO("playerbot", "BotWorldPositioner::BuildDefaultZones() - Built {} default zone placements", _zones.size());
}

void BotWorldPositioner::ValidateZones()
{
    uint32 invalidCount = 0;

    for (auto& zone : _zones)
    {
        // Validate level range
        if (zone.minLevel > zone.maxLevel)
        {
            TC_LOG_ERROR("playerbot", "BotWorldPositioner::ValidateZones() - Invalid level range for zone {}: {} > {}",
                zone.zoneId, zone.minLevel, zone.maxLevel);
            std::swap(zone.minLevel, zone.maxLevel);
            ++invalidCount;
        }

        // Validate coordinates (basic sanity check)
        if (std::abs(zone.x) > 20000.0f || std::abs(zone.y) > 20000.0f || std::abs(zone.z) > 10000.0f)
        {
            TC_LOG_WARN("playerbot", "BotWorldPositioner::ValidateZones() - Suspicious coordinates for zone {}: ({}, {}, {})",
                zone.zoneId, zone.x, zone.y, zone.z);
        }
    }

    if (invalidCount > 0)
        TC_LOG_WARN("playerbot", "BotWorldPositioner::ValidateZones() - Fixed {} invalid zones", invalidCount);
}

void BotWorldPositioner::BuildZoneCache()
{
    // Build zone ID lookup
    for (auto const& zone : _zones)
    {
        _zoneById[zone.zoneId] = &zone;
    }

    // Build level-based lookup (every 5 levels)
    for (auto const& zone : _zones)
    {
        for (uint32 level = zone.minLevel; level <= zone.maxLevel; level += 5)
        {
            _zonesByLevel[level].push_back(&zone);
        }

        // Also add to exact level for precision
        _zonesByLevel[zone.minLevel].push_back(&zone);
        _zonesByLevel[zone.maxLevel].push_back(&zone);
    }

    // Build race-to-starter-zone mapping
    BuildRaceZoneMapping();

    // Build capital city lists
    for (auto const& zone : _zones)
    {
        if ((zone.zoneName.find("City") != std::string::npos ||
             zone.zoneName.find("Ironforge") != std::string::npos ||
             zone.zoneName.find("Darnassus") != std::string::npos ||
             zone.zoneName.find("Thunder Bluff") != std::string::npos ||
             zone.zoneName.find("Undercity") != std::string::npos) &&
            zone.minLevel == 1 && zone.maxLevel >= 70)
        {
            if (zone.faction == TEAM_ALLIANCE)
                _allianceCapitals.push_back(&zone);
            else if (zone.faction == TEAM_HORDE)
                _hordeCapitals.push_back(&zone);
        }
    }

    TC_LOG_DEBUG("playerbot", "BotWorldPositioner::BuildZoneCache() - Built cache: {} zones, {} level brackets",
        _zoneById.size(), _zonesByLevel.size());
}

void BotWorldPositioner::BuildRaceZoneMapping()
{
    // Map races to their starter zones based on zone names
    // This is a simplified mapping - in production, use DBC data

    for (auto const& zone : _zones)
    {
        if (!zone.isStarterZone)
            continue;

        // Alliance races
        if (zone.zoneName.find("Elwynn") != std::string::npos)
        {
            _starterZonesByRace[RACE_HUMAN].push_back(&zone);
        }
        else if (zone.zoneName.find("Dun Morogh") != std::string::npos)
        {
            _starterZonesByRace[RACE_DWARF].push_back(&zone);
            _starterZonesByRace[RACE_GNOME].push_back(&zone);
        }
        else if (zone.zoneName.find("Teldrassil") != std::string::npos)
        {
            _starterZonesByRace[RACE_NIGHTELF].push_back(&zone);
        }
        else if (zone.zoneName.find("Azuremyst") != std::string::npos)
        {
            _starterZonesByRace[RACE_DRAENEI].push_back(&zone);
        }
        else if (zone.zoneName.find("Gilneas") != std::string::npos)
        {
            _starterZonesByRace[RACE_WORGEN].push_back(&zone);
        }

        // Horde races
        else if (zone.zoneName.find("Durotar") != std::string::npos)
        {
            _starterZonesByRace[RACE_ORC].push_back(&zone);
            _starterZonesByRace[RACE_TROLL].push_back(&zone);
        }
        else if (zone.zoneName.find("Tirisfal") != std::string::npos)
        {
            _starterZonesByRace[RACE_UNDEAD_PLAYER].push_back(&zone);
        }
        else if (zone.zoneName.find("Mulgore") != std::string::npos)
        {
            _starterZonesByRace[RACE_TAUREN].push_back(&zone);
        }
        else if (zone.zoneName.find("Eversong") != std::string::npos)
        {
            _starterZonesByRace[RACE_BLOODELF].push_back(&zone);
        }

        // Neutral/Allied races (use capital cities)
        else if (zone.zoneName.find("Stormwind") != std::string::npos && zone.isStarterZone)
        {
            _starterZonesByRace[RACE_PANDAREN_ALLIANCE].push_back(&zone);
        }
        else if (zone.zoneName.find("Orgrimmar") != std::string::npos && zone.isStarterZone)
        {
            _starterZonesByRace[RACE_PANDAREN_HORDE].push_back(&zone);
            _starterZonesByRace[RACE_GOBLIN].push_back(&zone);
        }
    }

    TC_LOG_DEBUG("playerbot", "BotWorldPositioner::BuildRaceZoneMapping() - Mapped {} races to starter zones",
        _starterZonesByRace.size());
}

// ====================================================================
// ZONE SELECTION (Thread-safe)
// ====================================================================

ZoneChoice BotWorldPositioner::SelectZone(uint32 level, TeamId faction, uint8 race)
{
    if (!IsReady())
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::SelectZone() - Not initialized");
        return ZoneChoice();
    }

    // L1-4: Use starter zones (race-specific)
    if (level <= 4)
    {
        return GetStarterZone(race, faction);
    }

    // L5+: Use level-appropriate zones
    return SelectByLevelRange(level, faction);
}

ZoneChoice BotWorldPositioner::GetStarterZone(uint8 race, TeamId faction)
{
    // Try race-specific starter zone first
    auto raceItr = _starterZonesByRace.find(race);
    if (raceItr != _starterZonesByRace.end() && !raceItr->second.empty())
    {
        // Random selection if multiple options
        size_t idx = rand() % raceItr->second.size();
        return ZoneChoice(raceItr->second[idx], 1.0f);
    }

    // Fallback: Use any starter zone for faction
    std::vector<ZonePlacement const*> candidates;
    for (auto const& zone : _zones)
    {
        if (zone.isStarterZone && zone.IsValidForFaction(faction))
        {
            candidates.push_back(&zone);
        }
    }

    if (!candidates.empty())
    {
        size_t idx = rand() % candidates.size();
        return ZoneChoice(candidates[idx], 0.8f);
    }

    // Final fallback: Capital city
    TC_LOG_WARN("playerbot", "BotWorldPositioner::GetStarterZone() - No starter zone for race {}, using capital", race);
    return GetCapitalCity(faction);
}

ZoneChoice BotWorldPositioner::SelectByLevelRange(uint32 level, TeamId faction) const
{
    // Get all zones valid for this level
    std::vector<ZonePlacement const*> candidates;

    for (auto const& zone : _zones)
    {
        if (zone.IsValidForLevel(level) && zone.IsValidForFaction(faction) && !zone.isStarterZone)
        {
            candidates.push_back(&zone);
        }
    }

    if (candidates.empty())
    {
        TC_LOG_WARN("playerbot", "BotWorldPositioner::SelectByLevelRange() - No zones for level {}, faction {}",
            level, faction);
        return ZoneChoice();
    }

    // Select weighted by suitability
    return SelectWeighted(candidates);
}

ZoneChoice BotWorldPositioner::SelectWeighted(std::vector<ZonePlacement const*> const& candidates) const
{
    if (candidates.empty())
        return ZoneChoice();

    // For now, simple random selection
    // Future: Weight by current bot population density
    size_t idx = rand() % candidates.size();
    return ZoneChoice(candidates[idx], 1.0f);
}

float BotWorldPositioner::CalculateSuitability(ZonePlacement const* zone, uint32 level) const
{
    if (!zone->IsValidForLevel(level))
        return 0.0f;

    // Calculate how well the level fits within the zone's range
    uint32 range = zone->maxLevel - zone->minLevel;
    if (range == 0)
        return 1.0f;

    uint32 offset = (level > zone->minLevel) ? (level - zone->minLevel) : 0;
    float fit = 1.0f - (static_cast<float>(offset) / static_cast<float>(range));

    return std::max(0.0f, std::min(1.0f, fit));
}

ZoneChoice BotWorldPositioner::GetCapitalCity(TeamId faction)
{
    auto const& capitals = (faction == TEAM_ALLIANCE) ? _allianceCapitals : _hordeCapitals;

    if (capitals.empty())
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::GetCapitalCity() - No capitals for faction {}", faction);
        return ZoneChoice();
    }

    size_t idx = rand() % capitals.size();
    return ZoneChoice(capitals[idx], 0.5f);
}

std::vector<ZonePlacement const*> BotWorldPositioner::GetValidZones(uint32 level, TeamId faction) const
{
    std::vector<ZonePlacement const*> result;

    for (auto const& zone : _zones)
    {
        if (zone.IsValidForLevel(level) && zone.IsValidForFaction(faction))
        {
            result.push_back(&zone);
        }
    }

    return result;
}

// ====================================================================
// TELEPORTATION (Main Thread Only)
// ====================================================================

bool BotWorldPositioner::TeleportToZone(Player* bot, ZonePlacement const* placement)
{
    if (!bot || !placement)
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::TeleportToZone() - Invalid parameters");
        return false;
    }

    // Validate coordinates
    if (!ValidateTeleportCoordinates(placement))
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::TeleportToZone() - Invalid coordinates for zone {}",
            placement->zoneId);
        ++_stats.teleportsFailed;
        return false;
    }

    // Teleport using TrinityCore API
    bool success = bot->TeleportTo(placement->mapId, placement->x, placement->y, placement->z, placement->orientation);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method TeleportTo");
        return;
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method TeleportTo");
        return;
    }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

    if (success)
    {
        LogPlacement(bot, placement);
        ++_stats.botsPlaced;
        ++_stats.placementsPerZone[placement->zoneId];
    }
    else
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::TeleportToZone() - Teleport failed for bot {} to zone {}",
            bot->GetName(), placement->zoneId);
        ++_stats.teleportsFailed;
    }

    return success;
}

bool BotWorldPositioner::PlaceBot(Player* bot, uint32 level, TeamId faction, uint8 race)
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::PlaceBot() - Invalid bot pointer");
        return false;
    }

    // Select zone
    ZoneChoice choice = SelectZone(level, faction, race);
    if (!choice.IsValid())
    {
        TC_LOG_ERROR("playerbot", "BotWorldPositioner::PlaceBot() - Failed to select zone for bot {} (L{}, F{}, R{})",
            bot->GetName(), level, faction, race);
        return false;
    }

    // Teleport to zone
    return TeleportToZone(bot, choice.placement);
}

bool BotWorldPositioner::ValidateTeleportCoordinates(ZonePlacement const* placement) const
{
    if (!placement)
        return false;

    // Basic coordinate sanity checks
    if (std::abs(placement->x) > 20000.0f || std::abs(placement->y) > 20000.0f)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        return false;

    if (std::abs(placement->z) > 10000.0f)
        return false;

    // Could add map existence checks here
    // Map const* map = sMapMgr->FindMap(placement->mapId, 0);
    // if (!map) return false;

    return true;
}

void BotWorldPositioner::LogPlacement(Player* bot, ZonePlacement const* placement)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
{
    TC_LOG_DEBUG("playerbot", "BotWorldPositioner::LogPlacement() - Placed bot {} (L{}) in {} (Zone {})",
        bot->GetName(), bot->GetLevel(), placement->zoneName, placement->zoneId);
}

// ====================================================================
// ZONE QUERIES
// ====================================================================

ZonePlacement const* BotWorldPositioner::GetZonePlacement(uint32 zoneId) const
{
    auto itr = _zoneById.find(zoneId);
    return (itr != _zoneById.end()) ? itr->second : nullptr;
}

std::string BotWorldPositioner::GetZoneName(uint32 zoneId) const
{
    auto const* placement = GetZonePlacement(zoneId);
    return placement ? placement->zoneName : "Unknown Zone";
}

bool BotWorldPositioner::IsZoneValid(uint32 zoneId, uint32 level, TeamId faction) const
{
    auto const* placement = GetZonePlacement(zoneId);
    if (!placement)
        return false;

    return placement->IsValidForLevel(level) && placement->IsValidForFaction(faction);
}

// ====================================================================
// STATISTICS & DEBUGGING
// ====================================================================

void BotWorldPositioner::PrintZoneReport() const
{
    TC_LOG_INFO("playerbot", "====================================================================");
    TC_LOG_INFO("playerbot", "BOT WORLD POSITIONER - ZONE REPORT");
    TC_LOG_INFO("playerbot", "====================================================================");
    TC_LOG_INFO("playerbot", "Total Zones:         {}", _stats.totalZones);
    TC_LOG_INFO("playerbot", "  Starter Zones:     {}", _stats.starterZones);
    TC_LOG_INFO("playerbot", "  Leveling Zones:    {}", _stats.levelingZones);
    TC_LOG_INFO("playerbot", "  Endgame Zones:     {}", _stats.endgameZones);
    TC_LOG_INFO("playerbot", "  Capital Cities:    {}", _stats.capitalCities);
    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "Placements:          {}", _stats.botsPlaced);
    TC_LOG_INFO("playerbot", "Failed Teleports:    {}", _stats.teleportsFailed);
    TC_LOG_INFO("playerbot", "");

    if (!_stats.placementsPerZone.empty())
    {
        TC_LOG_INFO("playerbot", "Top 10 Most Popular Zones:");
        std::vector<std::pair<uint32, uint32>> sorted(_stats.placementsPerZone.begin(), _stats.placementsPerZone.end());
        std::sort(sorted.begin(), sorted.end(),
            [](auto const& a, auto const& b) { return a.second > b.second; });

        size_t count = 0;
        for (auto const& [zoneId, placements] : sorted)
        {
            if (count++ >= 10)
                break;

            std::string zoneName = GetZoneName(zoneId);
            TC_LOG_INFO("playerbot", "  {} ({}): {} bots", zoneName, zoneId, placements);
        }
    }

    TC_LOG_INFO("playerbot", "====================================================================");
}

std::string BotWorldPositioner::GetZoneSummary() const
{
    std::ostringstream oss;
    oss << "BotWorldPositioner: " << _stats.totalZones << " zones ("
        << _stats.starterZones << " starter, "
        << _stats.levelingZones << " leveling, "
        << _stats.endgameZones << " endgame), "
        << _stats.botsPlaced << " bots placed, "
        << _stats.teleportsFailed << " failed";
    return oss.str();
}

} // namespace Playerbot
