#!/usr/bin/env python3
"""Categorize Table-prefix errors from DBErrors.log."""
import re
from collections import Counter

LOG = r"out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/DBErrors.log"
table_types = Counter()
count = 0

with open(LOG, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if line.startswith("Table "):
            # Extract table name pattern
            m = re.match(r"Table [`']([^`']+)[`']", line)
            if m:
                tbl = m.group(1)
            elif "hash" in line[:30]:
                tbl = "hash_redirect"
            else:
                tbl = "unknown"

            # Categorize the error type
            if "points to a loaded DB2" in line:
                key = f"{tbl}:DB2_store_redirect"
            elif "does not exist" in line:
                key = f"{tbl}:does_not_exist"
            elif "has no entry" in line:
                key = f"{tbl}:no_entry"
            elif "entry not found" in line:
                key = f"{tbl}:entry_not_found"
            elif "loot entry" in line.lower():
                key = f"{tbl}:loot_entry"
            else:
                snippet = line[len("Table ") + len(tbl) + 3:80].strip()
                key = f"{tbl}:{snippet[:50]}"

            table_types[key] += 1
            count += 1

print(f"Total Table-prefix lines: {count:,}\n")
print(f"{'Category':<70} {'Count':>10}")
print("-" * 82)
for k, c in table_types.most_common(50):
    print(f"{k:<70} {c:>10,}")
