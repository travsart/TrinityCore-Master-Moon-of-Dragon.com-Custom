/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotCharacterCreator.h"
#include "CharacterCache.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "LoginDatabase.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "Data/WoW112CharacterCreation.h"
#include "CharacterPackets.h"
#include <random>

namespace Playerbot
{

// ===================================================================================
// PUBLIC API IMPLEMENTATION
// ===================================================================================

BotCharacterCreator::CreateResult BotCharacterCreator::CreateBotCharacter(
    uint32 accountId,
    uint8 race,
    uint8 classId,
    uint8 gender,
    std::string const& name,
    ObjectGuid& outGuid,
    std::string& outErrorMsg)
{
    // Phase 1: Validation
    CreateResult validationResult = ValidateCreationRequest(
        accountId, race, classId, gender, name, outErrorMsg);

    if (validationResult != CreateResult::SUCCESS)
        return validationResult;

    // Phase 2: Create Player object
    return CreatePlayerObject(accountId, race, classId, gender, name, outGuid, outErrorMsg);
}

bool BotCharacterCreator::IsValidRaceClassCombination(uint8 race, uint8 classId)
{
    // Check using WoW 11.2 data
    for (auto const& combo : CharacterCreation::VALID_COMBINATIONS)
    {
        if (combo.race == static_cast<Races>(race) &&
            combo.playerClass == static_cast<Classes>(classId))
            return true;
    }
    return false;
}

bool BotCharacterCreator::CanCreateCharacter(uint32 accountId, uint32& currentCount)
{
    // Query current character count for account
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SUM_CHARS);
    stmt->setUInt32(0, accountId);

    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    currentCount = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        currentCount = fields[0].GetUInt32();
    }

    uint32 maxChars = sWorld->getIntConfig(CONFIG_CHARACTERS_PER_REALM);
    return currentCount < maxChars;
}

