#!/usr/bin/env python3
"""
Fix for ThreadPool Deadlock Pattern #3: Work Stealing vs Sleep Race Condition
Date: 2025-10-20

This script implements the multi-part fix for the third deadlock pattern.
"""

import os
import re
import sys
from pathlib import Path

def fix_worker_sleep_time():
    """Fix C: Increase worker sleep time from 1ms to 10ms"""
    print("Applying Fix C: Increasing worker sleep time...")

    file_path = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.h")
    if not file_path.exists():
        print(f"ERROR: File not found: {file_path}")
        return False

    content = file_path.read_text(encoding='utf-8')

    # Find and replace the workerSleepTime configuration
    pattern = r'(std::chrono::milliseconds workerSleepTime\{)1(\}; // Sleep time when idle)'
    replacement = r'\g<1>10\g<2>'

    new_content = re.sub(pattern, replacement, content)

    if new_content != content:
        file_path.write_text(new_content, encoding='utf-8')
        print(f"  [OK] Updated workerSleepTime from 1ms to 10ms in {file_path}")
        return True
    else:
        print(f"  [WARNING] workerSleepTime already updated or pattern not found")
        return False

def fix_wake_multiple_workers():
    """Fix A: Wake multiple workers on task submission"""
    print("Applying Fix A: Wake multiple workers...")

    file_path = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.h")
    if not file_path.exists():
        print(f"ERROR: File not found: {file_path}")
        return False

    content = file_path.read_text(encoding='utf-8')

    # Find the Submit() method and add multi-worker wake
    wake_fix = '''        // Wake worker if sleeping
        worker->Wake();

        // CRITICAL FIX #3: Wake additional workers for better work distribution
        // This prevents work starvation when tasks pile up on one worker
        if (_config.enableWorkStealing && !_workers.empty())
        {
            // Wake 25% of workers (minimum 2, maximum 4) to help with work distribution
            uint32 workersToWake = std::min(4u, std::max(2u, static_cast<uint32>(_workers.size()) / 4));

            for (uint32 i = 0; i < workersToWake; ++i)
            {
                uint32 randomWorker = SelectWorkerRoundRobin();
                if (randomWorker != workerId)
                {
                    WorkerThread* helperWorker = _workers[randomWorker].get();
                    if (helperWorker && helperWorker->_sleeping.load(std::memory_order_relaxed))
                    {
                        helperWorker->Wake();
                    }
                }
            }
        }'''

    # Replace the existing wake section
    pattern = r'(        // Wake worker if sleeping\s+worker->Wake\(\);)'

    if re.search(pattern, content):
        new_content = re.sub(pattern, wake_fix, content)

        if new_content != content:
            file_path.write_text(new_content, encoding='utf-8')
            print(f"  [OK]Added multi-worker wake logic to Submit() in {file_path}")
            return True

    print(f"  [WARNING]Multi-worker wake already added or pattern not found")
    return False

def fix_interruptible_backoff():
    """Fix B: Make exponential backoff interruptible"""
    print("Applying Fix B: Interruptible exponential backoff...")

    cpp_file = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.cpp")
    h_file = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.h")

    if not cpp_file.exists() or not h_file.exists():
        print(f"ERROR: Required files not found")
        return False

    # First, add the _stealBackoff flag to WorkerThread in the header
    h_content = h_file.read_text(encoding='utf-8')

    # Add _stealBackoff atomic flag after _sleeping
    if '_stealBackoff' not in h_content:
        pattern = r'(alignas\(64\) std::atomic<bool> _sleeping\{false\};)'
        replacement = r'''\g<1>
    alignas(64) std::atomic<bool> _stealBackoff{false};  // CRITICAL FIX #3: Interruptible backoff'''

        h_content = re.sub(pattern, replacement, h_content)
        h_file.write_text(h_content, encoding='utf-8')
        print(f"  [OK]Added _stealBackoff flag to WorkerThread")

    # Now fix the TryStealTask implementation
    cpp_content = cpp_file.read_text(encoding='utf-8')

    # Find and replace the exponential backoff section
    old_backoff = r'''        \+\+attempts;

        // Exponential backoff to reduce contention
        if \(attempts < maxAttempts\)
        \{
            std::this_thread::sleep_for\(std::chrono::microseconds\(backoffUs\)\);
            backoffUs = std::min\(backoffUs \* 2, 1000u\);  // Cap at 1ms
        \}'''

    new_backoff = '''        ++attempts;

        // CRITICAL FIX #3: Interruptible exponential backoff
        // Allows thread to wake immediately when new work arrives
        if (attempts < maxAttempts)
        {
            std::unique_lock<std::mutex> backoffLock(_wakeMutex);
            _stealBackoff.store(true, std::memory_order_relaxed);

            // Wait with backoff but allow wake interruption
            _wakeCv.wait_for(backoffLock, std::chrono::microseconds(backoffUs), [this]() {
                // Wake if: new work signaled, backoff cleared, or shutdown
                return !_stealBackoff.load(std::memory_order_relaxed) ||
                       !_running.load(std::memory_order_relaxed) ||
                       _pool->IsShuttingDown();
            });

            _stealBackoff.store(false, std::memory_order_relaxed);
            backoffUs = std::min(backoffUs * 2, 1000u);  // Cap at 1ms
        }'''

    if 'std::this_thread::sleep_for(std::chrono::microseconds(backoffUs))' in cpp_content:
        cpp_content = re.sub(old_backoff, new_backoff, cpp_content, flags=re.MULTILINE)

        # Also update Wake() to clear steal backoff
        wake_pattern = r'(void WorkerThread::Wake\(\)\s*\{[^}]+_sleeping\.store\(false, std::memory_order_relaxed\);)'
        wake_replacement = r'''\g<1>
    _stealBackoff.store(false, std::memory_order_relaxed);  // CRITICAL FIX #3: Clear steal backoff'''

        cpp_content = re.sub(wake_pattern, wake_replacement, cpp_content, flags=re.DOTALL)

        cpp_file.write_text(cpp_content, encoding='utf-8')
        print(f"  [OK]Made exponential backoff interruptible in TryStealTask()")
        return True

    print(f"  [WARNING]Interruptible backoff already implemented or pattern not found")
    return False

