---
allowed-tools: Bash(curl*), Bash(gh*), Bash(mysql*), Bash(powershell*), Bash(python*), Bash(7z*), Bash(tar*), Bash(unzip*), Read, Write, Grep, Glob
description: Download latest TrinityCore TDB release, extract a table, diff against local DB, and generate update SQL
paths: sql/**/*.sql
---

# TDB Diff

Compare a local database table against the latest TrinityCore TDB release to find missing or divergent rows.

## Arguments

$ARGUMENTS — required: table name (e.g., `spell_script_names`, `creature_template`, `conditions`) and optionally `--download` to force re-download

## Process

### Step 1: Find latest TDB release

```bash
gh release list --repo TrinityCore/TrinityCore --limit 5
```

Look for the latest `TDB` release (usually tagged like `TDB335.24081` or `TDB_full_*`). If no TDB-specific release, check:
```bash
gh release view --repo TrinityCore/TrinityCore --json assets --jq '.assets[].name' | grep -i tdb
```

### Step 2: Download TDB (if needed)

Check if we already have the TDB cached:
```bash
ls -la ExtTools/TDB/*.sql 2>/dev/null
```

If not cached or `--download` specified:
```bash
mkdir -p ExtTools/TDB
gh release download <tag> --repo TrinityCore/TrinityCore --pattern "*.sql*" --dir ExtTools/TDB
# Or for compressed:
gh release download <tag> --repo TrinityCore/TrinityCore --pattern "*.7z" --dir ExtTools/TDB
```

If compressed, extract:
```bash
7z x ExtTools/TDB/*.7z -oExtTools/TDB/
# or
tar xzf ExtTools/TDB/*.tar.gz -C ExtTools/TDB/
```

### Step 3: Extract target table from TDB

Search for the table's data in the TDB SQL dump:
```bash
# Find the INSERT block for the target table
grep -n "INSERT INTO \`$TABLE\`" ExtTools/TDB/*.sql | head -5
```

Extract just that table's data into a temp file. For large tables, use sed/awk to extract the block between the table's DROP/CREATE and the next table's DROP/CREATE.

### Step 4: Extract local table data

```bash
mysql -u root -padmin -N -e "SELECT * FROM \`$TABLE\`" world > /tmp/local_$TABLE.tsv
mysql -u root -padmin -N -e "SELECT COUNT(*) FROM \`$TABLE\`" world
```

Also get the local table schema:
```bash
mysql -u root -padmin -e "DESCRIBE \`$TABLE\`" world
```

### Step 5: Diff and analyze

Compare row counts and identify:
- **Missing in local**: Rows in TDB but not in our DB (by primary key)
- **Missing in TDB**: Rows in our DB but not in TDB (custom additions — preserve these)
- **Divergent**: Same primary key but different values

For small tables (<10K rows), do a full row-by-row comparison.
For large tables, compare by primary key first, then spot-check divergent rows.

### Step 6: Generate update SQL

Write to `sql/updates/pending/tdb_diff_[table]_[date].sql`:

```sql
-- ============================================================================
-- TDB Diff: [table] — [date]
-- Source: TrinityCore TDB [release tag]
-- Local: [row count] rows | TDB: [row count] rows
-- Missing: [N] | Divergent: [N] | Custom (preserved): [N]
-- ============================================================================

-- Missing rows (in TDB but not local)
INSERT INTO `[table]` VALUES (...);

-- Divergent rows (TDB values, commented for review)
-- UPDATE `[table]` SET ... WHERE ...;
```

**Important**: Divergent rows are COMMENTED OUT by default — they need manual review since our values may be intentional overrides.

### Step 7: Report

```
## TDB Diff: [table]

| Metric | Count |
|--------|-------|
| Local rows | N |
| TDB rows | N |
| Missing (will add) | N |
| Divergent (needs review) | N |
| Custom (preserved) | N |

### Generated
`sql/updates/pending/tdb_diff_[table]_[date].sql`

### Next Steps
- Review divergent rows (commented UPDATE statements)
- Apply: `/apply-sql sql/updates/pending/tdb_diff_[table]_[date].sql world`
```

## Rules
- NEVER delete custom rows (entries in VoxCore ranges: 400000-499999, 500001-500005, 1900003+)
- Divergent UPDATE statements must be COMMENTED OUT for manual review
- Cache TDB downloads in `ExtTools/TDB/` — don't re-download every run
- If the table doesn't exist in TDB, report it and exit
- For tables with composite primary keys, build the comparison on all PK columns
- Skip binary/blob columns in the diff (they produce unreadable output)
