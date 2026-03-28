#include "custom_display_handler.h"
#include "Define.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Common.h"
#include "StringConvert.h"

namespace RoleplayCore
{
    using namespace Trinity::ChatCommands;

    class DisplayCommands : public CommandScript
    {
    public:
        DisplayCommands() : CommandScript("player_display_commands") { }

        std::span<ChatCommandBuilder const> GetCommands() const override
        {
            static ChatCommandTable displayCommandTable =
            {
                { "head",      rbac::RBAC_PERM_COMMAND_DISP_HEAD,       false, Display<DISPLAY_TYPE_HEAD>,            "Syntax: .display head <itemId> [bonusId]\nOverrides head appearance." },
                { "shoulders", rbac::RBAC_PERM_COMMAND_DISP_SHOULDERS,  false, Display<DISPLAY_TYPE_SHOULDERS>,       "Syntax: .display shoulders <itemId> [bonusId]\nOverrides shoulder appearance." },
                { "lshoulder", rbac::RBAC_PERM_COMMAND_DISP_SHOULDERS,  false, Display<DISPLAY_TYPE_SHOULDERS, true>, "Syntax: .display lshoulder <itemId> [bonusId]\nOverrides secondary shoulder." },
                { "shirt",     rbac::RBAC_PERM_COMMAND_DISP_SHIRT,      false, Display<DISPLAY_TYPE_SHIRT>,           "Syntax: .display shirt <itemId> [bonusId]\nOverrides shirt appearance." },
                { "chest",     rbac::RBAC_PERM_COMMAND_DISP_CHEST,      false, Display<DISPLAY_TYPE_CHEST>,           "Syntax: .display chest <itemId> [bonusId]\nOverrides chest appearance." },
                { "waist",     rbac::RBAC_PERM_COMMAND_DISP_WAIST,      false, Display<DISPLAY_TYPE_WAIST>,           "Syntax: .display waist <itemId> [bonusId]\nOverrides belt appearance." },
                { "legs",      rbac::RBAC_PERM_COMMAND_DISP_LEGS,       false, Display<DISPLAY_TYPE_PANTS>,           "Syntax: .display legs <itemId> [bonusId]\nOverrides leg appearance." },
                { "feet",      rbac::RBAC_PERM_COMMAND_DISP_FEET,       false, Display<DISPLAY_TYPE_BOOTS>,           "Syntax: .display feet <itemId> [bonusId]\nOverrides boot appearance." },
                { "wrists",    rbac::RBAC_PERM_COMMAND_DISP_WRISTS,     false, Display<DISPLAY_TYPE_WRISTS>,          "Syntax: .display wrists <itemId> [bonusId]\nOverrides bracer appearance." },
                { "hands",     rbac::RBAC_PERM_COMMAND_DISP_HANDS,      false, Display<DISPLAY_TYPE_HANDS>,           "Syntax: .display hands <itemId> [bonusId]\nOverrides glove appearance." },
                { "back",      rbac::RBAC_PERM_COMMAND_DISP_BACK,       false, Display<DISPLAY_TYPE_BACK>,            "Syntax: .display back <itemId> [bonusId]\nOverrides cloak appearance." },
                { "tabard",    rbac::RBAC_PERM_COMMAND_DISP_TABARD,     false, Display<DISPLAY_TYPE_TABARD>,          "Syntax: .display tabard <itemId> [bonusId]\nOverrides tabard appearance." },
                { "mainhand",  rbac::RBAC_PERM_COMMAND_DISP_MAINHAND,   false, Display<DISPLAY_TYPE_MAIN>,            "Syntax: .display mainhand <itemId> [bonusId]\nOverrides main-hand weapon appearance." },
                { "offhand",   rbac::RBAC_PERM_COMMAND_DISP_OFFHAND,    false, Display<DISPLAY_TYPE_OFF>,             "Syntax: .display offhand <itemId> [bonusId]\nOverrides off-hand weapon appearance." },
            };

            static ChatCommandTable commandTable =
            {
                { "display", displayCommandTable},
            };

            return commandTable;
        }

        // Optimized template method for processing display commands
        template <DisplayType T, bool secondary = false>
        static bool Display(ChatHandler* handler, char const* args)
        {
            if (!handler || !args)
                return false;

            char* id = handler->extractKeyFromLink((char*)args, "Hitem");
            if (!id)
                return false;

            // Retrieve item ID using safe parsing
            auto itemIdOpt = Trinity::StringTo<uint32>(id);
            if (!itemIdOpt)
                return false;
            uint32 itemId = *itemIdOpt;

            // Retrieve the bonus, if specified
            uint32 bonus = 0;
            char* bonusStr = strtok(nullptr, " ");
            if (bonusStr)
                bonus = Trinity::StringTo<uint32>(bonusStr).value_or(0);

            // Call the display handler
            DisplayHandler::GetInstance().Display(handler, T, itemId, bonus, secondary);
            return true;
        }
    };
}

void AddSC_CustomDisplayCommands() { new RoleplayCore::DisplayCommands(); }
