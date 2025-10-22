#!/usr/bin/env python3
"""
Analyze the std::future pattern in BotWorldSessionMgr.cpp to identify potential issues
"""

import re
from pathlib import Path

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Session\BotWorldSessionMgr.cpp")

def analyze_lambda_returns(content):
    """Find all return statements in the updateLogic lambda"""

    # Find the lambda definition
    lambda_start = content.find("auto updateLogic = [")
    if lambda_start == -1:
        print("❌ Could not find updateLogic lambda")
        return

    # Find the matching closing brace
    lambda_end = content.find("};", lambda_start) + 2
    lambda_code = content[lambda_start:lambda_end]

    # Count lines for line numbers
    lines_before = content[:lambda_start].count('\n')

    # Find all return statements
    returns = []
    for match in re.finditer(r'return;', lambda_code):
        line_num = lines_before + lambda_code[:match.start()].count('\n')
        # Get context
        line_start = lambda_code.rfind('\n', 0, match.start()) + 1
        line_end = lambda_code.find('\n', match.end())
        line_text = lambda_code[line_start:line_end].strip()
        returns.append((line_num, line_text))

    print("=" * 80)
    print("Lambda Return Statement Analysis")
    print("=" * 80)
    print(f"\nLambda starts at line {lines_before}")
    print(f"Lambda definition: auto updateLogic = [guid, botSession, diff, currentTime, enterpriseMode, tickCounter, &botsUpdated, &disconnectionQueue, &disconnectionMutex]()")
    print(f"Return type: void (implicit)")
    print(f"\nFound {len(returns)} early return statements:\n")

    for line_num, line_text in returns:
        # Remove Unicode characters for console output
        line_text_clean = line_text.encode('ascii', 'ignore').decode('ascii')
        print(f"Line {line_num}: {line_text_clean}")

    # Check if lambda ends properly
    if "TC_LOG_TRACE" in lambda_code and "TASK END" in lambda_code:
        print("\n✅ Lambda has proper TASK END logging at the end")
    else:
        print("\n❌ Lambda missing TASK END logging")

    # Check if future.get() is called
    if "future.get()" in content:
        print("✅ future.get() is called to retrieve result and propagate exceptions")
    else:
        print("❌ future.get() is NOT called - exceptions won't propagate")

    # Check for timeout handling
    if "future.wait_for" in content and "std::future_status::ready" in content:
        print("✅ Timeout handling with wait_for() is implemented")
    else:
        print("❌ No timeout handling")

    print("\n" + "=" * 80)
    print("Diagnosis")
    print("=" * 80)
    print("""
The early return statements in the lambda are CORRECT behavior:
- Lambda return type is void
- std::packaged_task wraps the lambda
- When lambda returns (early or normally), the packaged_task completes the future
- The future becomes ready with void result
- This is standard C++ behavior and should work correctly

If futures are timing out, the problem is NOT the early returns.
Possible actual causes:
1. Worker threads are sleeping and not waking up
2. Tasks are not being executed at all
3. ThreadPool queue is full and tasks are rejected
4. Worker threads have crashed/deadlocked
5. Exception in worker thread before lambda executes

To diagnose, check:
- ThreadPool worker thread status
- Task queue sizes
- Worker sleep/wake statistics
- Exception logs from worker threads
""")

def main():
    if not FILE_PATH.exists():
        print(f"❌ File not found: {FILE_PATH}")
        return

    with open(FILE_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    analyze_lambda_returns(content)

if __name__ == "__main__":
    main()
