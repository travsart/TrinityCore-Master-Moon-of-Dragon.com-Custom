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

#include "BotCharacterDistribution.h"
#include "DatabaseEnv.h"
#include "Database/PlayerbotDatabase.h"
#include "Config/PlayerbotLog.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>

namespace Playerbot
{

BotCharacterDistribution* BotCharacterDistribution::instance()
{
    static BotCharacterDistribution instance;
    return &instance;
}

bool BotCharacterDistribution::LoadFromDatabase()
{
    TC_LOG_PLAYERBOT_CHAR_INFO("Loading Bot Character Distribution from database...");

    auto startTime = getMSTime();

    // Lade alle Verteilungsdaten
    LoadRaceClassDistribution();
    LoadGenderDistribution();
    LoadClassPopularity();
    LoadRaceClassGenderOverrides();

    // Baue kumulative Verteilung für effiziente Zufallsauswahl
    BuildCumulativeDistribution();

    m_loaded = !m_raceClassCombinations.empty();

    TC_LOG_PLAYERBOT_CHAR_INFO(">> Loaded character distribution data in {} ms",
                               GetMSTimeDiffToNow(startTime));
    TC_LOG_PLAYERBOT_CHAR_INFO("   - Race/Class combinations: {}",
                               m_raceClassCombinations.size());
    TC_LOG_PLAYERBOT_CHAR_INFO("   - Gender distributions: {} races",
                               m_genderDistributions.size());
    TC_LOG_PLAYERBOT_CHAR_INFO("   - Class popularities: {} classes",
                               m_classPopularities.size());

    return m_loaded;
}

void BotCharacterDistribution::ReloadDistributions()
{
    TC_LOG_INFO("module.playerbot.character", "Reloading character distributions...");

    // Clear existing data
    m_raceClassCombinations.clear();
    m_genderDistributions.clear();
    m_classPopularities.clear();
    m_raceClassGenderOverrides.clear();
    m_cumulativeDistribution.clear();
    m_raceCache.clear();
    m_classCache.clear();
    m_totalPercentage = 0.0f;
    m_loaded = false;

    // Reload from database
    LoadFromDatabase();
}

void BotCharacterDistribution::LoadRaceClassDistribution()
{
    m_raceClassCombinations.clear();
    m_raceCache.clear();
    m_classCache.clear();

    QueryResult result = sPlayerbotDatabase->Query(
        "SELECT race, class, percentage, is_popular, faction "
        "FROM playerbots_race_class_distribution "
        "ORDER BY percentage DESC"
    );

    if (!result)
    {
        TC_LOG_PLAYERBOT_CHAR_ERROR("No race/class distribution data found!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        RaceClassCombination combo;
        combo.race = fields[0].GetUInt8();
        combo.classId = fields[1].GetUInt8();
        combo.percentage = fields[2].GetFloat();
        combo.isPopular = fields[3].GetBool();
        combo.faction = fields[4].GetString();

        m_raceClassCombinations.push_back(combo);

        // Cache nach Rasse und Klasse für schnellen Zugriff
        m_raceCache[combo.race].push_back(combo);
        m_classCache[combo.classId].push_back(combo);

    } while (result->NextRow());

    TC_LOG_PLAYERBOT_CHAR_DEBUG("Loaded {} race/class combinations",
                                m_raceClassCombinations.size());
}

void BotCharacterDistribution::LoadGenderDistribution()
{
    m_genderDistributions.clear();

    QueryResult result = sPlayerbotDatabase->Query(
        "SELECT race, race_name, male_percentage, female_percentage "
        "FROM playerbots_gender_distribution"
    );

    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.character", "No gender distribution data found!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        GenderDistribution dist;
        dist.race = fields[0].GetUInt8();
        dist.raceName = fields[1].GetString();
        dist.malePercentage = fields[2].GetUInt8();
        dist.femalePercentage = fields[3].GetUInt8();

        m_genderDistributions[dist.race] = dist;

    } while (result->NextRow());

    TC_LOG_DEBUG("module.playerbot.character", "Loaded gender distribution for {} races",
                 m_genderDistributions.size());
}

void BotCharacterDistribution::LoadClassPopularity()
{
    m_classPopularities.clear();

    QueryResult result = sPlayerbotDatabase->Query(
        "SELECT class, class_name, popularity_percentage, "
        "pve_popularity, pvp_popularity, mythic_plus_popularity, raid_popularity "
        "FROM playerbots_class_popularity"
    );

    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.character", "No class popularity data found!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ClassPopularity pop;
        pop.classId = fields[0].GetUInt8();
        pop.className = fields[1].GetString();
        pop.overallPopularity = fields[2].GetFloat();
        pop.pvePopularity = fields[3].GetFloat();
        pop.pvpPopularity = fields[4].GetFloat();
        pop.mythicPlusPopularity = fields[5].GetFloat();
        pop.raidPopularity = fields[6].GetFloat();

        m_classPopularities[pop.classId] = pop;

    } while (result->NextRow());

    TC_LOG_DEBUG("module.playerbot.character", "Loaded popularity data for {} classes",
                 m_classPopularities.size());
}