std::string BotCharacterCreator::GenerateDefaultBotName(uint8 race, uint8 gender)
{
    // Simple name generator - can be enhanced with race-specific names
    static const std::vector<std::string> prefixes = {
        "Bot", "AI", "Auto", "Servo", "Mech", "Cyber", "Droid"
    };

    static const std::vector<std::string> suffixes = {
        "warrior", "mage", "hunter", "rogue", "priest", "knight", "slayer"
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> prefixDist(0, prefixes.size() - 1);
    std::uniform_int_distribution<> suffixDist(0, suffixes.size() - 1);
    std::uniform_int_distribution<> numberDist(1, 9999);

    std::string name = prefixes[prefixDist(gen)] +
                      suffixes[suffixDist(gen)] +
                      std::to_string(numberDist(gen));

    // Capitalize first letter
    if (!name.empty())
        name[0] = std::toupper(name[0]);

    return name;
}

uint8 BotCharacterCreator::GetStartingLevel(uint8 race, uint8 classId)
{
    if (classId == CLASS_EVOKER)
        return CharacterCreation::EVOKER_STARTING_LEVEL;

    if (classId == CLASS_DEATH_KNIGHT)
        return CharacterCreation::DEATH_KNIGHT_STARTING_LEVEL;

    if (classId == CLASS_DEMON_HUNTER)
        return CharacterCreation::DEMON_HUNTER_STARTING_LEVEL;

    // Check if allied race
    for (auto const& combo : CharacterCreation::VALID_COMBINATIONS)
    {
        if (combo.race == static_cast<Races>(race) && combo.isAlliedRace)
            return CharacterCreation::ALLIED_RACE_STARTING_LEVEL;
    }

    return CharacterCreation::DEFAULT_STARTING_LEVEL;
}

char const* BotCharacterCreator::ResultToString(CreateResult result)
{
    switch (result)
    {
        case CreateResult::SUCCESS: return "Success";
        case CreateResult::INVALID_RACE_CLASS_COMBO: return "Invalid race/class combination";
        case CreateResult::INVALID_NAME: return "Invalid character name";
        case CreateResult::NAME_IN_USE: return "Name already in use";
        case CreateResult::NAME_RESERVED: return "Name is reserved";
        case CreateResult::ACCOUNT_LIMIT: return "Account character limit reached";
        case CreateResult::REALM_LIMIT: return "Realm character limit reached";
        case CreateResult::EVOKER_LIMIT: return "Evoker limit reached for this realm";
        case CreateResult::DEMON_HUNTER_LIMIT: return "Demon Hunter limit reached for this realm";
        case CreateResult::DATABASE_ERROR: return "Database error";
        case CreateResult::CREATION_FAILED: return "Character creation failed";
        default: return "Unknown error";
    }
}

// ===================================================================================
// PRIVATE IMPLEMENTATION
// ===================================================================================

BotCharacterCreator::CreateResult BotCharacterCreator::ValidateCreationRequest(
    uint32 accountId,
    uint8 race,
    uint8 classId,
    uint8 gender,
    std::string const& name,
    std::string& outErrorMsg)
{
    // 1. Validate race/class combination
    if (!IsValidRaceClassCombination(race, classId))
    {
        outErrorMsg = "Invalid race/class combination for WoW 11.2";
        return CreateResult::INVALID_RACE_CLASS_COMBO;
    }

    // 2. Validate gender
    if (gender != GENDER_MALE && gender != GENDER_FEMALE)
    {
        outErrorMsg = "Invalid gender (must be GENDER_MALE or GENDER_FEMALE)";
        return CreateResult::INVALID_NAME;
    }

    // 3. Validate name
    std::string nameToCheck = name;
    if (!normalizePlayerName(nameToCheck))
    {
        outErrorMsg = "Character name is empty or invalid";
        return CreateResult::INVALID_NAME;
    }

    ResponseCodes nameCheck = ObjectMgr::CheckPlayerName(nameToCheck, DEFAULT_LOCALE, true);
    if (nameCheck != CHAR_NAME_SUCCESS)
    {
        outErrorMsg = "Character name validation failed";
        return CreateResult::INVALID_NAME;
    }

    // 4. Check if name is reserved
    if (sObjectMgr->IsReservedName(nameToCheck))
    {
        outErrorMsg = "Character name is reserved";
        return CreateResult::NAME_RESERVED;
    }

    // 5. Check if name already exists
    if (sCharacterCache->GetCharacterCacheByName(nameToCheck))
    {
        outErrorMsg = "Character name already in use";
        return CreateResult::NAME_IN_USE;
    }

    // 6. Check account character limit
    uint32 currentCount = 0;
    if (!CanCreateCharacter(accountId, currentCount))
    {
        outErrorMsg = "Account has reached character limit (" +
                     std::to_string(sWorld->getIntConfig(CONFIG_CHARACTERS_PER_REALM)) + ")";
        return CreateResult::REALM_LIMIT;
    }

    // 7. Check hero class limits (Evoker/DH)
    if (classId == CLASS_EVOKER || classId == CLASS_DEMON_HUNTER)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_CREATE_INFO);
        stmt->setUInt32(0, accountId);
        stmt->setUInt32(1, 1200); // Max chars per realm + deleted

        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (result)
        {
            uint32 evokerCount = 0;
            uint32 dhCount = 0;

            do
            {
                Field* fields = result->Fetch();
                uint8 accClass = fields[2].GetUInt8();

                if (accClass == CLASS_EVOKER)
                    ++evokerCount;
                else if (accClass == CLASS_DEMON_HUNTER)
                    ++dhCount;

            } while (result->NextRow());

            if (classId == CLASS_EVOKER && evokerCount >= CharacterCreation::MAX_EVOKERS_PER_REALM)
            {
                outErrorMsg = "Maximum Evokers per realm reached";
                return CreateResult::EVOKER_LIMIT;
            }

            if (classId == CLASS_DEMON_HUNTER && dhCount >= CharacterCreation::MAX_DEMON_HUNTERS_PER_REALM)
            {
                outErrorMsg = "Maximum Demon Hunters per realm reached";
                return CreateResult::DEMON_HUNTER_LIMIT;
            }
        }
    }

    return CreateResult::SUCCESS;
}

