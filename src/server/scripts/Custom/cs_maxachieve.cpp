#include "AchievementMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ChatCommandTags.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
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
}

class maxachieve_commandscript : public CommandScript
{
public:
    maxachieve_commandscript() : CommandScript("maxachieve_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "maxachieve", rbac::RBAC_PERM_COMMAND_MAXACHIEVE, false, &HandleMaxAchieveCommand,
              "Complete all reputation achievements and grant their rewards (titles, items)." },
        };
        return commandTable;
    }

    static bool HandleMaxAchieveCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // CompletedAchievement skips if GM mode is on — temporarily disable
        bool wasGM = player->IsGameMaster();
        if (wasGM)
            player->SetGameMaster(false);

        uint32 count = 0;
        uint32 skipped = 0;
        uint32 alreadyDone = 0;
        uint32 titleCount = 0;
        Team team = player->GetTeam();
        int factionIdx = team == ALLIANCE ? 0 : 1;

        for (AchievementEntry const* achievement : sAchievementStore)
        {
            // Only reputation achievements
            if (!IsReputationAchievement(achievement))
                continue;

            // Skip statistics (counter achievements that never complete)
            if (achievement->Flags & ACHIEVEMENT_FLAG_COUNTER)
            {
                skipped++;
                continue;
            }

            // Skip hidden tracking flags
            if (achievement->Flags & ACHIEVEMENT_FLAG_TRACKING_FLAG)
            {
                skipped++;
                continue;
            }

            // Skip wrong-faction achievements
            if ((achievement->Faction == ACHIEVEMENT_FACTION_HORDE && team != HORDE) ||
                (achievement->Faction == ACHIEVEMENT_FACTION_ALLIANCE && team != ALLIANCE))
            {
                skipped++;
                continue;
            }

            // Skip guild achievements (flag 0x4000) — these belong to guilds, not players
            if (achievement->Flags & ACHIEVEMENT_FLAG_GUILD)
            {
                skipped++;
                continue;
            }

            // Complete the achievement (CompletedAchievement skips if already done)
            if (player->HasAchieved(achievement->ID))
                alreadyDone++;
            else
                count++;

            player->CompletedAchievement(achievement);

            // Always grant title rewards directly — CompletedAchievement skips if already completed
            if (AchievementReward const* reward = sAchievementMgr->GetAchievementReward(achievement))
            {
                uint32 titleId = reward->TitleId[factionIdx];
                if (titleId)
                {
                    if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId))
                    {
                        if (!player->HasTitle(titleEntry))
                        {
                            player->SetTitle(titleEntry);
                            titleCount++;
                        }
                    }
                }
            }
        }

        // Restore GM mode
        if (wasGM)
            player->SetGameMaster(true);

        handler->PSendSysMessage("Completed %u reputation achievements (%u already done, %u skipped), granted %u titles.",
            count, alreadyDone, skipped, titleCount);
        return true;
    }
};

void AddSC_maxachieve_command()
{
    new maxachieve_commandscript();
}
