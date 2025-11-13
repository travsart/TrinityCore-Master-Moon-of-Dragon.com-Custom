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
class Creature;

namespace Playerbot
{

// Forward declarations
enum class VendorType : uint8;
struct VendorInfo;
struct VendorAnalysis;
struct BuyingStrategy;
struct SellingStrategy;
struct VendorMetrics;

class TC_GAME_API IVendorInteraction
{
public:
    virtual ~IVendorInteraction() = default;

    // Core vendor discovery
    virtual void LoadVendorDataFromDatabase() = 0;
    virtual ::std::vector<VendorInfo> QueryVendorsByZone(uint32 zoneId) = 0;
    virtual ::std::vector<VendorInfo> QueryVendorsByType(VendorType type) = 0;
    virtual VendorInfo GetVendorFromCreature(const Creature* creature) = 0;

    // Intelligent vendor selection
    virtual uint32 FindOptimalVendor(Player* player, VendorType preferredType, float maxDistance) = 0;
    virtual ::std::vector<uint32> FindVendorsWithItem(uint32 itemId, uint32 playerZone) = 0;
    virtual uint32 FindCheapestVendor(uint32 itemId, const ::std::vector<uint32>& vendorGuids) = 0;
    virtual uint32 FindNearestRepairVendor(Player* player) = 0;

    // Vendor interaction optimization
    virtual void OptimizeVendorRoute(Player* player, const ::std::vector<::std::pair<VendorType, uint32>>& needs) = 0;
    virtual void PlanVendorTrip(Player* player, const ::std::vector<uint32>& itemsToBuy, const ::std::vector<uint32>& itemsToSell) = 0;
    virtual bool ShouldTravelToVendor(Player* player, uint32 vendorGuid, float expectedValue) = 0;

    // Advanced vendor analysis
    virtual VendorAnalysis AnalyzeVendor(uint32 vendorGuid) = 0;
    virtual void UpdateVendorAnalysis(uint32 vendorGuid) = 0;
    virtual bool CanPlayerUseVendor(Player* player, uint32 vendorGuid) = 0;

    // Dynamic vendor inventory management
    virtual void TrackVendorInventory(uint32 vendorGuid) = 0;
    virtual void UpdateVendorStock(uint32 vendorGuid, uint32 itemId, int32 stockChange) = 0;
    virtual uint32 GetVendorStock(uint32 vendorGuid, uint32 itemId) = 0;
    virtual void PredictVendorRestocking(uint32 vendorGuid) = 0;

    // Automated buying strategies
    virtual void ExecuteBuyingStrategy(Player* player, uint32 vendorGuid, const BuyingStrategy& strategy) = 0;
    virtual void AutoBuyConsumables(Player* player, uint32 vendorGuid) = 0;
    virtual void AutoBuyReagents(Player* player, uint32 vendorGuid) = 0;
    virtual void BuyBestAvailableGear(Player* player, uint32 vendorGuid) = 0;

    // Automated selling strategies
    virtual void ExecuteSellingStrategy(Player* player, uint32 vendorGuid, const SellingStrategy& strategy) = 0;
    virtual void AutoSellJunkItems(Player* player, uint32 vendorGuid) = 0;
    virtual void SellOutdatedEquipment(Player* player, uint32 vendorGuid) = 0;
    virtual uint32 CalculateSellingValue(Player* player, const ::std::vector<uint32>& itemGuids) = 0;

    // Reputation and faction vendor handling
    virtual void HandleFactionVendors(Player* player) = 0;
    virtual ::std::vector<uint32> GetAccessibleFactionVendors(Player* player) = 0;
    virtual bool MeetsReputationRequirement(Player* player, uint32 vendorGuid) = 0;
    virtual void OptimizeReputationGains(Player* player) = 0;

    // Vendor service coordination
    virtual void CoordinateRepairServices(Player* player) = 0;
    virtual void HandleInnkeeperServices(Player* player, uint32 innkeeperGuid) = 0;
    virtual void ManageFlightPathServices(Player* player, uint32 flightMasterGuid) = 0;
    virtual void ProcessTrainerServices(Player* player, uint32 trainerGuid) = 0;

    // Performance monitoring
    virtual VendorMetrics GetPlayerVendorMetrics(uint32 playerGuid) = 0;
    virtual VendorMetrics GetGlobalVendorMetrics() = 0;

    // Configuration and optimization
    virtual void SetBuyingStrategy(uint32 playerGuid, const BuyingStrategy& strategy) = 0;
    virtual void SetSellingStrategy(uint32 playerGuid, const SellingStrategy& strategy) = 0;
    virtual BuyingStrategy GetBuyingStrategy(uint32 playerGuid) = 0;
    virtual SellingStrategy GetSellingStrategy(uint32 playerGuid) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void RefreshVendorDatabase() = 0;
    virtual void ValidateVendorData() = 0;
};

} // namespace Playerbot

#endif // IVendorInteraction_h