def fix_better_work_detection():
    """Fix D: Add better global work detection"""
    print("Applying Fix D: Better work detection...")

    file_path = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.h")
    if not file_path.exists():
        print(f"ERROR: File not found: {file_path}")
        return False

    content = file_path.read_text(encoding='utf-8')

    # Add HasWorkAvailable method to WorkerThread
    new_method = '''    /**
     * @brief Check if any work is available (own queues or stealable)
     * CRITICAL FIX #3: Better work detection to prevent unnecessary sleeping
     */
    bool HasWorkAvailable() const
    {
        // Check own queues first (fast path)
        for (size_t i = 0; i < static_cast<size_t>(TaskPriority::COUNT); ++i)
        {
            if (!_localQueues[i].Empty())
                return true;
        }

        // Check if work stealing is enabled and other workers have work
        if (_pool->GetConfiguration().enableWorkStealing)
        {
            for (uint32 i = 0; i < _pool->GetWorkerCount(); ++i)
            {
                if (i == _workerId)
                    continue;  // Skip self

                WorkerThread* other = _pool->GetWorker(i);
                if (!other)
                    continue;

                // Check if other worker has stealable work
                for (size_t j = 0; j < static_cast<size_t>(TaskPriority::COUNT); ++j)
                {
                    if (!other->_localQueues[j].Empty())
                        return true;  // Work available to steal
                }
            }
        }

        return false;  // No work available anywhere
    }
'''

    # Insert the method after GetMetrics()
    pattern = r'(    MetricsSnapshot GetMetrics\(\) const\s*\{[^}]+\})'
    replacement = f'{new_method}\n\n\\g<1>'

    if 'HasWorkAvailable' not in content:
        new_content = re.sub(pattern, replacement, content, flags=re.DOTALL)

        if new_content != content:
            file_path.write_text(new_content, encoding='utf-8')
            print(f"  [OK]Added HasWorkAvailable() method to WorkerThread")
            return True

    print(f"  [WARNING]HasWorkAvailable() already exists")
    return False

def update_sleep_logic():
    """Update Sleep() to use HasWorkAvailable()"""
    print("Updating Sleep() logic to use HasWorkAvailable()...")

    file_path = Path("src/modules/Playerbot/Performance/ThreadPool/ThreadPool.cpp")
    if not file_path.exists():
        print(f"ERROR: File not found: {file_path}")
        return False

    content = file_path.read_text(encoding='utf-8')

    # Replace the manual work checking with HasWorkAvailable()
    old_check = r'''    // Double-check for work before sleeping \(prevents unnecessary sleep\)
    bool hasWork = false;
    for \(size_t i = 0; i < static_cast<size_t>\(TaskPriority::COUNT\); \+\+i\)
    \{
        if \(!_localQueues\[i\]\.Empty\(\)\)
        \{
            hasWork = true;
            break;
        \}
    \}'''

    new_check = '''    // CRITICAL FIX #3: Use comprehensive work detection
    // Check both local queues and stealable work from other workers
    bool hasWork = HasWorkAvailable();'''

    if 'Double-check for work before sleeping' in content:
        new_content = re.sub(old_check, new_check, content, flags=re.MULTILINE)

        if new_content != content:
            file_path.write_text(new_content, encoding='utf-8')
            print(f"  [OK]Updated Sleep() to use HasWorkAvailable()")
            return True

    print(f"  [WARNING]Sleep() already uses HasWorkAvailable() or pattern not found")
    return False

def main():
    """Apply all fixes for ThreadPool Deadlock #3"""
    print("=" * 80)
    print("ThreadPool Deadlock #3 Fix Script")
    print("Fixing: Work Stealing vs Sleep Race Condition")
    print("=" * 80)

    # Change to TrinityCore root directory
    trinity_root = Path("C:/TrinityBots/TrinityCore")
    if trinity_root.exists():
        os.chdir(trinity_root)
        print(f"Working directory: {trinity_root}")
    else:
        print(f"ERROR: TrinityCore root not found at {trinity_root}")
        return 1

    fixes_applied = 0

    # Apply fixes in order of priority
    if fix_worker_sleep_time():
        fixes_applied += 1

    if fix_wake_multiple_workers():
        fixes_applied += 1

    if fix_interruptible_backoff():
        fixes_applied += 1

    if fix_better_work_detection():
        fixes_applied += 1

    if update_sleep_logic():
        fixes_applied += 1

    print("=" * 80)
    if fixes_applied > 0:
        print(f"[SUCCESS]Successfully applied {fixes_applied} fixes")
        print("\nNext steps:")
        print("1. Rebuild the project")
        print("2. Test with 100+ bots")
        print("3. Monitor thread states in debugger")
        print("4. Verify no threads stuck in sleep > 10ms")
    else:
        print("[WARNING]No fixes were applied (may already be fixed)")

    return 0

if __name__ == "__main__":
    sys.exit(main())