# Phase 2: Bot Character Management - Detailed Implementation Plan

## Executive Summary
**Component**: BotCharacterMgr - Enterprise Character Lifecycle Management  
**Duration**: Weeks 9-10 (2 weeks)  
**Foundation**: Leverages Phase 1 enterprise session infrastructure  
**Scope**: Character creation, naming, equipment, talent distribution, and management  

## Architecture Overview

### Component Integration
```cpp
// Character management leverages existing enterprise infrastructure
BotCharacterMgr
├── Uses: BotSession (Phase 1 - enterprise sessions)
├── Uses: BotSessionMgr (Phase 1 - parallel processing)
├── Uses: BotDatabasePool (Phase 1 - async operations)
├── Uses: BotAccountMgr (Phase 2 - account management)
└── Integrates: Trinity's Player::Create() and CharacterHandler
```

### Performance Targets
- **Character Creation**: <100ms per character (batch operations)
- **Name Generation**: <5ms with uniqueness validation
- **Equipment Assignment**: <10ms using template cache
- **Database Operations**: <10ms P95 (async via BotDatabasePool)
- **Memory Usage**: <50KB per character metadata
- **Concurrent Operations**: 100+ simultaneous creations

## Week 9: Character Creation System

### Day 1-2: BotCharacterMgr Core Implementation

#### File: `src/modules/Playerbot/Character/BotCharacterMgr.h`
```cpp
#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <future>
#include <tbb/concurrent_queue.h>
#include <phmap.h>

namespace Playerbot
{

// Forward declarations
class BotSession;
class BotNameGenerator;
class BotEquipmentMgr;
class BotTalentMgr;

// Character creation request structure
struct CharacterCreateRequest
{
    uint32 accountId = 0;
    uint32 bnetAccountId = 0;
    std::string name;
    uint8 race = 0;
    uint8 classId = 0;
    uint8 gender = 0;
    uint8 level = 1;
    bool randomize = true;
    std::function<void(bool, ObjectGuid)> callback;
};

// Character template for quick spawning
struct CharacterTemplate
{
    uint8 race;
    uint8 classId;
    uint8 gender;
    std::vector<uint32> startingItems;
    std::vector<uint32> talents;
    uint32 startingZone;
    float startX, startY, startZ, startO;
};

class TC_GAME_API BotCharacterMgr
{
    BotCharacterMgr();
    ~BotCharacterMgr();
    BotCharacterMgr(BotCharacterMgr const&) = delete;
    BotCharacterMgr& operator=(BotCharacterMgr const&) = delete;

public:
    static BotCharacterMgr* instance();

    // Initialization
    bool Initialize();
    void Shutdown();

    // Character creation - Enterprise batch operations
    ObjectGuid CreateBotCharacter(CharacterCreateRequest const& request);
    std::vector<ObjectGuid> CreateBotCharactersBatch(
        uint32 accountId, 
        uint32 count, 
        bool randomizeAll = true
    );
    std::future<ObjectGuid> CreateBotCharacterAsync(CharacterCreateRequest const& request);

    // Character management
    bool DeleteBotCharacter(ObjectGuid guid);
    bool RenameBotCharacter(ObjectGuid guid, std::string const& newName);
    bool ChangeBotCharacterFaction(ObjectGuid guid);
    
    // Character queries - Uses cached data
    uint32 GetBotCharacterCount(uint32 accountId) const;
    std::vector<ObjectGuid> GetBotCharacters(uint32 accountId) const;
    bool IsBotCharacter(ObjectGuid guid) const;
    
    // Template management for performance
    void RegisterTemplate(std::string const& name, CharacterTemplate const& tmpl);
    CharacterTemplate const* GetTemplate(std::string const& name) const;
    CharacterTemplate GenerateRandomTemplate(uint8 classId = 0) const;

    // Equipment and customization
    void EquipBotCharacter(ObjectGuid guid, uint8 level);
    void UpdateBotAppearance(ObjectGuid guid);
    
    // Statistics and monitoring
    struct Statistics
    {
        uint32 totalCharactersCreated;
        uint32 charactersActiveToday;
        uint32 averageCreationTimeMs;
        uint32 nameCollisions;
        uint32 templateCacheHits;
        uint32 databaseQueriesMs;
    };
    Statistics GetStatistics() const { return _stats; }

private:
    // Internal implementation
    ObjectGuid CreateCharacterInternal(CharacterCreateRequest const& request);
    bool ValidateCharacterName(std::string const& name, uint8 race) const;
    void LoadTemplates();
    void LoadBotCharacterCache();
    void UpdateCharacterCache(ObjectGuid guid, uint32 accountId);
    void ProcessCreationQueue();
    
    // Name generation with culture awareness
    std::string GenerateUniqueName(uint8 race, uint8 gender);
    
    // Equipment generation based on class/level
    std::vector<uint32> GenerateStartingEquipment(uint8 classId, uint8 level) const;
    
    // Talent distribution based on spec templates
    std::vector<uint32> GenerateTalentBuild(uint8 classId, uint8 level) const;

private:
    // Enterprise data structures
    phmap::parallel_flat_hash_map<uint32, std::vector<ObjectGuid>> _accountCharacters;
    phmap::parallel_flat_hash_map<ObjectGuid::LowType, uint32> _characterAccounts;
    phmap::parallel_flat_hash_map<std::string, CharacterTemplate> _templates;
    
    // Creation queue for batch processing
    tbb::concurrent_queue<CharacterCreateRequest> _creationQueue;
    
    // Sub-managers
    std::unique_ptr<BotNameGenerator> _nameGenerator;
    std::unique_ptr<BotEquipmentMgr> _equipmentMgr;
    std::unique_ptr<BotTalentMgr> _talentMgr;
    
    // Thread pool for async operations
    std::unique_ptr<boost::asio::thread_pool> _workerPool;
    
    // Statistics
    mutable Statistics _stats;
    
    // Configuration
    bool _enableAsyncCreation;
    uint32 _maxCharactersPerAccount;
    uint32 _creationBatchSize;
    uint32 _templateCacheSize;
};

#define sBotCharacterMgr BotCharacterMgr::instance()

} // namespace Playerbot

#### File: `src/modules/Playerbot/Character/BotCharacterMgr.cpp`
```cpp
#include "BotCharacterMgr.h"
#include "BotSession.h"
#include "BotSessionMgr.h"
#include "BotAccountMgr.h"
#include "BotDatabasePool.h"
#include "BotNameGenerator.h"
#include "BotEquipmentMgr.h"
#include "BotTalentMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "CharacterHandler.h"
#include "ObjectMgr.h"
#include "Log.h"
#include <boost/asio.hpp>
#include <execution>

