---
allowed-tools: Bash(ls:*), Bash(date:*), Write
description: Create a new correctly-named SQL update file with the next sequence number
paths: sql/**/*.sql
---

## Context

SQL update files follow the naming convention: `YYYY_MM_DD_NN_<db>.sql`
- `YYYY_MM_DD` — today's date
- `NN` — two-digit sequence number, starting at 00 for each day, incrementing
- `<db>` — the database name (world, auth, characters, hotfixes)
- Location: `sql/updates/<db>/master/`

## Arguments

$ARGUMENTS should contain the database name: `world`, `auth`, `characters`, or `hotfixes`

Optionally, the user may append a brief description after the db name (e.g., `world fix creature factions`). If provided, use it as a comment at the top of the file.

Example: `/new-sql-update world fix missing spawns`

## Your task

1. Parse the database name from $ARGUMENTS (first word). Validate it's one of: world, auth, characters, hotfixes
2. Get today's date in YYYY_MM_DD format
3. List existing files in `sql/updates/<db>/master/` matching today's date to determine the next sequence number:
   - `ls sql/updates/<db>/master/ | grep "^$(date +%Y_%m_%d)" | sort | tail -1`
   - If no files exist for today, use 00
   - Otherwise increment the last NN by 1 (zero-padded)
4. Create the file at `sql/updates/<db>/master/YYYY_MM_DD_NN_<db>.sql` with content:
   ```
   -- YYYY_MM_DD_NN_<db>.sql
   -- <description if provided, otherwise empty>

   ```
5. Report the created file path so the user can start editing