BotCharacterCreator::CreateResult BotCharacterCreator::CreatePlayerObject(
    uint32 accountId,
    uint8 race,
    uint8 classId,
    uint8 gender,
    std::string const& name,
    ObjectGuid& outGuid,
    std::string& outErrorMsg)
{
    // Create a minimal WorldSession for character creation
    // Note: This is a temporary session just for the creation process
    std::shared_ptr<WorldSession> tempSession = std::make_shared<WorldSession>(
        accountId,                                  // Account ID
        std::string(""),                            // Account name (battle tag)
        0,                                          // Battle.net account ID
        nullptr,                                    // No socket
        AccountTypes::SEC_PLAYER,                   // Security level
        CURRENT_EXPANSION,                          // Expansion
        0,                                          // Mute time
        "",                                         // OS string
        std::chrono::minutes(0),                    // Timezone offset
        0,                                          // Build
        ClientBuild::VariantId{},                   // Client build variant
        DEFAULT_LOCALE,                             // Locale
        0,                                          // Recruiter ID
        false,                                      // Is AR recruiter
        true                                        // is_bot flag
    );

    // Build CharacterCreateInfo structure
    auto createInfo = std::make_shared<WorldPackets::Character::CharacterCreateInfo>();
    createInfo->Race = race;
    createInfo->Class = classId;
    createInfo->Sex = gender;
    createInfo->Name = name;
    createInfo->IsTrialBoost = false;
    createInfo->UseNPE = false;
    createInfo->HardcoreSelfFound = false;
    createInfo->TimerunningSeasonID = 0;

    // Initialize customizations (empty for bots - can be enhanced later)
    // Note: Customizations array is already default-initialized by CharacterCreateInfo constructor

    // Create Player object
    std::shared_ptr<Player> newChar(new Player(tempSession.get()), [](Player* ptr)
    {
        ptr->CleanupsBeforeDelete();
        delete ptr;
    });

    newChar->GetMotionMaster()->Initialize();

    // Generate new character GUID
    ObjectGuid::LowType guidLow = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();

    // Call Player::Create
    if (!newChar->Create(guidLow, createInfo.get()))
    {
        outErrorMsg = "Player::Create failed (race/class problem or database issue)";
        TC_LOG_ERROR("module.playerbot", "BotCharacterCreator: Player::Create failed for bot '{}' (race: {}, class: {})",
            name, race, classId);
        return CreateResult::CREATION_FAILED;
    }

    // Set character as bot (skip cinematics)
    newChar->setCinematic(1);

    // Set first login flag
    newChar->SetAtLoginFlag(AT_LOGIN_FIRST);

    // NOTE: Specialization spells are NOT saved to database in modern WoW
    // They are learned dynamically from DB2 data on each login via LearnSpecializationSpells()
    // This is by design - see Player::LoadFromDB() line 18356

    // Prepare database transactions
    CharacterDatabaseTransaction charTransaction = CharacterDatabase.BeginTransaction();
    LoginDatabaseTransaction loginTransaction = LoginDatabase.BeginTransaction();

    try
    {
        // Save character to database
        newChar->SaveToDB(loginTransaction, charTransaction, true);

        // Update realm character count
        LoginDatabasePreparedStatement* loginStmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_REALM_CHARACTERS);
        uint32 currentCount = 0;
        CanCreateCharacter(accountId, currentCount);
        loginStmt->setUInt32(0, currentCount + 1);
        loginStmt->setUInt32(1, accountId);
        loginStmt->setUInt32(2, sRealmList->GetCurrentRealmId().Realm);
        loginTransaction->Append(loginStmt);

        // Commit transactions synchronously (we need immediate result)
        CharacterDatabase.DirectCommitTransaction(charTransaction);
        LoginDatabase.DirectCommitTransaction(loginTransaction);

        // Add to character cache
        sCharacterCache->AddCharacterCacheEntry(
            newChar->GetGUID(),
            accountId,
            newChar->GetName(),
            newChar->GetNativeGender(),
            newChar->GetRace(),
            newChar->GetClass(),
            newChar->GetLevel(),
            false // Not deleted
        );

        // Success - return GUID
        outGuid = newChar->GetGUID();

        TC_LOG_INFO("module.playerbot", "BotCharacterCreator: Successfully created bot character '{}' (GUID: {}, Race: {}, Class: {}, Level: {})",
            name, outGuid.ToString(), race, classId, newChar->GetLevel());

        // Call script hook
        sScriptMgr->OnPlayerCreate(newChar.get());

        return CreateResult::SUCCESS;
    }
    catch (...)
    {
        outErrorMsg = "Database transaction failed during character creation";
        TC_LOG_ERROR("module.playerbot", "BotCharacterCreator: Database exception during bot creation for '{}'", name);
        return CreateResult::DATABASE_ERROR;
    }
}

} // namespace Playerbot
