"""Convert a mysqldump SQL file to INSERT IGNORE format.

Strips DDL (DROP TABLE, CREATE TABLE) and converts INSERT INTO → INSERT IGNORE INTO.
Processes files streaming (line by line) so it can handle 600MB+ files.

Usage: python tdb_to_insert_ignore.py <input.sql> <output.sql> [--skip-tables TABLE1,TABLE2]
"""
import sys
import re
import time

def convert(input_path, output_path, skip_tables=None):
    skip_tables = set(t.lower() for t in (skip_tables or []))

    in_create_block = False
    current_table = None
    skipping_table_data = False

    lines_read = 0
    lines_written = 0
    tables_seen = set()
    tables_skipped = set()
    inserts_converted = 0

    start = time.time()
    last_report = start

    with open(input_path, 'r', encoding='utf-8', errors='replace') as fin, \
         open(output_path, 'w', encoding='utf-8') as fout:

        # Write header
        fout.write("-- Converted from TC TDB to INSERT IGNORE format\n")
        fout.write("-- Source: {}\n".format(input_path.split('/')[-1].split('\\')[-1]))
        fout.write("-- Generated: {}\n\n".format(time.strftime('%Y-%m-%d %H:%M:%S')))
        fout.write("SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;\n")
        fout.write("SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;\n\n")

        for line in fin:
            lines_read += 1

            # Progress report every 30 seconds
            now = time.time()
            if now - last_report > 30:
                elapsed = now - start
                print(f"  Progress: {lines_read:,} lines read, {inserts_converted:,} INSERTs converted, "
                      f"{len(tables_seen)} tables seen, {elapsed:.0f}s elapsed", file=sys.stderr)
                last_report = now

            stripped = line.strip()

            # Skip empty lines and comments (but keep useful comments)
            if not stripped or stripped.startswith('--'):
                continue

            # Skip mysqldump session variable settings
            if stripped.startswith('/*!') and ('SET' in stripped or 'character_set' in stripped.lower()):
                continue

            # Detect table name from DROP TABLE
            m = re.match(r"DROP TABLE IF EXISTS `([^`]+)`", stripped)
            if m:
                current_table = m.group(1)
                tables_seen.add(current_table)
                skipping_table_data = current_table.lower() in skip_tables
                if skipping_table_data:
                    tables_skipped.add(current_table)
                continue  # Skip the DROP TABLE statement

            # Skip CREATE TABLE blocks
            if stripped.startswith('CREATE TABLE'):
                in_create_block = True
                continue
            if in_create_block:
                if stripped.endswith(';'):
                    in_create_block = False
                continue

            # Skip if we're skipping this table's data
            if skipping_table_data:
                if stripped.startswith('LOCK TABLES') or stripped.startswith('UNLOCK TABLES'):
                    if 'UNLOCK' in stripped:
                        skipping_table_data = False
                continue

            # Convert INSERT INTO → INSERT IGNORE INTO
            if stripped.startswith('INSERT INTO'):
                line = line.replace('INSERT INTO', 'INSERT IGNORE INTO', 1)
                inserts_converted += 1
                fout.write(line)
                lines_written += 1
                continue

            # Keep LOCK/UNLOCK and DISABLE/ENABLE KEYS for performance
            if any(stripped.startswith(prefix) for prefix in [
                'LOCK TABLES', 'UNLOCK TABLES',
            ]):
                fout.write(line)
                lines_written += 1
                continue

            # Keep ALTER TABLE DISABLE/ENABLE KEYS
            if 'DISABLE KEYS' in stripped or 'ENABLE KEYS' in stripped:
                fout.write(line)
                lines_written += 1
                continue

        # Footer
        fout.write("\nSET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;\n")
        fout.write("SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;\n")

    elapsed = time.time() - start
    print(f"\nConversion complete in {elapsed:.1f}s:", file=sys.stderr)
    print(f"  Lines read:      {lines_read:,}", file=sys.stderr)
    print(f"  Lines written:   {lines_written:,}", file=sys.stderr)
    print(f"  INSERTs converted: {inserts_converted:,}", file=sys.stderr)
    print(f"  Tables seen:     {len(tables_seen)}", file=sys.stderr)
    print(f"  Tables skipped:  {len(tables_skipped)} {tables_skipped if tables_skipped else ''}", file=sys.stderr)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python tdb_to_insert_ignore.py <input.sql> <output.sql> [--skip-tables TABLE1,TABLE2]")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    skip_tables = []
    for i, arg in enumerate(sys.argv):
        if arg == '--skip-tables' and i + 1 < len(sys.argv):
            skip_tables = sys.argv[i + 1].split(',')

    convert(input_path, output_path, skip_tables)
