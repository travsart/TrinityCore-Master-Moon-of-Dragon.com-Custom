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

namespace Playerbot
{

// Forward declarations
struct MarketSnapshot;
enum class MarketTrend : uint8;
enum class MarketSegment : uint8;
struct MarketMetrics;
struct PriceAnalysis;
struct MarketOpportunity;
struct CompetitorAnalysis;
struct AnalysisMetrics;

class TC_GAME_API IMarketAnalysis
{
public:
    virtual ~IMarketAnalysis() = default;

    // Core market analysis
    virtual MarketSnapshot GetMarketSnapshot(uint32 itemId) = 0;
    virtual MarketTrend GetMarketTrend(uint32 itemId, uint32 daysBack = 7) = 0;
    virtual float GetPricePrediction(uint32 itemId, uint32 hoursAhead = 24) = 0;
    virtual std::vector<uint32> GetTrendingItems(MarketSegment segment = MarketSegment::EQUIPMENT) = 0;

    // Market intelligence
    virtual void AnalyzeMarketConditions() = 0;
    virtual void UpdateMarketData(uint32 itemId, uint32 price, uint32 quantity, uint32 timestamp = 0) = 0;
    virtual void RecordSale(uint32 itemId, uint32 price, uint32 quantity, uint32 sellTime) = 0;

    // Advanced market metrics
    virtual MarketMetrics GetMarketMetrics(uint32 itemId) = 0;
    virtual MarketMetrics GetSegmentMetrics(MarketSegment segment) = 0;

    // Price analysis and forecasting
    virtual PriceAnalysis AnalyzePrice(uint32 itemId) = 0;
    virtual float CalculateFairValue(uint32 itemId) = 0;
    virtual bool IsPriceAnomaly(uint32 itemId, uint32 price) = 0;

    // Market opportunity identification
    virtual std::vector<MarketOpportunity> IdentifyOpportunities(Player* player, uint32 budgetLimit = 0) = 0;
    virtual bool IsGoodBuyingOpportunity(uint32 itemId, uint32 price) = 0;
    virtual bool IsGoodSellingOpportunity(uint32 itemId, uint32 price) = 0;

    // Competitive analysis
    virtual CompetitorAnalysis AnalyzeCompetition(uint32 itemId) = 0;
    virtual std::vector<uint32> GetTopSellers(uint32 itemId, uint32 count = 5) = 0;

    // Market segment analysis
    virtual void AnalyzeMarketSegment(MarketSegment segment) = 0;
    virtual MarketTrend GetSegmentTrend(MarketSegment segment) = 0;

    // Performance and accuracy tracking
    virtual AnalysisMetrics const& GetAnalysisMetrics() const = 0;

    // Configuration and learning
    virtual void SetAnalysisDepth(float depth) = 0;
    virtual void UpdatePredictionAccuracy(uint32 itemId, float predictedPrice, float actualPrice) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateTrendAnalysis() = 0;
    virtual void CleanupOldData() = 0;
};

} // namespace Playerbot
