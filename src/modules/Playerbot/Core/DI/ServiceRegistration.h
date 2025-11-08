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

#pragma once

#include "ServiceContainer.h"
#include "Interfaces/ISpatialGridManager.h"
#include "Interfaces/IBotSessionMgr.h"
#include "Interfaces/IConfigManager.h"
#include "Interfaces/IBotLifecycleManager.h"
#include "Interfaces/IBotDatabasePool.h"
#include "Interfaces/IBotNameMgr.h"
#include "Interfaces/IDungeonScriptMgr.h"
#include "Interfaces/IEquipmentManager.h"
#include "Interfaces/IBotAccountMgr.h"
#include "Interfaces/ILFGBotManager.h"
#include "Interfaces/IBotGearFactory.h"
#include "Interfaces/IBotMonitor.h"
#include "Interfaces/IBotLevelManager.h"
#include "Interfaces/IPlayerbotGroupManager.h"
#include "Interfaces/IBotTalentManager.h"
#include "Interfaces/IPlayerbotConfig.h"
#include "Interfaces/IBotSpawner.h"
#include "Interfaces/IBotWorldPositioner.h"
#include "Interfaces/IBotHealthCheck.h"
#include "Interfaces/IBotScheduler.h"
#include "Interfaces/IBotCharacterDistribution.h"
#include "Interfaces/IBotLevelDistribution.h"
#include "Interfaces/IGroupEventBus.h"
#include "Interfaces/ILFGGroupCoordinator.h"
#include "Interfaces/ILootEventBus.h"
#include "Interfaces/IQuestEventBus.h"
#include "Interfaces/IAuctionEventBus.h"
#include "Interfaces/INPCEventBus.h"
#include "Interfaces/ICooldownEventBus.h"
#include "Interfaces/IAuraEventBus.h"
#include "Interfaces/IInstanceEventBus.h"
#include "Interfaces/ISocialEventBus.h"
#include "Interfaces/ICombatEventBus.h"
#include "Interfaces/IResourceEventBus.h"
#include "Interfaces/ILootAnalysis.h"
#include "Interfaces/IGuildBankManager.h"
#include "Interfaces/ILootCoordination.h"
#include "Interfaces/ILootDistribution.h"
#include "Interfaces/IMarketAnalysis.h"
#include "Interfaces/ITradeSystem.h"
#include "Interfaces/IQuestPickup.h"
#include "Interfaces/IGuildEventCoordinator.h"
#include "Interfaces/IProfessionManager.h"
#include "Interfaces/IQuestCompletion.h"
#include "Interfaces/IQuestValidation.h"
#include "Interfaces/IQuestTurnIn.h"
#include "Interfaces/IRoleAssignment.h"
#include "Interfaces/IDynamicQuestSystem.h"
#include "Interfaces/IFarmingCoordinator.h"
#include "Interfaces/IAuctionHouse.h"
#include "Interfaces/IProfessionAuctionBridge.h"
#include "Interfaces/ILFGRoleDetector.h"
#include "Interfaces/IVendorInteraction.h"
#include "Interfaces/ILFGBotSelector.h"
#include "Interfaces/IGuildIntegration.h"
#include "Interfaces/IDungeonBehavior.h"
#include "Interfaces/IInstanceCoordination.h"
#include "Spatial/SpatialGridManager.h"
#include "Session/BotSessionMgr.h"
#include "Config/ConfigManager.h"
#include "Lifecycle/BotLifecycleManager.h"
#include "Database/BotDatabasePool.h"
#include "Character/BotNameMgr.h"
#include "Dungeon/DungeonScriptMgr.h"
#include "Equipment/EquipmentManager.h"
#include "Account/BotAccountMgr.h"
#include "LFG/LFGBotManager.h"
#include "Equipment/BotGearFactory.h"
#include "Monitoring/BotMonitor.h"
#include "Character/BotLevelManager.h"
#include "Group/PlayerbotGroupManager.h"
#include "Talents/BotTalentManager.h"
#include "Config/PlayerbotConfig.h"
#include "Lifecycle/BotSpawner.h"
#include "Movement/BotWorldPositioner.h"
#include "Session/BotHealthCheck.h"
#include "Lifecycle/BotScheduler.h"
#include "Character/BotCharacterDistribution.h"
#include "Character/BotLevelDistribution.h"
#include "Group/GroupEventBus.h"
#include "LFG/LFGGroupCoordinator.h"
#include "Loot/LootEventBus.h"
#include "Quest/QuestEventBus.h"
#include "Auction/AuctionEventBus.h"
#include "NPC/NPCEventBus.h"
#include "Cooldown/CooldownEventBus.h"
#include "Aura/AuraEventBus.h"
#include "Instance/InstanceEventBus.h"
#include "Social/SocialEventBus.h"
#include "Combat/CombatEventBus.h"
#include "Resource/ResourceEventBus.h"
#include "Social/LootAnalysis.h"
#include "Social/GuildBankManager.h"
#include "Social/LootCoordination.h"
#include "Social/LootDistribution.h"
#include "Social/MarketAnalysis.h"
#include "Social/TradeSystem.h"
#include "Quest/QuestPickup.h"
#include "Social/GuildEventCoordinator.h"
#include "Professions/ProfessionManager.h"
#include "Quest/QuestCompletion.h"
#include "Quest/QuestValidation.h"
#include "Quest/QuestTurnIn.h"
#include "Group/RoleAssignment.h"
#include "Quest/DynamicQuestSystem.h"
#include "Professions/FarmingCoordinator.h"
#include "Social/AuctionHouse.h"
#include "Professions/ProfessionAuctionBridge.h"
#include "LFG/LFGRoleDetector.h"
#include "Social/VendorInteraction.h"
#include "LFG/LFGBotSelector.h"
#include "Social/GuildIntegration.h"
#include "Dungeon/DungeonBehavior.h"
#include "Dungeon/InstanceCoordination.h"
#include "Log.h"

