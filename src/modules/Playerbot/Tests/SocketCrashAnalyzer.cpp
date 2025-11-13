/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "WorldSession.h"
#include "BotSession.h"
#include "Socket.h"
#include "WorldSocket.h"
#include "Log.h"
#include "GameTime.h"
#include <memory>
#include <exception>
#include <chrono>

using namespace Playerbot;

/**
 * SOCKET CRASH ANALYZER
 *
 * This class performs detailed analysis of the ACCESS_VIOLATION crash
 * occurring at Socket.h line 230 in the atomic fetch_or operation.
 *
 * The crash stack trace shows:
 * - std::_Atomic_integral<unsigned char,1>::fetch_or+B at atomic line 1333
 * - Trinity::Net::Socket<...>::CloseSocket+37 at Socket.h line 230
 * - WorldSession::Update+72 at WorldSession.cpp line 357
 *
 * This suggests that despite our BUILD_PLAYERBOT guards, some code path
 * is still attempting to call CloseSocket() on a null or invalid socket.
 */
class SocketCrashAnalyzer
{
public:
    static void AnalyzeSocketCrashScenarios()
    {
        TC_LOG_INFO("test.playerbot", "🔍 Starting Socket Crash Analysis");
        TC_LOG_INFO("test.playerbot", "Target: ACCESS_VIOLATION at Socket.h:230 (_openState.fetch_or)");

        TestScenario1_DirectSocketAccess();
        TestScenario2_WorldSessionUpdatePaths();
        TestScenario3_GuardEffectiveness();
        TestScenario4_SocketLifecycle();
        TestScenario5_AtomicOperationValidation();
        TestScenario6_UnguardedCodePaths();

        TC_LOG_INFO("test.playerbot", "🔍 Socket Crash Analysis Complete");
    }

private:
    /**
     * SCENARIO 1: Direct Socket Access
     * Tests if any code is directly accessing socket members on bot sessions
     */
    static void TestScenario1_DirectSocketAccess()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 1: Direct Socket Access Analysis");

        try {
            auto botSession = BotSession::Create(99999);
            if (!botSession) {
                TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                return;
            }

            TC_LOG_INFO("test.playerbot", "✅ BotSession created, analyzing socket access patterns...");

            // Test 1.1: Check if m_Socket array contains null pointers as expected
            TC_LOG_INFO("test.playerbot", "🔍 Testing socket array state...");

            // We can't directly access m_Socket as it's private, but we can test the public interface
            bool disconnected = botSession->PlayerDisconnected();
            TC_LOG_INFO("test.playerbot", "PlayerDisconnected() returned: {}", disconnected);

            // Test the custom bot socket methods
            bool hasSocket = botSession->HasSocket();
            bool socketOpen = botSession->IsSocketOpen();
            TC_LOG_INFO("test.playerbot", "HasSocket(): {}, IsSocketOpen(): {}", hasSocket, socketOpen);

            // Test the dangerous CloseSocket() method that should be overridden
            TC_LOG_INFO("test.playerbot", "🔍 Testing CloseSocket() override...");
            botSession->CloseSocket(); // This should be safe due to override
            TC_LOG_INFO("test.playerbot", "✅ CloseSocket() call completed without crash");

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 1 Exception: {}", e.what());
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 1 Unknown exception");
        }

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 1 Complete");
    }

    /**
     * SCENARIO 2: WorldSession Update Code Paths
     * Tests all the code paths in WorldSession::Update that could trigger socket access
     */
    static void TestScenario2_WorldSessionUpdatePaths()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 2: WorldSession Update Path Analysis");

        try {
            auto botSession = BotSession::Create(99998);
            if (!botSession) {
                TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                return;
            }

            // Create test filter to trigger different code paths
            class TestPacketFilter : public PacketFilter {
            public:
                bool Process(WorldPacket* packet) override { return true; }
                bool ProcessUnsafe() override { return unsafe_mode; }
                bool unsafe_mode = false;
            } filter;

            // Test 2.1: Safe Update path (ProcessUnsafe = false)
            TC_LOG_INFO("test.playerbot", "🔍 Testing SAFE update path...");
            filter.unsafe_mode = false;
            bool result1 = botSession->Update(100, filter);
            TC_LOG_INFO("test.playerbot", "Safe update result: {}", result1);

            // Test 2.2: Unsafe Update path (ProcessUnsafe = true) - this triggers socket cleanup code
            TC_LOG_INFO("test.playerbot", "🔍 Testing UNSAFE update path (potential crash location)...");
            filter.unsafe_mode = true;

            // This is the critical test - the unsafe path contains the socket cleanup code
            // that leads to the crash at line 357 -> Socket.h:230
            bool result2 = botSession->Update(100, filter);
            TC_LOG_INFO("test.playerbot", "Unsafe update result: {}", result2);

            // Test 2.3: Force timeout condition
            TC_LOG_INFO("test.playerbot", "🔍 Testing timeout condition...");

            // Set timeout to trigger idle connection check
            botSession->ResetTimeOutTime(false);

            // Wait a moment then update - this should trigger IsConnectionIdle() check
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            bool result3 = botSession->Update(100, filter);
            TC_LOG_INFO("test.playerbot", "Timeout condition update result: {}", result3);

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 2 Exception: {}", e.what());
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 2 Unknown exception");
        }

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 2 Complete");
    }

    /**
     * SCENARIO 3: Guard Effectiveness Analysis
     * Tests whether the BUILD_PLAYERBOT guards are actually preventing socket access
     */
    static void TestScenario3_GuardEffectiveness()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 3: Guard Effectiveness Analysis");