namespace Playerbot
{

BotCharacterMgr::BotCharacterMgr()
    : _enableAsyncCreation(true)
    , _maxCharactersPerAccount(10) // Trinity hardcoded limit
    , _creationBatchSize(10)
    , _templateCacheSize(100)
{
    _nameGenerator = std::make_unique<BotNameGenerator>();
    _equipmentMgr = std::make_unique<BotEquipmentMgr>();
    _talentMgr = std::make_unique<BotTalentMgr>();
    _workerPool = std::make_unique<boost::asio::thread_pool>(4);
}

BotCharacterMgr::~BotCharacterMgr()
{
    Shutdown();
}

BotCharacterMgr* BotCharacterMgr::instance()
{
    static BotCharacterMgr instance;
    return &instance;
}

bool BotCharacterMgr::Initialize()
{
    TC_LOG_INFO("playerbot", "Initializing BotCharacterMgr with enterprise features...");

    // Load configuration
    _enableAsyncCreation = sConfigMgr->GetBoolDefault("Playerbot.Character.AsyncCreation", true);
    _maxCharactersPerAccount = 10; // Trinity limit, cannot be changed
    _creationBatchSize = sConfigMgr->GetIntDefault("Playerbot.Character.BatchSize", 10);
    _templateCacheSize = sConfigMgr->GetIntDefault("Playerbot.Character.TemplateCacheSize", 100);

    // Initialize sub-systems
    if (!_nameGenerator->Initialize())
    {
        TC_LOG_ERROR("playerbot", "Failed to initialize name generator");
        return false;
    }

    if (!_equipmentMgr->Initialize())
    {
        TC_LOG_ERROR("playerbot", "Failed to initialize equipment manager");
        return false;
    }

    if (!_talentMgr->Initialize())
    {
        TC_LOG_ERROR("playerbot", "Failed to initialize talent manager");
        return false;
    }

    // Load templates and cache
    LoadTemplates();
    LoadBotCharacterCache();

    // Start async processing thread
    if (_enableAsyncCreation)
    {
        std::thread([this] { ProcessCreationQueue(); }).detach();
    }

    TC_LOG_INFO("playerbot", "BotCharacterMgr initialized successfully");
    TC_LOG_INFO("playerbot", "  -> Async creation: %s", _enableAsyncCreation ? "enabled" : "disabled");
    TC_LOG_INFO("playerbot", "  -> Batch size: %u", _creationBatchSize);
    TC_LOG_INFO("playerbot", "  -> Template cache: %u", _templateCacheSize);
    TC_LOG_INFO("playerbot", "  -> Loaded %zu character templates", _templates.size());
    TC_LOG_INFO("playerbot", "  -> Cached %zu bot characters", _characterAccounts.size());

    return true;
}

ObjectGuid BotCharacterMgr::CreateBotCharacter(CharacterCreateRequest const& request)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Validate account
    if (!sBotAccountMgr->IsBotAccount(request.accountId))
    {
        TC_LOG_ERROR("playerbot", "CreateBotCharacter: Account %u is not a bot account",
                     request.accountId);
        return ObjectGuid::Empty;
    }

    // Check character limit (Trinity enforces 10 per account)
    if (GetBotCharacterCount(request.accountId) >= _maxCharactersPerAccount)
    {
        TC_LOG_ERROR("playerbot", "CreateBotCharacter: Account %u has reached character limit (%u)",
                     request.accountId, _maxCharactersPerAccount);
        return ObjectGuid::Empty;
    }

    // Prepare creation data
    CharacterCreateRequest createReq = request;

    // Generate name if not provided
    if (createReq.name.empty())
    {
        createReq.name = GenerateUniqueName(createReq.race, createReq.gender);
    }

    // Validate name
    if (!ValidateCharacterName(createReq.name, createReq.race))
    {
        TC_LOG_ERROR("playerbot", "CreateBotCharacter: Invalid name '%s'", createReq.name.c_str());
        return ObjectGuid::Empty;
    }

    // Randomize if requested
    if (createReq.randomize)
    {
        CharacterTemplate tmpl = GenerateRandomTemplate(createReq.classId);
        createReq.race = tmpl.race;
        createReq.classId = tmpl.classId;
        createReq.gender = tmpl.gender;
    }

    // Create character using Trinity's Player::Create
    ObjectGuid guid = CreateCharacterInternal(createReq);

