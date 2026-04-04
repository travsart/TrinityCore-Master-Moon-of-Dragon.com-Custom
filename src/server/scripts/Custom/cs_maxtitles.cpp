#include "AchievementMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DB2Stores.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"

using namespace Trinity::ChatCommands;
namespace
{
    bool IsReputationAchievement(AchievementEntry const* achievement)
    {
        int16 category = achievement->Category;
        while (true)
        {
            // Main Reputation root (201), FoS Reputation (15273), or Argent Tournament (14941)
            if (category == 201 || category == 15273 || category == 14941)
                return true;

            Achievement_CategoryEntry const* catEntry = sAchievementCategoryStore.LookupEntry(category);
            if (!catEntry || catEntry->Parent == -1)
                break;

            category = catEntry->Parent;
        }
        return false;
    }

    // Renown milestone titles — the renown system doesn't auto-grant these
    static constexpr uint32 renownTitleIds[] =
    {
        // Shadowlands Covenant Renown 40
        678,  // Hand of the Archon (Kyrian)
        679,  // Baron (Necrolord, male)
        680,  // Baroness (Necrolord, female)
        681,  // Winter's Envoy (Night Fae)
        682,  // Count (Venthyr, male)
        683,  // Countess (Venthyr, female)
        // Shadowlands Covenant Renown 80
        699,  // Protector of the Weald (Night Fae)
        700,  // Sword of the Primus (Necrolord)
        701,  // Sin Eater (Venthyr)
        702,  // Disciple of Devotion (Kyrian)
        // Dragonflight Renown
        734,  // Khansguard (Maruuk Centaur)
        735,  // Ally of Dragons (Valdrakken Accord)
        736,  // Intrepid Explorer (Dragonscale Expedition)
        737,  // of Iskaara (Iskaara Tuskarr)
        768,  // Smelly (Loamm Niffen)
        801,  // Dream Defender (Dream Wardens)
        810,  // Plunderlord (Plunderstorm)
        // The War Within Renown
        839,  // Machine Whisperer (Assembly of the Deeps)
        840,  // Honorary Councilmember (Council of Dornogal)
        841,  // Lamplighter (Hallowfall Arathi)
        842,  // Thread-Spinner (Severed Threads)
        924,  // of the Twilight Star (K'aresh Trust)
        // Midnight Renown
        935,  // Loa-Speaker (Amani Tribe)
        1020, // Honorary Hara'ti (Hara'ti)
        1026, // Life of the Party (Silvermoon Court)
        1212, // the Singular (The Singularity)
    };
}

class maxtitles_commandscript : public CommandScript
{
public:
    maxtitles_commandscript() : CommandScript("maxtitles_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "maxtitles", HandleMaxTitlesCommand, rbac::RBAC_PERM_COMMAND_MAXTITLES, Console::No },
        };
        return commandTable;
    }

    static bool HandleMaxTitlesCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 newCount = 0;
        uint32 foundCount = 0;
        int factionIdx = Player::TeamForRace(player->GetRace()) == ALLIANCE ? 0 : 1;

        // Part 1: Achievement-based reputation titles
        for (AchievementEntry const* achievement : sAchievementStore)
        {
            if (!IsReputationAchievement(achievement))
                continue;

            AchievementReward const* reward = sAchievementMgr->GetAchievementReward(achievement);
            if (!reward)
                continue;

            uint32 titleId = reward->TitleId[factionIdx];
            if (!titleId)
                continue;

            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId);
            if (!titleEntry)
                continue;

            foundCount++;
            if (!player->HasTitle(titleEntry))
            {
                player->SetTitle(titleEntry);
                newCount++;
            }
        }

        // Part 2: Renown milestone titles (not triggered by achievement system)
        uint32 renownCount = 0;
        for (uint32 titleId : renownTitleIds)
        {
            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId);
            if (!titleEntry)
                continue;

            foundCount++;
            if (!player->HasTitle(titleEntry))
            {
                player->SetTitle(titleEntry);
                newCount++;
                renownCount++;
            }
        }

        handler->PSendSysMessage("Reputation titles: %u found, %u newly granted (%u already owned, %u from renown).",
            foundCount, newCount, foundCount - newCount, renownCount);
        return true;
    }
};

void AddSC_maxtitles_command()
{
    new maxtitles_commandscript();
}