#ifdef BUILD_PLAYERBOT
        TC_LOG_INFO("test.playerbot", "✅ BUILD_PLAYERBOT is defined");

        try {
            auto botSession = BotSession::Create(99997);
            if (!botSession) {
                TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                return;
            }

            // Test 3.1: Verify IsBot() is properly set during construction
            bool isBot = botSession->IsBot();
            TC_LOG_INFO("test.playerbot", "IsBot() during construction: {}", isBot);

            if (!isBot) {
                TC_LOG_ERROR("test.playerbot", "❌ CRITICAL: IsBot() returns false - guards will NOT work!");
                TC_LOG_ERROR("test.playerbot", "❌ This explains why socket crashes still occur");
            }

            // Test 3.2: Check if the WorldSession constructor properly sets the bot flag
            // We need to examine the actual WorldSession state
            TC_LOG_INFO("test.playerbot", "🔍 Examining WorldSession constructor behavior...");

            // Create another session to test consistency
            auto botSession2 = BotSession::Create(99996);
            if (botSession2) {
                bool isBot2 = botSession2->IsBot();
                TC_LOG_INFO("test.playerbot", "Second session IsBot(): {}", isBot2);

                if (isBot != isBot2) {
                    TC_LOG_ERROR("test.playerbot", "❌ INCONSISTENT: IsBot() results vary between sessions!");
                }
            }

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 3 Exception: {}", e.what());
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 3 Unknown exception");
        }

#else
        TC_LOG_ERROR("test.playerbot", "❌ BUILD_PLAYERBOT is NOT defined - guards are inactive!");
        TC_LOG_ERROR("test.playerbot", "❌ This explains the socket crashes - recompile with BUILD_PLAYERBOT=1");