namespace Playerbot
{

/**
 * @brief Service registration for Playerbot module
 *
 * Registers all core services with the DI container at startup.
 * This enables dependency injection throughout the module.
 *
 * **Call this during module initialization (World::SetInitialWorldSettings)**
 *
 * Example:
 * @code
 * void World::SetInitialWorldSettings()
 * {
 *     // ... existing code ...
 *
 *     // Register Playerbot services
 *     Playerbot::RegisterPlayerbotServices();
 * }
 * @endcode
 */
inline void RegisterPlayerbotServices()
{
    TC_LOG_INFO("playerbot.di", "Registering Playerbot services with DI container...");

    try
    {
        auto& container = Services::Container();

        // Register SpatialGridManager
        container.RegisterInstance<ISpatialGridManager>(
            std::shared_ptr<ISpatialGridManager>(
                &SpatialGridManager::Instance(),
                [](ISpatialGridManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ISpatialGridManager");

        // Register BotSessionMgr
        container.RegisterInstance<IBotSessionMgr>(
            std::shared_ptr<IBotSessionMgr>(
                BotSessionMgr::instance(),
                [](IBotSessionMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotSessionMgr");

        // Register ConfigManager (Phase 2)
        container.RegisterInstance<IConfigManager>(
            std::shared_ptr<IConfigManager>(
                ConfigManager::instance(),
                [](IConfigManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IConfigManager");

        // Register BotLifecycleManager (Phase 2)
        container.RegisterInstance<IBotLifecycleManager>(
            std::shared_ptr<IBotLifecycleManager>(
                BotLifecycleManager::instance(),
                [](IBotLifecycleManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLifecycleManager");

        // Register BotDatabasePool (Phase 2)
        container.RegisterInstance<IBotDatabasePool>(
            std::shared_ptr<IBotDatabasePool>(
                BotDatabasePool::instance(),
                [](IBotDatabasePool*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotDatabasePool");

        // Register BotNameMgr (Phase 3)
        container.RegisterInstance<IBotNameMgr>(
            std::shared_ptr<IBotNameMgr>(
                BotNameMgr::instance(),
                [](IBotNameMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotNameMgr");

        // Register DungeonScriptMgr (Phase 3)
        container.RegisterInstance<IDungeonScriptMgr>(
            std::shared_ptr<IDungeonScriptMgr>(
                Playerbot::DungeonScriptMgr::instance(),
                [](IDungeonScriptMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IDungeonScriptMgr");

        // Register EquipmentManager (Phase 4)
        container.RegisterInstance<IEquipmentManager>(
            std::shared_ptr<IEquipmentManager>(
                Playerbot::EquipmentManager::instance(),
                [](IEquipmentManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IEquipmentManager");

        // Register BotAccountMgr (Phase 4)
        container.RegisterInstance<IBotAccountMgr>(
            std::shared_ptr<IBotAccountMgr>(
                Playerbot::BotAccountMgr::instance(),
                [](IBotAccountMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotAccountMgr");

        // Register LFGBotManager (Phase 5)
        container.RegisterInstance<ILFGBotManager>(
            std::shared_ptr<ILFGBotManager>(
                LFGBotManager::instance(),
                [](ILFGBotManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILFGBotManager");

        // Register BotGearFactory (Phase 5)
        container.RegisterInstance<IBotGearFactory>(
            std::shared_ptr<IBotGearFactory>(
                Playerbot::BotGearFactory::instance(),
                [](IBotGearFactory*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotGearFactory");

        // Register BotMonitor (Phase 6)
        container.RegisterInstance<IBotMonitor>(
            std::shared_ptr<IBotMonitor>(
                Playerbot::BotMonitor::instance(),
                [](IBotMonitor*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotMonitor");

        // Register BotLevelManager (Phase 6)
        container.RegisterInstance<IBotLevelManager>(
            std::shared_ptr<IBotLevelManager>(
                Playerbot::BotLevelManager::instance(),
                [](IBotLevelManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLevelManager");

        // Register PlayerbotGroupManager (Phase 7)
        container.RegisterInstance<IPlayerbotGroupManager>(
            std::shared_ptr<IPlayerbotGroupManager>(
                Playerbot::PlayerbotGroupManager::instance(),
                [](IPlayerbotGroupManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IPlayerbotGroupManager");

        // Register BotTalentManager (Phase 7)
        container.RegisterInstance<IBotTalentManager>(
            std::shared_ptr<IBotTalentManager>(
                Playerbot::BotTalentManager::instance(),
                [](IBotTalentManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotTalentManager");

        // Register PlayerbotConfig (Phase 8)
        container.RegisterInstance<IPlayerbotConfig>(
            std::shared_ptr<IPlayerbotConfig>(
                PlayerbotConfig::instance(),
                [](IPlayerbotConfig*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IPlayerbotConfig");

        // Register BotSpawner (Phase 8)
        container.RegisterInstance<IBotSpawner>(
            std::shared_ptr<IBotSpawner>(
                Playerbot::BotSpawner::instance(),
                [](IBotSpawner*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotSpawner");

        // Register BotWorldPositioner (Phase 9)
        container.RegisterInstance<IBotWorldPositioner>(
            std::shared_ptr<IBotWorldPositioner>(
                Playerbot::BotWorldPositioner::instance(),
                [](IBotWorldPositioner*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotWorldPositioner");

        // Register BotHealthCheck (Phase 9)
        container.RegisterInstance<IBotHealthCheck>(
            std::shared_ptr<IBotHealthCheck>(
                Playerbot::BotHealthCheck::instance(),
                [](IBotHealthCheck*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotHealthCheck");

        // Register BotScheduler (Phase 10)
        container.RegisterInstance<IBotScheduler>(
            std::shared_ptr<IBotScheduler>(
                Playerbot::BotScheduler::instance(),
                [](IBotScheduler*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotScheduler");

        // Register BotCharacterDistribution (Phase 11)
        container.RegisterInstance<IBotCharacterDistribution>(
            std::shared_ptr<IBotCharacterDistribution>(
                Playerbot::BotCharacterDistribution::instance(),
                [](IBotCharacterDistribution*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotCharacterDistribution");

        // Register BotLevelDistribution (Phase 11)
        container.RegisterInstance<IBotLevelDistribution>(
            std::shared_ptr<IBotLevelDistribution>(
                Playerbot::BotLevelDistribution::instance(),
                [](IBotLevelDistribution*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLevelDistribution");

        // Register GroupEventBus (Phase 12)
        container.RegisterInstance<IGroupEventBus>(
            std::shared_ptr<IGroupEventBus>(
                Playerbot::GroupEventBus::instance(),
                [](IGroupEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IGroupEventBus");

        // Register LFGGroupCoordinator (Phase 12)
        container.RegisterInstance<ILFGGroupCoordinator>(
            std::shared_ptr<ILFGGroupCoordinator>(
                Playerbot::LFGGroupCoordinator::instance(),
                [](ILFGGroupCoordinator*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILFGGroupCoordinator");

        // Register LootEventBus (Phase 13)
        container.RegisterInstance<ILootEventBus>(
            std::shared_ptr<ILootEventBus>(
                Playerbot::LootEventBus::instance(),
                [](ILootEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILootEventBus");

        // Register QuestEventBus (Phase 13)
        container.RegisterInstance<IQuestEventBus>(
            std::shared_ptr<IQuestEventBus>(
                Playerbot::QuestEventBus::instance(),
                [](IQuestEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IQuestEventBus");

        // Register AuctionEventBus (Phase 14)
        container.RegisterInstance<IAuctionEventBus>(
            std::shared_ptr<IAuctionEventBus>(
                Playerbot::AuctionEventBus::instance(),
                [](IAuctionEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IAuctionEventBus");

        // Register NPCEventBus (Phase 14)
        container.RegisterInstance<INPCEventBus>(
            std::shared_ptr<INPCEventBus>(
                Playerbot::NPCEventBus::instance(),
                [](INPCEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered INPCEventBus");

        // Register CooldownEventBus (Phase 15)
        container.RegisterInstance<ICooldownEventBus>(
            std::shared_ptr<ICooldownEventBus>(
                Playerbot::CooldownEventBus::instance(),
                [](ICooldownEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ICooldownEventBus");

        // Register AuraEventBus (Phase 15)
        container.RegisterInstance<IAuraEventBus>(
            std::shared_ptr<IAuraEventBus>(
                Playerbot::AuraEventBus::instance(),
                [](IAuraEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IAuraEventBus");

        // Register InstanceEventBus (Phase 16)
        container.RegisterInstance<IInstanceEventBus>(
            std::shared_ptr<IInstanceEventBus>(
                Playerbot::InstanceEventBus::instance(),
                [](IInstanceEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IInstanceEventBus");

        // Register SocialEventBus (Phase 17)
        container.RegisterInstance<ISocialEventBus>(
            std::shared_ptr<ISocialEventBus>(
                Playerbot::SocialEventBus::instance(),
                [](ISocialEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ISocialEventBus");

        // Register CombatEventBus (Phase 18)
        container.RegisterInstance<ICombatEventBus>(
            std::shared_ptr<ICombatEventBus>(
                Playerbot::CombatEventBus::instance(),
                [](ICombatEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ICombatEventBus");

        // Register ResourceEventBus (Phase 19)
        container.RegisterInstance<IResourceEventBus>(
            std::shared_ptr<IResourceEventBus>(
                Playerbot::ResourceEventBus::instance(),
                [](IResourceEventBus*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IResourceEventBus");

        // Register LootAnalysis (Phase 20)
        container.RegisterInstance<ILootAnalysis>(
            std::shared_ptr<ILootAnalysis>(
                Playerbot::LootAnalysis::instance(),
                [](ILootAnalysis*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILootAnalysis");

        // Register GuildBankManager (Phase 21)
        container.RegisterInstance<IGuildBankManager>(
            std::shared_ptr<IGuildBankManager>(
                Playerbot::GuildBankManager::instance(),
                [](IGuildBankManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IGuildBankManager");

        // Register LootCoordination (Phase 22)
        container.RegisterInstance<ILootCoordination>(
            std::shared_ptr<ILootCoordination>(
                Playerbot::LootCoordination::instance(),
                [](ILootCoordination*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILootCoordination");

        // Register LootDistribution (Phase 23)
        container.RegisterInstance<ILootDistribution>(
            std::shared_ptr<ILootDistribution>(
                Playerbot::LootDistribution::instance(),
                [](ILootDistribution*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILootDistribution");

        // Register MarketAnalysis (Phase 24)
        container.RegisterInstance<IMarketAnalysis>(
            std::shared_ptr<IMarketAnalysis>(
                Playerbot::MarketAnalysis::instance(),
                [](IMarketAnalysis*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IMarketAnalysis");

        // Register TradeSystem (Phase 25)
        container.RegisterInstance<ITradeSystem>(
            std::shared_ptr<ITradeSystem>(
                Playerbot::TradeSystem::instance(),
                [](ITradeSystem*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ITradeSystem");

        // Register QuestPickup (Phase 26)
        container.RegisterInstance<IQuestPickup>(
            std::shared_ptr<IQuestPickup>(
                Playerbot::QuestPickup::instance(),
                [](IQuestPickup*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IQuestPickup");

        // Register GuildEventCoordinator (Phase 27)
        container.RegisterInstance<IGuildEventCoordinator>(
            std::shared_ptr<IGuildEventCoordinator>(
                Playerbot::GuildEventCoordinator::instance(),
                [](IGuildEventCoordinator*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IGuildEventCoordinator");

        // Register ProfessionManager (Phase 28)
        container.RegisterInstance<IProfessionManager>(
            std::shared_ptr<IProfessionManager>(
                Playerbot::ProfessionManager::instance(),
                [](IProfessionManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IProfessionManager");

        // Register QuestCompletion (Phase 29)
        container.RegisterInstance<IQuestCompletion>(
            std::shared_ptr<IQuestCompletion>(
                Playerbot::QuestCompletion::instance(),
                [](IQuestCompletion*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IQuestCompletion");

        // Register QuestValidation (Phase 30)
        container.RegisterInstance<IQuestValidation>(
            std::shared_ptr<IQuestValidation>(
                Playerbot::QuestValidation::instance(),
                [](IQuestValidation*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IQuestValidation");

        // Register QuestTurnIn (Phase 31)
        container.RegisterInstance<IQuestTurnIn>(
            std::shared_ptr<IQuestTurnIn>(
                Playerbot::QuestTurnIn::instance(),
                [](IQuestTurnIn*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IQuestTurnIn");

        // Register RoleAssignment (Phase 32)
        container.RegisterInstance<IRoleAssignment>(
            std::shared_ptr<IRoleAssignment>(
                Playerbot::RoleAssignment::instance(),
                [](IRoleAssignment*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IRoleAssignment");

        // Register DynamicQuestSystem (Phase 33)
        container.RegisterInstance<IDynamicQuestSystem>(
            std::shared_ptr<IDynamicQuestSystem>(
                Playerbot::DynamicQuestSystem::instance(),
                [](IDynamicQuestSystem*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IDynamicQuestSystem");

        // Register FarmingCoordinator (Phase 34)
        container.RegisterInstance<IFarmingCoordinator>(
            std::shared_ptr<IFarmingCoordinator>(
                Playerbot::FarmingCoordinator::instance(),
                [](IFarmingCoordinator*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IFarmingCoordinator");

        // Register AuctionHouse (Phase 35)
        container.RegisterInstance<IAuctionHouse>(
            std::shared_ptr<IAuctionHouse>(
                Playerbot::AuctionHouse::instance(),
                [](IAuctionHouse*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IAuctionHouse");

        // Register ProfessionAuctionBridge (Phase 36)
        container.RegisterInstance<IProfessionAuctionBridge>(
            std::shared_ptr<IProfessionAuctionBridge>(
                Playerbot::ProfessionAuctionBridge::instance(),
                [](IProfessionAuctionBridge*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IProfessionAuctionBridge");

        // Register LFGRoleDetector (Phase 37)
        container.RegisterInstance<ILFGRoleDetector>(
            std::shared_ptr<ILFGRoleDetector>(
                LFGRoleDetector::instance(),
                [](ILFGRoleDetector*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILFGRoleDetector");

        // Register VendorInteraction (Phase 38)
        container.RegisterInstance<IVendorInteraction>(
            std::shared_ptr<IVendorInteraction>(
                Playerbot::VendorInteraction::instance(),
                [](IVendorInteraction*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IVendorInteraction");

        // Register LFGBotSelector (Phase 39)
        container.RegisterInstance<ILFGBotSelector>(
            std::shared_ptr<ILFGBotSelector>(
                LFGBotSelector::instance(),
                [](ILFGBotSelector*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILFGBotSelector");

        // Register GuildIntegration (Phase 40)
        container.RegisterInstance<IGuildIntegration>(
            std::shared_ptr<IGuildIntegration>(
                Playerbot::GuildIntegration::instance(),
                [](IGuildIntegration*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IGuildIntegration");

        // Register DungeonBehavior (Phase 41)
        container.RegisterInstance<IDungeonBehavior>(
            std::shared_ptr<IDungeonBehavior>(
                Playerbot::DungeonBehavior::instance(),
                [](IDungeonBehavior*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IDungeonBehavior");

        // Register InstanceCoordination (Phase 42)
        container.RegisterInstance<IInstanceCoordination>(
            std::shared_ptr<IInstanceCoordination>(
                Playerbot::InstanceCoordination::instance(),
                [](IInstanceCoordination*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IInstanceCoordination");

        TC_LOG_INFO("playerbot.di", "Playerbot service registration complete. {} services registered.",
            container.GetServiceCount());
    }
    catch (std::exception const& e)
    {
        TC_LOG_FATAL("playerbot.di", "Failed to register Playerbot services: {}", e.what());
    }
}

/**
 * @brief Clear all registered services
 *
 * Useful for shutdown or testing cleanup.
 * Called during World::CleanupsBeforeStop()
 */
inline void UnregisterPlayerbotServices()
{
    TC_LOG_INFO("playerbot.di", "Unregistering Playerbot services...");
    Services::Container().Clear();
}

} // namespace Playerbot
