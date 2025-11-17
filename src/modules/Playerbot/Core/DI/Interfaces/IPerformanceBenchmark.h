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
#include <vector>
#include <cstdint>

namespace Playerbot
{

// Forward declarations
struct BenchmarkResult;
struct ScalabilityTest;
struct ResourceAnalysis;
struct StressTest;
struct OptimizationRecommendation;
struct PerformanceSnapshot;
enum class BenchmarkType : uint8;
enum class LoadLevel : uint8;

/**
 * @brief Interface for performance benchmarking system
 *
 * Provides comprehensive performance analysis, stress testing, and optimization
 * insights for all playerbot systems under various load conditions.
 */
class TC_GAME_API IPerformanceBenchmark
{
public:
    virtual ~IPerformanceBenchmark() = default;

    // Core benchmarking framework
    virtual BenchmarkResult RunBenchmark(BenchmarkType type, LoadLevel loadLevel, uint32 duration = 60000) = 0;
    virtual ::std::vector<BenchmarkResult> RunBenchmarkSuite(LoadLevel loadLevel) = 0;
    virtual void RunContinuousBenchmarking(uint32 intervalMs) = 0;
    virtual void StopContinuousBenchmarking() = 0;

    // AI performance benchmarks
    virtual BenchmarkResult BenchmarkAIDecisionSpeed(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkCombatAI(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkStrategyExecution(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkActionSelection(uint32 botCount, uint32 duration) = 0;

    // System-specific benchmarks
    virtual BenchmarkResult BenchmarkGroupCoordination(uint32 groupCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkQuestExecution(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkLootProcessing(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkTradeOperations(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkAuctionAnalysis(uint32 botCount, uint32 duration) = 0;
    virtual BenchmarkResult BenchmarkGuildInteractions(uint32 botCount, uint32 duration) = 0;

    // Scalability testing
    virtual ScalabilityTest RunScalabilityTest(BenchmarkType type) = 0;
    virtual void FindPerformanceBreakpoints() = 0;
    virtual uint32 DetermineOptimalBotCount() = 0;
    virtual void AnalyzeScalingCharacteristics() = 0;

    // Resource utilization analysis
    virtual ResourceAnalysis AnalyzeResourceUtilization(LoadLevel loadLevel) = 0;
    virtual void ProfileMemoryUsagePatterns() = 0;
    virtual void AnalyzeCPUHotspots() = 0;
    virtual void MeasureDatabasePerformance() = 0;

    // Performance regression testing
    virtual bool RunRegressionBenchmarks() = 0;
    virtual void EstablishPerformanceBaseline() = 0;
    virtual bool DetectPerformanceRegression() = 0;
    virtual void CompareWithBaseline(const ::std::vector<BenchmarkResult>& currentResults) = 0;

    // Stress testing
    virtual StressTest RunStressTest(const StressTest& testConfig) = 0;
    virtual void TestSystemLimits() = 0;
    virtual void MeasureRecoveryTime() = 0;
    virtual bool ValidateSystemStability(uint32 duration) = 0;

    // Performance optimization insights
    virtual ::std::vector<OptimizationRecommendation> GenerateOptimizationRecommendations() = 0;
    virtual void AnalyzePerformancePatterns() = 0;
    virtual void IdentifyBottlenecks() = 0;
    virtual void SuggestConfigurationTuning() = 0;

    // Comparative benchmarking
    virtual void CompareBenchmarkResults(const BenchmarkResult& baseline, const BenchmarkResult& current) = 0;
    virtual void GeneratePerformanceReport() = 0;
    virtual void TrackPerformanceTrends() = 0;
    virtual void BenchmarkAgainstTargets() = 0;

    // Real-time monitoring
    virtual void StartPerformanceMonitoring() = 0;
    virtual void StopPerformanceMonitoring() = 0;
    virtual PerformanceSnapshot GetCurrentPerformanceSnapshot() = 0;
    virtual ::std::vector<PerformanceSnapshot> GetPerformanceHistory(uint32 durationMs) = 0;

    // Configuration and settings
    virtual void SetPerformanceTargets(BenchmarkType type, uint32 targetOps, float targetResponseTime) = 0;
    virtual void SetBenchmarkTimeout(uint32 timeoutMs) = 0;
    virtual void EnableDetailedProfiling(bool enable) = 0;
    virtual void SetBenchmarkReportLevel(uint32 level) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessBenchmarkQueue() = 0;
    virtual void CleanupBenchmarkData() = 0;
};

} // namespace Playerbot
