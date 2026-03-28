#include "Chat.h"
#include "DB2Stores.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ChatCommandTags.h"
#include "Player.h"
#include "RBAC.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"

using namespace Trinity::ChatCommands;
class maxrep_commandscript : public CommandScript
{
public:
    maxrep_commandscript() : CommandScript("maxrep_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "maxrep", rbac::RBAC_PERM_COMMAND_MAXREP, false, &HandleMaxRepCommand,
              "Max all faction reputations, renown currencies, and related currencies." },
        };
        return commandTable;
    }

    static bool HandleMaxRepCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 factionCount = 0;
        uint32 renownCount = 0;
        uint32 currencyCount = 0;

        // Part 1: Max all trackable factions to their maximum standing
        for (FactionEntry const* faction : sFactionStore)
        {
            if (faction->ReputationIndex < 0)
                continue;

            player->GetReputationMgr().SetVisible(faction);
            int32 maxRep = player->GetReputationMgr().GetMaxReputation(faction);
            player->GetReputationMgr().SetOneFactionReputation(faction, maxRep, false);
            player->GetReputationMgr().SendState(player->GetReputationMgr().GetState(faction));
            factionCount++;
        }

        // Part 2: Max all renown currencies via RenownCurrencyID on factions
        for (FactionEntry const* faction : sFactionStore)
        {
            if (faction->RenownCurrencyID <= 0)
                continue;

            CurrencyTypesEntry const* currency = sCurrencyTypesStore.LookupEntry(faction->RenownCurrencyID);
            if (!currency || currency->MaxQty == 0)
                continue;

            uint32 current = player->GetCurrencyQuantity(currency->ID);
            if (current < currency->MaxQty)
            {
                player->ModifyCurrency(currency->ID, int32(currency->MaxQty - current), CurrencyGainSource::Cheat);
                renownCount++;
            }
        }

        // Part 3: Bonus currencies (Shadowlands, general useful ones)
        static constexpr std::pair<uint32, uint32> bonusCurrencies[] =
        {
            { 1767,  50000  },  // Stygia
            { 1813,  200000 },  // Reservoir Anima
            { 1828,  50000  },  // Soul Ash
            { 1859,  200000 },  // Reservoir Anima-Kyrian
            { 1860,  200000 },  // Reservoir Anima-Venthyr
            { 1861,  200000 },  // Reservoir Anima-Night Fae
            { 1862,  200000 },  // Reservoir Anima-Necrolord
            { 1885,  10000  },  // Grateful Offering
            { 1977,  5000   },  // Stygian Ember
        };

        for (auto [currId, target] : bonusCurrencies)
        {
            CurrencyTypesEntry const* currency = sCurrencyTypesStore.LookupEntry(currId);
            if (!currency)
                continue;

            uint32 current = player->GetCurrencyQuantity(currId);
            if (current < target)
            {
                player->ModifyCurrency(currId, int32(target - current), CurrencyGainSource::Cheat);
                currencyCount++;
            }
        }

        handler->PSendSysMessage("Maxed %u factions to Exalted, %u renown currencies, %u bonus currencies.",
            factionCount, renownCount, currencyCount);
        return true;
    }
};

void AddSC_maxrep_command()
{
    new maxrep_commandscript();
}