    if (!guid.IsEmpty())
    {
        // Update cache
        UpdateCharacterCache(guid, request.accountId);

        // Equip starting items
        EquipBotCharacter(guid, createReq.level);

        // Apply talents if higher level
        if (createReq.level > 10)
        {
            auto talents = GenerateTalentBuild(createReq.classId, createReq.level);
            _talentMgr->ApplyTalents(guid, talents);
        }

        // Store metadata in playerbot database
        auto stmt = sBotDBPool->GetPreparedStatement(BOT_INS_CHARACTER_META);
        stmt->setUInt64(0, guid.GetRawValue());
        stmt->setUInt32(1, request.accountId);
        stmt->setUInt32(2, request.bnetAccountId);
        stmt->setString(3, createReq.name);
        stmt->setUInt8(4, createReq.race);
        stmt->setUInt8(5, createReq.classId);
        stmt->setUInt8(6, createReq.gender);
        stmt->setUInt8(7, createReq.level);
        sBotDBPool->ExecuteAsync(stmt);

        // Update statistics
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startTime).count();
        _stats.totalCharactersCreated++;
        _stats.averageCreationTimeMs =
            (_stats.averageCreationTimeMs + elapsed) / 2;

        TC_LOG_INFO("playerbot", "Created bot character '%s' (GUID: %s) in %ums",
                    createReq.name.c_str(), guid.ToString().c_str(), elapsed);
    }

    // Execute callback if provided
    if (request.callback)
    {
        request.callback(!guid.IsEmpty(), guid);
    }

    return guid;
}

std::vector<ObjectGuid> BotCharacterMgr::CreateBotCharactersBatch(
    uint32 accountId, uint32 count, bool randomizeAll)
{
    TC_LOG_INFO("playerbot", "Creating batch of %u bot characters for account %u",
                count, accountId);

    std::vector<ObjectGuid> results;
    results.reserve(count);

    // Check how many we can actually create
    uint32 existing = GetBotCharacterCount(accountId);
    uint32 canCreate = std::min(count, _maxCharactersPerAccount - existing);

    if (canCreate == 0)
    {
        TC_LOG_WARN("playerbot", "Account %u already has maximum characters (%u)",
                    accountId, _maxCharactersPerAccount);
        return results;
    }

    // Use parallel execution for batch creation
    std::vector<std::future<ObjectGuid>> futures;
    futures.reserve(canCreate);

    for (uint32 i = 0; i < canCreate; ++i)
    {
        futures.push_back(
            boost::asio::post(*_workerPool, boost::asio::use_future([this, accountId, randomizeAll]()
            {
                CharacterCreateRequest request;
                request.accountId = accountId;
                request.bnetAccountId = sBotAccountMgr->GetBnetAccountId(accountId);
                request.randomize = randomizeAll;

                if (randomizeAll)
                {
                    // Generate random template
                    CharacterTemplate tmpl = GenerateRandomTemplate();
                    request.race = tmpl.race;
                    request.classId = tmpl.classId;
                    request.gender = tmpl.gender;
                }

                return CreateBotCharacter(request);
            })
        );
    }

    // Collect results
    for (auto& future : futures)
    {
        try
        {
            ObjectGuid guid = future.get();
            if (!guid.IsEmpty())
            {
                results.push_back(guid);
            }
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("playerbot", "Batch character creation failed: %s", e.what());
        }
    }

    TC_LOG_INFO("playerbot", "Successfully created %zu/%u bot characters for account %u",
                results.size(), canCreate, accountId);

    return results;
}

ObjectGuid BotCharacterMgr::CreateCharacterInternal(CharacterCreateRequest const& request)
{
    // This uses Trinity's Player::Create internally
    // We create through the character handler to ensure all validations pass

    // Get or create bot session
    BotSession* session = sBotSessionMgr->GetSession(request.accountId);
    if (!session)
    {
        session = sBotSessionMgr->CreateSession(request.accountId);
        if (!session)
        {
            TC_LOG_ERROR("playerbot", "Failed to create session for account %u", request.accountId);
            return ObjectGuid::Empty;
        }
    }

    // Prepare character create info matching Trinity's structure
    CharacterCreateInfo createInfo;
    createInfo.Name = request.name;
    createInfo.Race = request.race;
    createInfo.Class = request.classId;
    createInfo.Gender = request.gender;
    // Appearance customization would go here

    // Use Trinity's character creation through Player::Create
    // This ensures all racial traits, starting spells, etc. are properly set
    std::shared_ptr<Player> newPlayer = std::make_shared<Player>(session);
    if (!newPlayer->Create(sObjectMgr->GetGenerator<HighGuid::Player>().Generate(), &createInfo))
    {
        TC_LOG_ERROR("playerbot", "Player::Create failed for bot character '%s'", request.name.c_str());
        return ObjectGuid::Empty;
    }

    // Set starting level if not 1
    if (request.level > 1)
    {
        newPlayer->SetLevel(request.level);
        newPlayer->InitStatsForLevel(true);
    }

    // Save to database (characters database)
    if (!newPlayer->SaveToDB(true))
    {
        TC_LOG_ERROR("playerbot", "Failed to save bot character '%s' to database", request.name.c_str());
        return ObjectGuid::Empty;
    }

    return newPlayer->GetGUID();
}

std::string BotCharacterMgr::GenerateUniqueName(uint8 race, uint8 gender)
{
    std::string name;
    uint32 attempts = 0;
    const uint32 maxAttempts = 100;

    do
    {
        name = _nameGenerator->GenerateName(race, gender);
        attempts++;

        if (attempts >= maxAttempts)
        {
            // Fallback to numbered name
            name = "Bot" + std::to_string(std::rand() % 100000);
            break;
        }
    }
    while (sCharacterCache->GetCharacterGuidByName(name)); // Check uniqueness

    if (attempts > 1)
    {
        _stats.nameCollisions += (attempts - 1);
    }

    return name;
}

void BotCharacterMgr::LoadBotCharacterCache()
{
    TC_LOG_INFO("playerbot", "Loading bot character cache...");
    auto startTime = std::chrono::high_resolution_clock::now();

    // Load all bot characters from playerbot metadata
    auto result = sBotDBPool->Query("SELECT guid, account_id FROM playerbot_character_meta");
    if (!result)
    {
        TC_LOG_INFO("playerbot", "No bot characters found in database");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());
        uint32 accountId = fields[1].GetUInt32();

        _characterAccounts[guid.GetRawValue()] = accountId;
        _accountCharacters[accountId].push_back(guid);
        count++;
    }
    while (result->NextRow());

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();

    TC_LOG_INFO("playerbot", "Loaded %u bot characters in %ums", count, elapsed);
}

