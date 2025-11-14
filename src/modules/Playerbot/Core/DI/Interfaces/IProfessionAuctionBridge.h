/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include <vector>

class Player;

namespace Playerbot
{

// Forward declarations
enum class ProfessionType : uint8;
enum class ProfessionAuctionStrategy : uint8;
class AuctionHouse;
struct MaterialStockpileConfig;
struct CraftedItemAuctionConfig;
struct ProfessionAuctionProfile;
struct ProfessionAuctionStatistics;

class TC_GAME_API IProfessionAuctionBridge
{
public:
    virtual ~IProfessionAuctionBridge() = default;

    // Core bridge management
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual void SetEnabled(::Player* player, bool enabled) = 0;
    virtual bool IsEnabled(::Player* player) const = 0;
    virtual void SetAuctionProfile(uint32 playerGuid, ProfessionAuctionProfile const& profile) = 0;
    virtual ProfessionAuctionProfile GetAuctionProfile(uint32 playerGuid) const = 0;

    // Material auction automation
    virtual void SellExcessMaterials(::Player* player) = 0;
    virtual bool ShouldSellMaterial(::Player* player, uint32 itemId, uint32 currentCount) const = 0;
    virtual bool ListMaterialOnAuction(::Player* player, uint32 itemGuid, MaterialStockpileConfig const& config) = 0;
    virtual uint32 GetOptimalMaterialPrice(::Player* player, uint32 itemId, uint32 stackSize) const = 0;

    // Crafted item auction automation
    virtual void SellCraftedItems(::Player* player) = 0;
    virtual bool ShouldSellCraftedItem(::Player* player, uint32 itemId, uint32 materialCost) const = 0;
    virtual bool ListCraftedItemOnAuction(::Player* player, uint32 itemGuid, CraftedItemAuctionConfig const& config) = 0;
    virtual float CalculateProfitMargin(::Player* player, uint32 itemId, uint32 marketPrice, uint32 materialCost) const = 0;

    // Material purchasing automation
    virtual void BuyMaterialsForLeveling(::Player* player, ProfessionType profession) = 0;
    virtual ::std::vector<::std::pair<uint32, uint32>> GetNeededMaterialsForLeveling(::Player* player, ProfessionType profession) const = 0;
    virtual bool IsMaterialAvailableForPurchase(::Player* player, uint32 itemId, uint32 quantity, uint32 maxPricePerUnit) const = 0;
    virtual bool PurchaseMaterial(::Player* player, uint32 itemId, uint32 quantity, uint32 maxPricePerUnit) = 0;

    // Stockpile management
    virtual void SetMaterialStockpile(uint32 playerGuid, uint32 itemId, MaterialStockpileConfig const& config) = 0;
    virtual void SetCraftedItemAuction(uint32 playerGuid, uint32 itemId, CraftedItemAuctionConfig const& config) = 0;
    virtual uint32 GetCurrentStockpile(::Player* player, uint32 itemId) const = 0;
    virtual bool IsStockpileTargetMet(::Player* player, uint32 itemId) const = 0;

    // Integration with auction house
    virtual AuctionHouse* GetAuctionHouse() const = 0;
    virtual void SynchronizeWithAuctionHouse(::Player* player) = 0;

    // Statistics
    virtual ProfessionAuctionStatistics const& GetPlayerStatistics(uint32 playerGuid) const = 0;
    virtual ProfessionAuctionStatistics const& GetGlobalStatistics() const = 0;
    virtual void ResetStatistics(uint32 playerGuid) = 0;
};

} // namespace Playerbot