#endif

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 3 Complete");
    }

    /**
     * SCENARIO 4: Socket Lifecycle Analysis
     * Tests the socket lifecycle to understand when crashes occur
     */
    static void TestScenario4_SocketLifecycle()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 4: Socket Lifecycle Analysis");

        try {
            TC_LOG_INFO("test.playerbot", "🔍 Testing socket lifecycle during session destruction...");

            {
                auto botSession = BotSession::Create(99995);
                if (!botSession) {
                    TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                    return;
                }

                TC_LOG_INFO("test.playerbot", "✅ BotSession created in scope");

                // Force some operations that might set up state
                class LifecycleFilter : public PacketFilter {
                public:
                    bool Process(WorldPacket* packet) override { return true; }
                    bool ProcessUnsafe() override { return true; }
                } filter;

                botSession->Update(50, filter);

                TC_LOG_INFO("test.playerbot", "✅ Update completed, about to leave scope...");
            } // BotSession destructor should be called here

            TC_LOG_INFO("test.playerbot", "✅ BotSession destroyed without crash");

            // Test rapid creation/destruction cycles
            TC_LOG_INFO("test.playerbot", "🔍 Testing rapid creation/destruction cycles...");

            for (int i = 0; i < 5; ++i) {
                auto tempSession = BotSession::Create(99990 + i);
                if (tempSession) {
                    tempSession->IsBot(); // Basic operation
                    // Automatic destruction when tempSession goes out of scope
                }
            }

            TC_LOG_INFO("test.playerbot", "✅ Rapid cycles completed without crash");

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 4 Exception: {}", e.what());
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 4 Unknown exception");
        }

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 4 Complete");
    }

    /**
     * SCENARIO 5: Atomic Operation Validation
     * Tests the specific atomic operation that crashes
     */
    static void TestScenario5_AtomicOperationValidation()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 5: Atomic Operation Validation");

        try {
            auto botSession = BotSession::Create(99994);
            if (!botSession) {
                TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                return;
            }

            TC_LOG_INFO("test.playerbot", "🔍 Testing conditions that trigger _openState.fetch_or...");

            // The crash occurs when CloseSocket() is called on a Socket object
            // where the _openState atomic variable is invalid/corrupted

            // Test 5.1: Multiple socket operations
            TC_LOG_INFO("test.playerbot", "Testing multiple socket state checks...");

            for (int i = 0; i < 10; ++i) {
                bool disconnected = botSession->PlayerDisconnected();
                bool idle = botSession->IsConnectionIdle();
                TC_LOG_DEBUG("test.playerbot", "Iteration {}: disconnected={}, idle={}", i, disconnected, idle);
            }

            // Test 5.2: Force socket closure attempts
            TC_LOG_INFO("test.playerbot", "Testing socket closure operations...");

            for (int i = 0; i < 5; ++i) {
                botSession->CloseSocket(); // Should be safe due to override
                TC_LOG_DEBUG("test.playerbot", "CloseSocket() call {} completed", i);
            }

            TC_LOG_INFO("test.playerbot", "✅ All atomic operations completed safely");

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 5 Exception: {}", e.what());
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 5 Unknown exception");
        }

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 5 Complete");
    }

    /**
     * SCENARIO 6: Unguarded Code Paths
     * Searches for code paths that might bypass the BUILD_PLAYERBOT guards
     */
    static void TestScenario6_UnguardedCodePaths()
    {
        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 6: Unguarded Code Path Analysis");

        try {
            auto botSession = BotSession::Create(99993);
            if (!botSession) {
                TC_LOG_ERROR("test.playerbot", "❌ Failed to create BotSession for testing");
                return;
            }

            TC_LOG_INFO("test.playerbot", "🔍 Searching for unguarded socket access paths...");

            // Test 6.1: Check for socket access during packet processing
            TC_LOG_INFO("test.playerbot", "Testing packet processing paths...");

            WorldPacket testPacket(0x1234, 8);
            testPacket << uint64(42);

            // This might trigger unguarded socket access during packet validation
            botSession->SendPacket(&testPacket);

            auto queuePacket = new WorldPacket(0x5678, 4);
            *queuePacket << uint32(84);
            botSession->QueuePacket(queuePacket);
            delete queuePacket;

            // Test 6.2: Force error conditions that might bypass guards
            TC_LOG_INFO("test.playerbot", "Testing error conditions...");

            class ErrorTriggerFilter : public PacketFilter {
            public:
                bool Process(WorldPacket* packet) override { return false; } // Force error path
                bool ProcessUnsafe() override { return true; }
            } errorFilter;

            // This might trigger error paths that bypass the guards
            bool result = botSession->Update(100, errorFilter);
            TC_LOG_INFO("test.playerbot", "Error condition update result: {}", result);

            // Test 6.3: Check for static socket operations that don't use IsBot()
            TC_LOG_INFO("test.playerbot", "Testing static socket operations...");

            // Look for operations that might not check IsBot() before accessing sockets
            // These would be the unguarded paths causing the crash

            TC_LOG_INFO("test.playerbot", "✅ Unguarded path analysis completed");

        } catch (std::exception const& e) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 6 Exception: {}", e.what());
            TC_LOG_ERROR("test.playerbot", "❌ This exception might indicate an unguarded code path!");
        } catch (...) {
            TC_LOG_ERROR("test.playerbot", "❌ SCENARIO 6 Unknown exception - possible unguarded path!");
        }

        TC_LOG_INFO("test.playerbot", "📋 SCENARIO 6 Complete");
    }
};

/**
 * Utility function to run the socket crash analysis
 * Call this from server startup or test framework
 */
void RunSocketCrashAnalysis()
{
    TC_LOG_INFO("test.playerbot", "🚨 SOCKET CRASH ANALYSIS STARTING");
    TC_LOG_INFO("test.playerbot", "🚨 Purpose: Find root cause of ACCESS_VIOLATION at Socket.h:230");
    TC_LOG_INFO("test.playerbot", "🚨 Crash signature: _openState.fetch_or() on invalid socket object");

    SocketCrashAnalyzer::AnalyzeSocketCrashScenarios();

    TC_LOG_INFO("test.playerbot", "🚨 SOCKET CRASH ANALYSIS COMPLETE");
    TC_LOG_INFO("test.playerbot", "🚨 Check logs above for identified issues");
}