/*
 * Copyright (C) 2024+ TrinityCore <http://www.trinitycore.org/>
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

#include "QuestHubDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "World.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <set>

namespace Playerbot
{
    // ============================================================================
    // QuestHub Implementation
    // ============================================================================

    bool QuestHub::IsAppropriateFor(Player const* player) const
    {
        if (!player)
            return false;

        // Check level range
        uint32 playerLevel = player->GetLevel();
        if (playerLevel < minLevel)
            return false;

        if (maxLevel > 0 && playerLevel > maxLevel)
            return false;

        // Check faction
        TeamId team = player->GetTeamId();
        uint32 playerFactionBit = 0;

        switch (team)
        {
            case TEAM_ALLIANCE:
                playerFactionBit = 0x01; // Bit 0
                break;
            case TEAM_HORDE:
                playerFactionBit = 0x02; // Bit 1
                break;
            default:
                playerFactionBit = 0x04; // Bit 2 (Neutral)
                break;
        }

        // Check if faction is allowed
        if (!(factionMask & playerFactionBit))
            return false;

        return true;
    }

    float QuestHub::GetDistanceFrom(Player const* player) const
    {
        if (!player)
            return std::numeric_limits<float>::max();

        // Use 2D distance for performance (Z-axis less relevant for quest selection)
        float dx = player->GetPositionX() - location.GetPositionX();
        float dy = player->GetPositionY() - location.GetPositionY();
        return std::sqrt(dx * dx + dy * dy);
    }

    bool QuestHub::ContainsPosition(Position const& pos) const
    {
        float dx = pos.GetPositionX() - location.GetPositionX();
        float dy = pos.GetPositionY() - location.GetPositionY();
        float distance = std::sqrt(dx * dx + dy * dy);
        return distance <= radius;
    }

    float QuestHub::CalculateSuitabilityScore(Player const* player) const
    {
        if (!IsAppropriateFor(player))
            return 0.0f;

        float score = 100.0f; // Base score

        // Level appropriateness scoring
        uint32 playerLevel = player->GetLevel();
        uint32 hubMidLevel = (minLevel + maxLevel) / 2;
        int32 levelDiff = std::abs(static_cast<int32>(playerLevel) - static_cast<int32>(hubMidLevel));

        // Ideal: ±2 levels = full score
        // ±5 levels = 50% score
        // ±10 levels = 10% score
        float levelScore = std::max(0.0f, 1.0f - (levelDiff / 10.0f));
        score *= levelScore;

        // Distance penalty (closer is better)
        float distance = GetDistanceFrom(player);
        float distanceScore = 1.0f / (1.0f + distance / 1000.0f); // Normalize to 0-1 range
        score *= distanceScore;

        // Quest availability bonus
        float questBonus = 1.0f + (questIds.size() * 0.1f); // 10% bonus per quest
        score *= std::min(2.0f, questBonus); // Cap at 2x multiplier

        return score;
    }

    // ============================================================================
    // QuestHubDatabase Implementation
    // ============================================================================

    QuestHubDatabase& QuestHubDatabase::Instance()
    {
        static QuestHubDatabase instance;
        return instance;
    }

    bool QuestHubDatabase::Initialize()
    {
        TC_LOG_INFO("playerbot", "QuestHubDatabase: Initializing quest hub database...");

        auto startTime = std::chrono::steady_clock::now();

        try
        {
            // Step 1: Load quest givers from database
            uint32 questGiverCount = LoadQuestGiversFromDB();
            if (questGiverCount == 0)
            {
                TC_LOG_ERROR("playerbot", "QuestHubDatabase: No quest givers found in database!");
                return false;
            }

            TC_LOG_DEBUG("playerbot", "QuestHubDatabase: Loaded {} quest givers", questGiverCount);

            // Step 2: Cluster quest givers into hubs
            uint32 hubCount = ClusterQuestGiversIntoHubs();
            if (hubCount == 0)
            {
                TC_LOG_ERROR("playerbot", "QuestHubDatabase: Failed to create quest hubs!");
                return false;
            }

            TC_LOG_DEBUG("playerbot", "QuestHubDatabase: Created {} quest hubs", hubCount);

            // Step 3: Load quest data for hubs
            LoadQuestDataForHubs();

            // Step 4: Build spatial index
            BuildSpatialIndex();

            // Step 5: Validate data
            ValidateHubData();

            // Calculate memory usage
            _memoryUsage = _questHubs.size() * sizeof(QuestHub);
            _memoryUsage += _hubIdToIndex.size() * (sizeof(uint32) + sizeof(size_t));
            for (auto const& [zoneId, indices] : _zoneIndex)
                _memoryUsage += indices.size() * sizeof(size_t);

            _initialized = true;

            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            TC_LOG_INFO("playerbot", "QuestHubDatabase: Initialization complete. "
                                     "{} hubs loaded in {} ms. Memory usage: {} KB",
                        hubCount, duration.count(), _memoryUsage / 1024);

            return true;
        }
        catch (std::exception const& ex)
        {
            TC_LOG_ERROR("playerbot", "QuestHubDatabase: Exception during initialization: {}", ex.what());
            return false;
        }
    }

    bool QuestHubDatabase::Reload()
    {
        TC_LOG_INFO("playerbot", "QuestHubDatabase: Reloading quest hub database...");

        // Exclusive lock for writing
        std::unique_lock lock(_mutex);

        // Clear existing data
        _questHubs.clear();
        _hubIdToIndex.clear();
        _zoneIndex.clear();
        _initialized = false;
        _memoryUsage = 0;

        // Release lock before calling Initialize (which will acquire its own locks as needed)
        lock.unlock();

        return Initialize();
    }

    std::vector<QuestHub const*> QuestHubDatabase::GetQuestHubsForPlayer(
        Player const* player,
        uint32 maxCount) const
    {
        if (!player || !_initialized)
            return {};

        // Shared lock for reading
        std::shared_lock lock(_mutex);

        // Collect all appropriate hubs with their scores
        std::vector<std::pair<QuestHub const*, float>> scoredHubs;
        scoredHubs.reserve(_questHubs.size());

        for (auto const& hub : _questHubs)
        {
            float score = hub.CalculateSuitabilityScore(player);
            if (score > 0.0f)
                scoredHubs.emplace_back(&hub, score);
        }

        // Sort by score (highest first)
        std::sort(scoredHubs.begin(), scoredHubs.end(),
            [](auto const& a, auto const& b) { return a.second > b.second; });

        // Extract top N hubs
        std::vector<QuestHub const*> result;
        result.reserve(std::min(maxCount, static_cast<uint32>(scoredHubs.size())));

        for (size_t i = 0; i < scoredHubs.size() && i < maxCount; ++i)
            result.push_back(scoredHubs[i].first);

        return result;
    }

    QuestHub const* QuestHubDatabase::GetNearestQuestHub(Player const* player) const
    {
        if (!player || !_initialized)
            return nullptr;

        // Shared lock for reading
        std::shared_lock lock(_mutex);

        QuestHub const* nearest = nullptr;
        float minDistance = std::numeric_limits<float>::max();

        for (auto const& hub : _questHubs)
        {
            if (!hub.IsAppropriateFor(player))
                continue;

            float distance = hub.GetDistanceFrom(player);
            if (distance < minDistance)
            {
                minDistance = distance;
                nearest = &hub;
            }
        }

        return nearest;
    }

    QuestHub const* QuestHubDatabase::GetQuestHubById(uint32 hubId) const
    {
        if (!_initialized)
            return nullptr;

        // Shared lock for reading
        std::shared_lock lock(_mutex);

        auto it = _hubIdToIndex.find(hubId);
        if (it == _hubIdToIndex.end())
            return nullptr;

        return &_questHubs[it->second];
    }

    std::vector<QuestHub const*> QuestHubDatabase::GetQuestHubsInZone(uint32 zoneId) const
    {
        if (!_initialized)
            return {};

        // Shared lock for reading
        std::shared_lock lock(_mutex);

        auto it = _zoneIndex.find(zoneId);
        if (it == _zoneIndex.end())
            return {};

        std::vector<QuestHub const*> result;
        result.reserve(it->second.size());

        for (size_t index : it->second)
            result.push_back(&_questHubs[index]);

        return result;
    }

    QuestHub const* QuestHubDatabase::GetQuestHubAtPosition(
        Position const& pos,
        std::optional<uint32> zoneId) const
    {
        if (!_initialized)
            return nullptr;

        // Shared lock for reading
        std::shared_lock lock(_mutex);

        // If zone is specified, only check hubs in that zone
        if (zoneId.has_value())
        {
            auto it = _zoneIndex.find(*zoneId);
            if (it == _zoneIndex.end())
                return nullptr;

            for (size_t index : it->second)
            {
                if (_questHubs[index].ContainsPosition(pos))
                    return &_questHubs[index];
            }
        }
        else
        {
            // Check all hubs
            for (auto const& hub : _questHubs)
            {
                if (hub.ContainsPosition(pos))
                    return &hub;
            }
        }

        return nullptr;
    }

    size_t QuestHubDatabase::GetQuestHubCount() const
    {
        std::shared_lock lock(_mutex);
        return _questHubs.size();
    }

    size_t QuestHubDatabase::GetMemoryUsage() const
    {
        std::shared_lock lock(_mutex);
        return _memoryUsage;
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    struct QuestGiverData
    {
        uint32 creatureEntry;
        Position position;
        uint32 zoneId;
        uint32 factionTemplate;
    };

    uint32 QuestHubDatabase::LoadQuestGiversFromDB()
    {
        std::vector<QuestGiverData> questGivers;

        // Query creature spawns that are quest givers
        QueryResult result = WorldDatabase.Query(
            "SELECT c.guid, c.id, c.position_x, c.position_y, c.position_z, "
            "c.map, ct.faction, COALESCE(c.zoneId, 0) as zoneId "
            "FROM creature c "
            "INNER JOIN creature_template ct ON c.id = ct.entry "
            "WHERE ct.npcflag & 2 != 0 " // UNIT_NPC_FLAG_QUESTGIVER
            "AND c.map < 2 " // Only Azeroth (0 = Eastern Kingdoms, 1 = Kalimdor)
        );

        if (!result)
        {
            TC_LOG_ERROR("playerbot", "QuestHubDatabase: Failed to load quest givers from database");
            return 0;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            QuestGiverData data;
            data.creatureEntry = fields[1].GetUInt32();
            data.position.Relocate(
                fields[2].GetFloat(),  // x
                fields[3].GetFloat(),  // y
                fields[4].GetFloat()   // z
            );
            data.zoneId = fields[7].GetUInt32();
            data.factionTemplate = fields[6].GetUInt32();

            questGivers.push_back(data);
            ++count;

        } while (result->NextRow());

        // Store quest givers temporarily for clustering
        // We'll use a member variable to pass data between methods
        struct TempData
        {
            std::vector<QuestGiverData> questGivers;
        };
        static TempData tempData;
        tempData.questGivers = std::move(questGivers);

        return count;
    }

    uint32 QuestHubDatabase::ClusterQuestGiversIntoHubs()
    {
        // Retrieve temporary data
        struct TempData
        {
            std::vector<QuestGiverData> questGivers;
        };
        static TempData tempData;

        if (tempData.questGivers.empty())
            return 0;

        // DBSCAN clustering parameters
        constexpr float EPSILON = 75.0f;  // 75 yards search radius
        constexpr uint32 MIN_POINTS = 2;  // Minimum 2 quest givers per hub

        std::vector<bool> visited(tempData.questGivers.size(), false);
        std::vector<int32> clusterIds(tempData.questGivers.size(), -1); // -1 = noise
        int32 currentClusterId = 0;

        // Lambda: Find neighbors within EPSILON
        auto findNeighbors = [&](size_t index) -> std::vector<size_t>
        {
            std::vector<size_t> neighbors;
            Position const& pos = tempData.questGivers[index].position;

            for (size_t i = 0; i < tempData.questGivers.size(); ++i)
            {
                if (i == index)
                    continue;

                Position const& otherPos = tempData.questGivers[i].position;
                float dx = pos.GetPositionX() - otherPos.GetPositionX();
                float dy = pos.GetPositionY() - otherPos.GetPositionY();
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance <= EPSILON)
                    neighbors.push_back(i);
            }

            return neighbors;
        };

        // DBSCAN algorithm
        for (size_t i = 0; i < tempData.questGivers.size(); ++i)
        {
            if (visited[i])
                continue;

            visited[i] = true;
            std::vector<size_t> neighbors = findNeighbors(i);

            if (neighbors.size() < MIN_POINTS)
            {
                // Mark as noise (singleton quest giver)
                clusterIds[i] = -1;
                continue;
            }

            // Start new cluster
            clusterIds[i] = currentClusterId;

            // Expand cluster
            for (size_t j = 0; j < neighbors.size(); ++j)
            {
                size_t neighborIdx = neighbors[j];

                if (!visited[neighborIdx])
                {
                    visited[neighborIdx] = true;
                    std::vector<size_t> neighborNeighbors = findNeighbors(neighborIdx);

                    if (neighborNeighbors.size() >= MIN_POINTS)
                    {
                        // Add new neighbors to expansion list
                        neighbors.insert(neighbors.end(),
                                       neighborNeighbors.begin(),
                                       neighborNeighbors.end());
                    }
                }

                if (clusterIds[neighborIdx] == -1)
                    clusterIds[neighborIdx] = currentClusterId;
            }

            ++currentClusterId;
        }

        // Create QuestHub objects from clusters
        std::unordered_map<int32, std::vector<size_t>> clusterMap;
        for (size_t i = 0; i < clusterIds.size(); ++i)
        {
            if (clusterIds[i] >= 0)
                clusterMap[clusterIds[i]].push_back(i);
        }

        uint32 hubId = 1;
        for (auto const& [clusterId, indices] : clusterMap)
        {
            if (indices.size() < MIN_POINTS)
                continue;

            QuestHub hub;
            hub.hubId = hubId++;

            // Calculate center position (average of all quest giver positions)
            float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
            std::set<uint32> uniqueCreatures;
            uint32 zoneId = 0;

            for (size_t idx : indices)
            {
                QuestGiverData const& qg = tempData.questGivers[idx];
                sumX += qg.position.GetPositionX();
                sumY += qg.position.GetPositionY();
                sumZ += qg.position.GetPositionZ();
                uniqueCreatures.insert(qg.creatureEntry);

                if (zoneId == 0)
                    zoneId = qg.zoneId;
            }

            hub.location.Relocate(
                sumX / indices.size(),
                sumY / indices.size(),
                sumZ / indices.size()
            );

            hub.zoneId = zoneId;
            hub.creatureIds.assign(uniqueCreatures.begin(), uniqueCreatures.end());

            // Calculate radius (max distance from center)
            float maxDist = 0.0f;
            for (size_t idx : indices)
            {
                Position const& pos = tempData.questGivers[idx].position;
                float dx = pos.GetPositionX() - hub.location.GetPositionX();
                float dy = pos.GetPositionY() - hub.location.GetPositionY();
                float dist = std::sqrt(dx * dx + dy * dy);
                maxDist = std::max(maxDist, dist);
            }
            hub.radius = maxDist + 10.0f; // Add 10 yard buffer

            // Default faction mask (will be refined with quest data)
            hub.factionMask = 0x07; // All factions initially

            // Generate name (will be refined with zone data)
            hub.name = "Quest Hub " + std::to_string(hub.hubId);

            _questHubs.push_back(std::move(hub));
        }

        return static_cast<uint32>(_questHubs.size());
    }

    void QuestHubDatabase::LoadQuestDataForHubs()
    {
        for (auto& hub : _questHubs)
        {
            if (hub.creatureIds.empty())
                continue;

            // Build IN clause for SQL query
            std::string creatureList;
            for (size_t i = 0; i < hub.creatureIds.size(); ++i)
            {
                if (i > 0)
                    creatureList += ",";
                creatureList += std::to_string(hub.creatureIds[i]);
            }

            // Query quests offered by creatures in this hub
            std::string query = "SELECT DISTINCT qr.quest, qt.MinLevel, qt.QuestLevel, qt.AllowableRaces "
                               "FROM creature_queststarter qr "
                               "INNER JOIN quest_template qt ON qr.quest = qt.ID "
                               "WHERE qr.id IN (" + creatureList + ")";

            QueryResult result = WorldDatabase.Query(query.c_str());

            if (!result)
                continue;

            uint32 minLevel = 255;
            uint32 maxLevel = 0;
            std::set<uint32> uniqueQuests;

            do
            {
                Field* fields = result->Fetch();

                uint32 questId = fields[0].GetUInt32();
                uint32 questMinLevel = fields[1].GetUInt32();
                uint32 questLevel = fields[2].GetUInt32();
                uint32 allowableRaces = fields[3].GetUInt32();

                uniqueQuests.insert(questId);

                // Update level range
                minLevel = std::min(minLevel, questMinLevel);
                maxLevel = std::max(maxLevel, questLevel);

                // Update faction mask based on allowable races
                // WoW race bits: Alliance (1,3,4,7,11,22,25,29,32,34,37), Horde (2,5,6,8,9,10,26,27,28,31,35,36)
                if (allowableRaces == 0 || allowableRaces == 0xFFFFFFFF)
                {
                    hub.factionMask |= 0x07; // All factions
                }
                else
                {
                    // Simplified faction detection
                    bool hasAlliance = (allowableRaces & 0x0000044D) != 0; // Alliance race bits
                    bool hasHorde = (allowableRaces & 0x000002B2) != 0;    // Horde race bits

                    if (hasAlliance)
                        hub.factionMask |= 0x01;
                    if (hasHorde)
                        hub.factionMask |= 0x02;
                }

            } while (result->NextRow());

            // Assign quest data to hub
            hub.questIds.assign(uniqueQuests.begin(), uniqueQuests.end());
            hub.minLevel = (minLevel != 255) ? minLevel : 1;
            hub.maxLevel = maxLevel;

            // Refine hub name with zone info if available
            if (hub.zoneId > 0)
            {
                // Note: In production, we'd query AreaTable.dbc for zone name
                // For now, use generic name with zone ID
                hub.name = "Quest Hub (Zone " + std::to_string(hub.zoneId) + ")";
            }
        }
    }

    void QuestHubDatabase::BuildSpatialIndex()
    {
        _hubIdToIndex.clear();
        _zoneIndex.clear();

        for (size_t i = 0; i < _questHubs.size(); ++i)
        {
            QuestHub const& hub = _questHubs[i];

            // Build hub ID index
            _hubIdToIndex[hub.hubId] = i;

            // Build zone index
            _zoneIndex[hub.zoneId].push_back(i);
        }
    }

    void QuestHubDatabase::ValidateHubData()
    {
        uint32 warnings = 0;

        for (auto const& hub : _questHubs)
        {
            // Check for empty hubs
            if (hub.questIds.empty())
            {
                TC_LOG_WARN("playerbot", "QuestHubDatabase: Hub {} has no quests", hub.hubId);
                ++warnings;
            }

            // Check for invalid positions
            if (hub.location.GetPositionX() == 0.0f &&
                hub.location.GetPositionY() == 0.0f &&
                hub.location.GetPositionZ() == 0.0f)
            {
                TC_LOG_WARN("playerbot", "QuestHubDatabase: Hub {} has invalid position (0,0,0)", hub.hubId);
                ++warnings;
            }

            // Check for invalid level ranges
            if (hub.minLevel > hub.maxLevel && hub.maxLevel > 0)
            {
                TC_LOG_WARN("playerbot", "QuestHubDatabase: Hub {} has invalid level range ({}-{})",
                           hub.hubId, hub.minLevel, hub.maxLevel);
                ++warnings;
            }

            // Check for no faction access
            if (hub.factionMask == 0)
            {
                TC_LOG_WARN("playerbot", "QuestHubDatabase: Hub {} has no faction access", hub.hubId);
                ++warnings;
            }
        }

        if (warnings > 0)
        {
            TC_LOG_WARN("playerbot", "QuestHubDatabase: Validation found {} warnings", warnings);
        }
        else
        {
            TC_LOG_DEBUG("playerbot", "QuestHubDatabase: Validation passed with no warnings");
        }
    }

} // namespace Playerbot