void BotCharacterDistribution::LoadRaceClassGenderOverrides()
{
    m_raceClassGenderOverrides.clear();

    QueryResult result = sPlayerbotDatabase->Query(
        "SELECT race, class, male_percentage "
        "FROM playerbots_race_class_gender"
    );

    if (!result)
    {
        TC_LOG_DEBUG("module.playerbot.character", "No race/class gender overrides found (optional)");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint8 race = fields[0].GetUInt8();
        uint8 classId = fields[1].GetUInt8();
        uint8 malePercentage = fields[2].GetUInt8();

        uint32 key = (race << 8) | classId;
        m_raceClassGenderOverrides[key] = malePercentage;

    } while (result->NextRow());

    TC_LOG_DEBUG("module.playerbot.character", "Loaded {} race/class gender overrides",
                 m_raceClassGenderOverrides.size());
}

void BotCharacterDistribution::BuildCumulativeDistribution()
{
    m_cumulativeDistribution.clear();
    m_totalPercentage = 0.0f;

    for (const auto& combo : m_raceClassCombinations)
    {
        m_totalPercentage += combo.percentage;
        m_cumulativeDistribution.push_back(m_totalPercentage);
    }

    TC_LOG_DEBUG("module.playerbot.character", "Built cumulative distribution, total percentage: {:.2f}",
                 m_totalPercentage);
}

std::pair<uint8, uint8> BotCharacterDistribution::GetRandomRaceClassByDistribution()
{
    if (m_raceClassCombinations.empty())
    {
        TC_LOG_ERROR("module.playerbot.character", "No race/class distribution data available!");
        return {1, 1}; // Fallback: Human Warrior
    }

    // Zufallszahl zwischen 0 und totalPercentage
    float random = frand(0.0f, m_totalPercentage);

    // Binäre Suche in kumulativer Verteilung
    auto it = std::lower_bound(m_cumulativeDistribution.begin(),
                               m_cumulativeDistribution.end(),
                               random);

    if (it != m_cumulativeDistribution.end())
    {
        size_t index = std::distance(m_cumulativeDistribution.begin(), it);
        if (index < m_raceClassCombinations.size())
        {
            const auto& combo = m_raceClassCombinations[index];
            return {combo.race, combo.classId};
        }
    }

    // Fallback: Erste Kombination
    const auto& combo = m_raceClassCombinations[0];
    return {combo.race, combo.classId};
}

uint8 BotCharacterDistribution::GetRandomGenderForRace(uint8 race)
{
    auto it = m_genderDistributions.find(race);
    if (it == m_genderDistributions.end())
    {
        // 50/50 wenn keine Daten vorhanden
        return urand(0, 100) < 50 ? 0 : 1; // GENDER_MALE : GENDER_FEMALE
    }

    return urand(0, 100) < it->second.malePercentage ? 0 : 1; // GENDER_MALE : GENDER_FEMALE
}

uint8 BotCharacterDistribution::GetRandomGenderForRaceClass(uint8 race, uint8 classId)
{
    // Prüfe zuerst ob es einen spezifischen Override gibt
    uint32 key = (race << 8) | classId;
    auto overrideIt = m_raceClassGenderOverrides.find(key);

    if (overrideIt != m_raceClassGenderOverrides.end())
    {
        return urand(0, 100) < overrideIt->second ? 0 : 1; // GENDER_MALE : GENDER_FEMALE
    }

    // Ansonsten nutze die allgemeine Rassen-Verteilung
    return GetRandomGenderForRace(race);
}

std::vector<RaceClassCombination> BotCharacterDistribution::GetTopCombinations(uint32 limit) const
{
    std::vector<RaceClassCombination> top;

    uint32 count = 0;
    for (const auto& combo : m_raceClassCombinations)
    {
        if (count >= limit)
            break;

        top.push_back(combo);
        count++;
    }

    return top;
}

std::vector<RaceClassCombination> BotCharacterDistribution::GetPopularCombinations() const
{
    std::vector<RaceClassCombination> popular;

    for (const auto& combo : m_raceClassCombinations)
    {
        if (combo.isPopular)
            popular.push_back(combo);
    }

    return popular;
}

uint32 BotCharacterDistribution::GetPopularCombinationsCount() const
{
    uint32 count = 0;
    for (const auto& combo : m_raceClassCombinations)
    {
        if (combo.isPopular)
            count++;
    }
    return count;
}

float BotCharacterDistribution::GetRaceClassPercentage(uint8 race, uint8 classId) const
{
    for (const auto& combo : m_raceClassCombinations)
    {
        if (combo.race == race && combo.classId == classId)
            return combo.percentage;
    }

    return 0.0f;
}

float BotCharacterDistribution::GetClassPopularity(uint8 classId) const
{
    auto it = m_classPopularities.find(classId);
    if (it != m_classPopularities.end())
        return it->second.overallPopularity;

    return 0.0f;
}

uint8 BotCharacterDistribution::GetMalePercentageForRace(uint8 race) const
{
    auto it = m_genderDistributions.find(race);
    if (it != m_genderDistributions.end())
        return it->second.malePercentage;

    return 50; // Default 50/50
}

} // namespace Playerbot