void BotCharacterMgr::ProcessCreationQueue()
{
    while (true)
    {
        CharacterCreateRequest request;
        if (_creationQueue.try_pop(request))
        {
            CreateBotCharacter(request);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace Playerbot
```

### Day 3-4: Name Generation System

#### File: `src/modules/Playerbot/Character/BotNameGenerator.h`
```cpp
#pragma once

#include "Define.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <phmap.h>

namespace Playerbot
{

// Name database structure
struct NameDatabase
{
    std::vector<std::string> prefixes;
    std::vector<std::string> suffixes;
    std::vector<std::string> fullNames;
    bool useCombination = true;  // Combine prefix+suffix vs use full names
};

class TC_GAME_API BotNameGenerator
{
public:
    BotNameGenerator();
    ~BotNameGenerator();

    bool Initialize();
    void Shutdown();

    // Main generation function
    std::string GenerateName(uint8 race, uint8 gender);

    // Batch generation for performance
    std::vector<std::string> GenerateNames(uint8 race, uint8 gender, uint32 count);

    // Validation
    bool IsNameValid(std::string const& name, uint8 race) const;
    bool IsNameAvailable(std::string const& name) const;

    // Database management
    void ReloadNameDatabase();
    void AddCustomName(uint8 race, uint8 gender, std::string const& name);

    // Statistics
    uint32 GetTotalNamesGenerated() const { return _totalGenerated; }
    uint32 GetUniqueNamesInDatabase() const;

private:
    void LoadRaceNames(uint8 race);
    void LoadNamePatternsFromDB();
    void GenerateNameVariations();
    std::string ApplyRaceNamingRules(std::string const& name, uint8 race) const;
    std::string CapitalizeName(std::string const& name) const;

    // Culture-specific generation
    std::string GenerateHumanName(uint8 gender);
    std::string GenerateOrcName(uint8 gender);
    std::string GenerateElfName(uint8 gender, bool night);
    std::string GenerateDwarfName(uint8 gender);
    std::string GenerateUndeadName(uint8 gender);
    std::string GenerateTaurenName(uint8 gender);
    std::string GenerateGnomeName(uint8 gender);
    std::string GenerateTrollName(uint8 gender);
    std::string GenerateDraeneiName(uint8 gender);
    std::string GenerateWorgenName(uint8 gender);
    std::string GenerateGoblinName(uint8 gender);
    std::string GeneratePandarenName(uint8 gender);

private:
    // Race/gender specific name databases
    phmap::parallel_flat_hash_map<uint16, NameDatabase> _nameData; // Key: (race << 8) | gender

    // Cache of recently used names to avoid duplicates
    phmap::parallel_flat_hash_set<std::string> _recentlyUsed;

    // Random generation
    std::mt19937 _rng;

    // Statistics
    std::atomic<uint32> _totalGenerated;

    // Configuration
    uint32 _recentlyUsedCacheSize;
    bool _allowNumberSuffix;
};

} // namespace Playerbot
```

#### File: `src/modules/Playerbot/Character/BotNameGenerator.cpp` (partial)
```cpp
#include "BotNameGenerator.h"
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Util.h"

namespace Playerbot
{

BotNameGenerator::BotNameGenerator()
    : _rng(std::random_device{}())
    , _totalGenerated(0)
    , _recentlyUsedCacheSize(1000)
    , _allowNumberSuffix(false)
{
}

bool BotNameGenerator::Initialize()
{
    TC_LOG_INFO("playerbot", "Initializing BotNameGenerator...");

    // Load configuration
    _recentlyUsedCacheSize = sConfigMgr->GetIntDefault("Playerbot.Name.CacheSize", 1000);
    _allowNumberSuffix = sConfigMgr->GetBoolDefault("Playerbot.Name.AllowNumbers", false);

    // Load name patterns from database
    LoadNamePatternsFromDB();

    // Load race-specific names
    for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
    {
        if (race == RACE_NONE)
            continue;
        LoadRaceNames(race);
    }

    // Generate variations
    GenerateNameVariations();

    TC_LOG_INFO("playerbot", "BotNameGenerator initialized with %u unique name patterns",
                GetUniqueNamesInDatabase());

    return true;
}

std::string BotNameGenerator::GenerateName(uint8 race, uint8 gender)
{
    // Generate culture-appropriate name
    std::string name;

    switch (race)
    {
        case RACE_HUMAN:
            name = GenerateHumanName(gender);
            break;
        case RACE_ORC:
            name = GenerateOrcName(gender);
            break;
        case RACE_DWARF:
            name = GenerateDwarfName(gender);
            break;
        case RACE_NIGHTELF:
            name = GenerateElfName(gender, true);
            break;
        case RACE_UNDEAD_PLAYER:
            name = GenerateUndeadName(gender);
            break;
        case RACE_TAUREN:
            name = GenerateTaurenName(gender);
            break;
        case RACE_GNOME:
            name = GenerateGnomeName(gender);
            break;
        case RACE_TROLL:
            name = GenerateTrollName(gender);
            break;
        case RACE_BLOODELF:
            name = GenerateElfName(gender, false);
            break;
        case RACE_DRAENEI:
            name = GenerateDraeneiName(gender);
            break;
        case RACE_WORGEN:
            name = GenerateWorgenName(gender);
            break;
        case RACE_GOBLIN:
            name = GenerateGoblinName(gender);
            break;
        case RACE_PANDAREN_NEUTRAL:
        case RACE_PANDAREN_ALLIANCE:
        case RACE_PANDAREN_HORDE:
            name = GeneratePandarenName(gender);
            break;
        default:
            // Fallback to human-style names
            name = GenerateHumanName(gender);
            break;
    }

    // Apply race-specific naming rules
    name = ApplyRaceNamingRules(name, race);

    // Ensure proper capitalization
    name = CapitalizeName(name);

    // Add to recently used cache
    _recentlyUsed.insert(name);
    if (_recentlyUsed.size() > _recentlyUsedCacheSize)
    {
        // Remove oldest entries (simple FIFO for now)
        auto it = _recentlyUsed.begin();
        std::advance(it, _recentlyUsed.size() - _recentlyUsedCacheSize);
        _recentlyUsed.erase(_recentlyUsed.begin(), it);
    }

    _totalGenerated++;

    return name;
}

std::string BotNameGenerator::GenerateHumanName(uint8 gender)
{
    uint16 key = (RACE_HUMAN << 8) | gender;
    auto it = _nameData.find(key);
    if (it == _nameData.end())
    {
        // Fallback names
        return gender == GENDER_MALE ? "Marcus" : "Sarah";
    }

    NameDatabase const& db = it->second;

    if (db.useCombination && !db.prefixes.empty() && !db.suffixes.empty())
    {
        // Combine prefix + suffix
        std::uniform_int_distribution<size_t> prefixDist(0, db.prefixes.size() - 1);
        std::uniform_int_distribution<size_t> suffixDist(0, db.suffixes.size() - 1);

        std::string prefix = db.prefixes[prefixDist(_rng)];
        std::string suffix = db.suffixes[suffixDist(_rng)];

        return prefix + suffix;
    }
    else if (!db.fullNames.empty())
    {
        // Use full name
        std::uniform_int_distribution<size_t> nameDist(0, db.fullNames.size() - 1);
        return db.fullNames[nameDist(_rng)];
    }

    // Fallback
    return gender == GENDER_MALE ? "John" : "Jane";
}

// Similar implementations for other races...

void BotNameGenerator::LoadNamePatternsFromDB()
{
    TC_LOG_INFO("playerbot", "Loading name patterns from database...");

    // Load from custom playerbot name database
    auto result = sBotDBPool->Query(
        "SELECT race, gender, type, value FROM playerbot_name_patterns ORDER BY race, gender, type"
    );

    if (!result)
    {
        TC_LOG_WARN("playerbot", "No name patterns found in database, using defaults");
        LoadDefaultNamePatterns();
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint8 race = fields[0].GetUInt8();
        uint8 gender = fields[1].GetUInt8();
        std::string type = fields[2].GetString();
        std::string value = fields[3].GetString();

        uint16 key = (race << 8) | gender;

        if (type == "prefix")
            _nameData[key].prefixes.push_back(value);
        else if (type == "suffix")
            _nameData[key].suffixes.push_back(value);
        else if (type == "full")
            _nameData[key].fullNames.push_back(value);

        count++;
    }
    while (result->NextRow());

    TC_LOG_INFO("playerbot", "Loaded %u name patterns", count);
}

void BotNameGenerator::LoadDefaultNamePatterns()
{
    // Human male names
    uint16 humanMaleKey = (RACE_HUMAN << 8) | GENDER_MALE;
    _nameData[humanMaleKey].prefixes = {"Al", "Ed", "Gar", "Mal", "Nor", "Rob", "Wil"};
    _nameData[humanMaleKey].suffixes = {"bert", "win", "rick", "colm", "man", "ert", "liam"};

    // Human female names
    uint16 humanFemaleKey = (RACE_HUMAN << 8) | GENDER_FEMALE;
    _nameData[humanFemaleKey].prefixes = {"An", "Cath", "El", "Jen", "Mar", "Sar", "Vic"};
    _nameData[humanFemaleKey].suffixes = {"na", "erine", "eanor", "ifer", "garet", "ah", "toria"};

    // Add more races...
}

} // namespace Playerbot
```

## Week 10: Equipment & Customization Systems

### Day 5-6: Equipment Management

#### File: `src/modules/Playerbot/Character/BotEquipmentMgr.h`
```cpp
#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "ItemTemplate.h"
#include <vector>
#include <unordered_map>
#include <phmap.h>

namespace Playerbot
{

// Equipment template for class/level combinations
struct EquipmentTemplate
{
    uint8 classId;
    uint8 minLevel;
    uint8 maxLevel;
    uint8 spec;  // Specialization (0 = any)

    // Item IDs by slot
    std::unordered_map<uint8, std::vector<uint32>> slotItems;

    // Weight for random selection
    uint32 weight = 100;
};

// Equipment set for a character
struct EquipmentSet
{
    std::unordered_map<uint8, uint32> items;  // slot -> itemId
    uint32 itemLevel = 0;
    uint32 setId = 0;  // For tier sets
};

class TC_GAME_API BotEquipmentMgr
{
public:
    BotEquipmentMgr();
    ~BotEquipmentMgr();

    bool Initialize();
    void Shutdown();

    // Main equipment functions
    bool EquipCharacter(ObjectGuid guid, uint8 level, uint8 spec = 0);
    bool EquipCharacterWithTemplate(ObjectGuid guid, std::string const& templateName);
    bool EquipCharacterWithSet(ObjectGuid guid, EquipmentSet const& set);

    // Equipment generation
    EquipmentSet GenerateEquipmentSet(uint8 classId, uint8 level, uint8 spec = 0);
    EquipmentSet GenerateRandomEquipmentSet(uint8 classId, uint8 level);
    EquipmentSet GeneratePvPEquipmentSet(uint8 classId, uint8 level);
    EquipmentSet GeneratePvEEquipmentSet(uint8 classId, uint8 level);

    // Template management
    void LoadEquipmentTemplates();
    void RegisterTemplate(std::string const& name, EquipmentTemplate const& tmpl);
    EquipmentTemplate const* GetTemplate(std::string const& name) const;
    std::vector<EquipmentTemplate const*> GetTemplatesForClass(uint8 classId, uint8 level) const;

    // Item evaluation
    uint32 CalculateItemScore(ItemTemplate const* proto, uint8 classId, uint8 spec) const;
    bool IsItemSuitableForClass(ItemTemplate const* proto, uint8 classId) const;
    bool IsItemSuitableForLevel(ItemTemplate const* proto, uint8 level) const;

    // Upgrade functions
    bool UpgradeEquipment(ObjectGuid guid);
    bool UpgradeSlot(ObjectGuid guid, uint8 slot);

    // Statistics
    struct Statistics
    {
        uint32 totalEquipped;
        uint32 templatesLoaded;
        uint32 averageItemLevel;
        uint32 upgradesPerformed;
    };
    Statistics GetStatistics() const { return _stats; }

private:
    // Internal equipment logic
    bool EquipItem(Player* player, uint32 itemId, uint8 slot);
    void UnequipItem(Player* player, uint8 slot);
    uint32 SelectBestItem(std::vector<uint32> const& items, uint8 classId, uint8 level, uint8 spec);

    // Stat weight calculation for different specs
    float GetStatWeight(uint32 statType, uint8 classId, uint8 spec) const;

    // Item database queries
    std::vector<uint32> GetItemsForSlot(uint8 slot, uint8 classId, uint8 minLevel, uint8 maxLevel);
    void CacheClassItems(uint8 classId);

private:
    // Template storage
    phmap::parallel_flat_hash_map<std::string, EquipmentTemplate> _templates;
    phmap::parallel_flat_hash_map<uint8, std::vector<EquipmentTemplate>> _classTemplates;

    // Item cache by class and level
    phmap::parallel_flat_hash_map<uint32, std::vector<uint32>> _itemCache; // key: (class << 16) | level

    // Stat weights by class/spec
    phmap::parallel_flat_hash_map<uint16, std::unordered_map<uint32, float>> _statWeights;

    // Statistics
    mutable Statistics _stats;

    // Configuration
    bool _useRandomEnchants;
    bool _useGems;
    uint32 _itemQualityThreshold;
};

} // namespace Playerbot
```

### Day 7: Talent Management System

#### File: `src/modules/Playerbot/Character/BotTalentMgr.h`
```cpp
#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <vector>
#include <unordered_map>
#include <phmap.h>

namespace Playerbot
{

// Talent build template
struct TalentBuild
{
    uint8 classId;
    uint8 spec;
    uint8 minLevel;
    uint8 maxLevel;
    std::string name;
    std::string description;

    // Talent IDs in order of priority
    std::vector<uint32> talents;

    // PvP or PvE focus
    enum Focus { PVE, PVP, HYBRID };
    Focus focus = PVE;

    // Build weight for random selection
    uint32 weight = 100;
};

class TC_GAME_API BotTalentMgr
{
public:
    BotTalentMgr();
    ~BotTalentMgr();

    bool Initialize();
    void Shutdown();

    // Apply talents to character
    bool ApplyTalents(ObjectGuid guid, std::vector<uint32> const& talentIds);
    bool ApplyTalentBuild(ObjectGuid guid, TalentBuild const& build);
    bool ApplyRandomBuild(ObjectGuid guid, uint8 level);

    // Build management
    void LoadTalentBuilds();
    void RegisterBuild(TalentBuild const& build);
    TalentBuild const* GetBuild(std::string const& name) const;
    std::vector<TalentBuild const*> GetBuildsForClass(uint8 classId, uint8 level) const;

    // Build generation
    TalentBuild GenerateBuild(uint8 classId, uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GeneratePvPBuild(uint8 classId, uint8 spec, uint8 level);
    TalentBuild GeneratePvEBuild(uint8 classId, uint8 spec, uint8 level);

    // Reset and respec
    bool ResetTalents(ObjectGuid guid);
    bool RespecCharacter(ObjectGuid guid, uint8 newSpec);

private:
    // Internal talent logic
    bool LearnTalent(Player* player, uint32 talentId);
    bool ValidateTalentRequirements(Player* player, uint32 talentId);
    uint32 GetTalentPoints(uint8 level) const;

    // Build selection
    TalentBuild const* SelectBuildForCharacter(Player* player) const;

    // Class-specific build generation
    TalentBuild GenerateWarriorBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GeneratePaladinBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateHunterBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateRogueBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GeneratePriestBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateDeathKnightBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateShamanBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateMageBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    TalentBuild GenerateWarlockBuild(uint8 spec, uint8 level, TalentBuild::Focus focus);
    // Add other classes...

private:
    // Build storage
    phmap::parallel_flat_hash_map<std::string, TalentBuild> _builds;
    phmap::parallel_flat_hash_map<uint8, std::vector<TalentBuild>> _classBuilds;

    // Statistics
    std::atomic<uint32> _totalApplications;
    std::atomic<uint32> _totalResets;
};

} // namespace Playerbot
```

## Database Schema

### SQL Schema: `sql/playerbot/002_character_management.sql`
```sql
-- Bot character metadata (extends Trinity's characters)
CREATE TABLE IF NOT EXISTS `playerbot_character_meta` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `account_id` INT UNSIGNED NOT NULL,
    `bnet_account_id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(50) NOT NULL,
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `gender` TINYINT UNSIGNED NOT NULL,
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `strategy` VARCHAR(50) DEFAULT 'default',
    `ai_state` JSON,
    `equipment_template` VARCHAR(50) DEFAULT NULL,
    `talent_build` VARCHAR(50) DEFAULT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `last_login` TIMESTAMP NULL DEFAULT NULL,
    `play_time` INT UNSIGNED DEFAULT 0,
    PRIMARY KEY (`guid`),
    KEY `idx_account` (`account_id`),
    KEY `idx_name` (`name`),
    KEY `idx_class_level` (`class`, `level`),
    CONSTRAINT `fk_char_account` FOREIGN KEY (`account_id`)
        REFERENCES `playerbot_account_meta` (`account_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Name generation patterns
CREATE TABLE IF NOT EXISTS `playerbot_name_patterns` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `race` TINYINT UNSIGNED NOT NULL,
    `gender` TINYINT UNSIGNED NOT NULL,
    `type` ENUM('prefix', 'suffix', 'full') NOT NULL,
    `value` VARCHAR(50) NOT NULL,
    `culture` VARCHAR(20) DEFAULT NULL,
    `weight` INT UNSIGNED DEFAULT 100,
    PRIMARY KEY (`id`),
    KEY `idx_race_gender` (`race`, `gender`, `type`),
    UNIQUE KEY `uk_name_pattern` (`race`, `gender`, `type`, `value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Equipment templates
CREATE TABLE IF NOT EXISTS `playerbot_equipment_templates` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(50) NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED DEFAULT 0,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `slot` TINYINT UNSIGNED NOT NULL,
    `item_id` INT UNSIGNED NOT NULL,
    `weight` INT UNSIGNED DEFAULT 100,
    PRIMARY KEY (`id`),
    KEY `idx_template` (`name`, `class`, `min_level`),
    KEY `idx_class_level` (`class`, `min_level`, `max_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Talent builds
CREATE TABLE IF NOT EXISTS `playerbot_talent_builds` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(50) NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 10,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `talents` JSON NOT NULL,
    `focus` ENUM('PVE', 'PVP', 'HYBRID') DEFAULT 'PVE',
    `description` TEXT,
    `weight` INT UNSIGNED DEFAULT 100,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_build_name` (`name`),
    KEY `idx_class_spec` (`class`, `spec`, `min_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Character statistics
CREATE TABLE IF NOT EXISTS `playerbot_character_stats` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `total_kills` INT UNSIGNED DEFAULT 0,
    `total_deaths` INT UNSIGNED DEFAULT 0,
    `quests_completed` INT UNSIGNED DEFAULT 0,
    `dungeons_completed` INT UNSIGNED DEFAULT 0,
    `pvp_kills` INT UNSIGNED DEFAULT 0,
    `pvp_deaths` INT UNSIGNED DEFAULT 0,
    `gold_earned` BIGINT UNSIGNED DEFAULT 0,
    `items_looted` INT UNSIGNED DEFAULT 0,
    `last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`guid`),
    CONSTRAINT `fk_stats_char` FOREIGN KEY (`guid`)
        REFERENCES `playerbot_character_meta` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

## Testing Suite

### File: `src/modules/Playerbot/Tests/CharacterManagementTest.cpp`
```cpp
#include "catch.hpp"
#include "BotCharacterMgr.h"
#include "BotAccountMgr.h"
#include "BotNameGenerator.h"
#include "BotEquipmentMgr.h"
#include "BotTalentMgr.h"
#include "TestHelpers.h"

using namespace Playerbot;

TEST_CASE("BotCharacterMgr", "[character][integration]")
{
    SECTION("Character Creation")
    {
        // Setup
        uint32 accountId = TestHelpers::CreateTestBotAccount();
        REQUIRE(accountId > 0);

        SECTION("Creates character successfully")
        {
            CharacterCreateRequest request;
            request.accountId = accountId;
            request.race = RACE_HUMAN;
            request.classId = CLASS_WARRIOR;
            request.gender = GENDER_MALE;
            request.name = "Testbot";

            ObjectGuid guid = sBotCharacterMgr->CreateBotCharacter(request);
            REQUIRE(!guid.IsEmpty());
            REQUIRE(sBotCharacterMgr->IsBotCharacter(guid));
        }

        SECTION("Generates unique name when not provided")
        {
            CharacterCreateRequest request;
            request.accountId = accountId;
            request.race = RACE_HUMAN;
            request.classId = CLASS_WARRIOR;
            request.gender = GENDER_MALE;
            // No name provided

            ObjectGuid guid = sBotCharacterMgr->CreateBotCharacter(request);
            REQUIRE(!guid.IsEmpty());
        }

        SECTION("Respects 10 character limit")
        {
            // Create 10 characters
            for (int i = 0; i < 10; ++i)
            {
                CharacterCreateRequest request;
                request.accountId = accountId;
                request.randomize = true;
                ObjectGuid guid = sBotCharacterMgr->CreateBotCharacter(request);
                REQUIRE(!guid.IsEmpty());
            }

            // 11th should fail
            CharacterCreateRequest request;
            request.accountId = accountId;
            request.randomize = true;
            ObjectGuid guid = sBotCharacterMgr->CreateBotCharacter(request);
            REQUIRE(guid.IsEmpty());
        }

        SECTION("Batch creation works")
        {
            auto guids = sBotCharacterMgr->CreateBotCharactersBatch(accountId, 5, true);
            REQUIRE(guids.size() == 5);
            for (auto const& guid : guids)
            {
                REQUIRE(!guid.IsEmpty());
                REQUIRE(sBotCharacterMgr->IsBotCharacter(guid));
            }
        }
    }

    SECTION("Name Generation")
    {
        BotNameGenerator generator;
        REQUIRE(generator.Initialize());

        SECTION("Generates valid names for all races")
        {
            for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
            {
                if (race == RACE_NONE)
                    continue;

                for (uint8 gender = GENDER_MALE; gender <= GENDER_FEMALE; ++gender)
                {
                    std::string name = generator.GenerateName(race, gender);
                    REQUIRE(!name.empty());
                    REQUIRE(name.length() >= 2);
                    REQUIRE(name.length() <= 12);
                    REQUIRE(generator.IsNameValid(name, race));
                }
            }
        }

        SECTION("Generates unique names in batch")
        {
            auto names = generator.GenerateNames(RACE_HUMAN, GENDER_MALE, 100);
            REQUIRE(names.size() == 100);

            std::set<std::string> uniqueNames(names.begin(), names.end());
            REQUIRE(uniqueNames.size() > 90); // Allow some duplicates
        }
    }

    SECTION("Equipment Management")
    {
        BotEquipmentMgr equipMgr;
        REQUIRE(equipMgr.Initialize());

        SECTION("Generates appropriate equipment for class/level")
        {
            for (uint8 classId = CLASS_WARRIOR; classId < MAX_CLASSES; ++classId)
            {
                EquipmentSet set = equipMgr.GenerateEquipmentSet(classId, 60);
                REQUIRE(!set.items.empty());
                REQUIRE(set.itemLevel > 0);
            }
        }

        SECTION("PvP vs PvE equipment differs")
        {
            EquipmentSet pvpSet = equipMgr.GeneratePvPEquipmentSet(CLASS_WARRIOR, 70);
            EquipmentSet pveSet = equipMgr.GeneratePvEEquipmentSet(CLASS_WARRIOR, 70);

            // Should have different items
            bool hasDifference = false;
            for (auto const& [slot, itemId] : pvpSet.items)
            {
                if (pveSet.items.count(slot) && pveSet.items.at(slot) != itemId)
                {
                    hasDifference = true;
                    break;
                }
            }
            REQUIRE(hasDifference);
        }
    }

    SECTION("Talent Management")
    {
        BotTalentMgr talentMgr;
        REQUIRE(talentMgr.Initialize());

        SECTION("Generates valid talent builds")
        {
            for (uint8 classId = CLASS_WARRIOR; classId < MAX_CLASSES; ++classId)
            {
                TalentBuild build = talentMgr.GeneratePvEBuild(classId, 1, 80);
                REQUIRE(!build.talents.empty());
                REQUIRE(build.classId == classId);
            }
        }
    }
}

TEST_CASE("Performance", "[character][performance]")
{
    SECTION("Character creation performance")
    {
        uint32 accountId = TestHelpers::CreateTestBotAccount();

        auto start = std::chrono::high_resolution_clock::now();

        CharacterCreateRequest request;
        request.accountId = accountId;
        request.randomize = true;

        ObjectGuid guid = sBotCharacterMgr->CreateBotCharacter(request);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();

        REQUIRE(!guid.IsEmpty());
        REQUIRE(elapsed < 100); // Should complete in under 100ms
    }

    SECTION("Batch creation performance")
    {
        uint32 accountId = TestHelpers::CreateTestBotAccount();

        auto start = std::chrono::high_resolution_clock::now();

        auto guids = sBotCharacterMgr->CreateBotCharactersBatch(accountId, 10, true);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();

        REQUIRE(guids.size() == 10);
        REQUIRE(elapsed < 500); // 10 characters in under 500ms
    }
}
```

## Implementation Checklist

### Week 9 Tasks
- [ ] Implement BotCharacterMgr core class
- [ ] Integrate with Trinity's Player::Create()
- [ ] Implement BotNameGenerator with cultural awareness
- [ ] Create name pattern database
- [ ] Implement character caching system
- [ ] Add batch creation with parallel processing
- [ ] Create character metadata tables
- [ ] Write comprehensive unit tests

### Week 10 Tasks
- [ ] Implement BotEquipmentMgr
- [ ] Create equipment template system
- [ ] Implement stat weight calculations
- [ ] Implement BotTalentMgr
- [ ] Create talent build templates
- [ ] Add PvP/PvE build differentiation
- [ ] Performance testing and optimization
- [ ] Integration testing with account management

## Performance Optimizations

### Implemented Optimizations
1. **Parallel Creation**: Uses Intel TBB for batch character creation
2. **Name Caching**: Recently used names cached to avoid database queries
3. **Template System**: Pre-defined templates for quick character generation
4. **Async Database**: All metadata stored asynchronously via BotDatabasePool
5. **Batch Processing**: Creation queue for efficient bulk operations

### Memory Management
- Character metadata: ~50KB per character
- Name cache: Limited to 1000 entries
- Template cache: Limited to 100 templates
- Total overhead: <100MB for 1000 characters

## Integration Points

### With Phase 1 Components
```cpp
// Uses enterprise session from Phase 1
BotSession* session = sBotSessionMgr->GetSession(accountId);

// Uses async database from Phase 1
sBotDBPool->ExecuteAsync(stmt, callback);

// Leverages parallel processing
phmap::parallel_flat_hash_map for all caches
```

### With Phase 2 Account Management
```cpp
// Validates bot accounts
if (!sBotAccountMgr->IsBotAccount(accountId))
    return false;

// Gets BattleNet account ID
uint32 bnetId = sBotAccountMgr->GetBnetAccountId(accountId);
```

## Next Steps

After completing Week 9-10 character management:
1. Proceed to Bot Lifecycle Management (Week 11)
2. Integrate spawning and scheduling systems
3. Begin AI Framework implementation (Week 12-14)

---

**Status**: Ready for implementation
**Dependencies**: Phase 1 ✅, Account Management ✅
**Estimated Completion**: 2 weeks
```