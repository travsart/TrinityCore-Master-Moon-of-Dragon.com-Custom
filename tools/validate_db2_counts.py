"""
DB2 Record Count Validator — Phase 1 of Data Integrity Validation

Compares runtime WDC5 binary DB2 record counts against Wago merged CSV
ground truth to prove data completeness after the TrinityCore migration.

WDC5 total records = header.RecordCount + sum(section.CopyTableCount)

Usage: python tools/validate_db2_counts.py
"""

import os
import struct
import sys
from pathlib import Path

DB2_DIR = Path(r"C:\Users\atayl\VoxCore\runtime\dbc\enUS")
CSV_DIR = Path(r"C:\Users\atayl\VoxCore\wago\merged_csv\12.0.1.66666\enUS")

CRITICAL_TABLES = {
    "SpellName", "SpellEffect", "SpellMisc", "SpellAuraOptions",
    "ItemSparse", "Talent", "TraitNodeEntry", "TraitDefinition",
    "TraitNode", "TraitCond", "ChrSpecialization", "ChrRaces",
    "SkillLineAbility", "CreatureDisplayInfo",
}


def get_db2_total(filepath: Path) -> tuple[int | None, str | None]:
    """Read WDC5 header + section headers, return total record count."""
    try:
        with open(filepath, "rb") as f:
            data = f.read(16384)  # enough for headers with many sections
    except Exception as e:
        return None, str(e)

    if len(data) < 204:
        return None, "File too small for WDC5 header"

    magic = data[0:4]
    if magic != b"WDC5":
        return None, f"Not WDC5 (magic={magic})"

    record_count = struct.unpack_from("<I", data, 136)[0]
    section_count = struct.unpack_from("<I", data, 200)[0]

    needed = 204 + section_count * 40
    if len(data) < needed:
        return None, f"Buffer too small for {section_count} sections (need {needed})"

    total_copies = 0
    for i in range(section_count):
        off = 204 + i * 40
        total_copies += struct.unpack_from("<I", data, off + 36)[0]

    return record_count + total_copies, None


def count_csv_rows(filepath: Path) -> int:
    """Count data rows in a CSV (total lines minus header)."""
    count = 0
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        for _ in f:
            count += 1
    return max(count - 1, 0)  # minus header


def main():
    db2_files = sorted(DB2_DIR.glob("*.db2"))
    csv_files = {p.stem.replace("-enUS", ""): p for p in CSV_DIR.glob("*-enUS.csv")}

    print(f"Runtime DB2 files: {len(db2_files)}")
    print(f"Wago merged CSVs:  {len(csv_files)}")
    print()

    results = []  # (name, db2_count, wago_count, diff, pct, status, note)
    missing_csv = []
    missing_db2 = []
    errors = []

    for db2_path in db2_files:
        name = db2_path.stem
        total, err = get_db2_total(db2_path)

        if err:
            errors.append((name, err))
            continue

        csv_name = name
        if csv_name not in csv_files:
            missing_csv.append((name, total))
            continue

        wago_count = count_csv_rows(csv_files[csv_name])
        diff = total - wago_count
        pct = abs(diff) / wago_count * 100 if wago_count > 0 else (0 if total == 0 else 100)

        if pct <= 0.5:
            status = "PASS"
        elif pct <= 5.0:
            status = "WARN"
        else:
            status = "FAIL"

        is_critical = name in CRITICAL_TABLES
        results.append((name, total, wago_count, diff, pct, status, is_critical))

    # Check for CSVs with no matching DB2
    db2_stems = {p.stem for p in db2_files}
    for csv_name in sorted(csv_files.keys()):
        if csv_name not in db2_stems:
            missing_db2.append(csv_name)

    # --- Output ---
    pass_count = sum(1 for r in results if r[5] == "PASS")
    warn_count = sum(1 for r in results if r[5] == "WARN")
    fail_count = sum(1 for r in results if r[5] == "FAIL")

    print("=" * 100)
    print(f"SUMMARY: {pass_count} PASS / {warn_count} WARN / {fail_count} FAIL / {len(errors)} ERROR / {len(missing_csv)} no-CSV / {len(missing_db2)} no-DB2")
    print("=" * 100)

    # Critical tables first
    print("\n--- CRITICAL TABLES ---")
    print(f"{'Table':<35} {'DB2':>10} {'Wago':>10} {'Diff':>8} {'%':>7} {'Status'}")
    print("-" * 85)
    for name, db2, wago, diff, pct, status, is_crit in sorted(results, key=lambda r: r[0]):
        if is_crit:
            marker = "***" if status != "PASS" else "   "
            print(f"{marker}{name:<32} {db2:>10,} {wago:>10,} {diff:>+8,} {pct:>6.2f}% {status}")

    # FAILs
    fails = [(n, d, w, df, p, s, c) for n, d, w, df, p, s, c in results if s == "FAIL"]
    if fails:
        print(f"\n--- FAILURES ({len(fails)}) ---")
        print(f"{'Table':<35} {'DB2':>10} {'Wago':>10} {'Diff':>8} {'%':>7}")
        print("-" * 75)
        for name, db2, wago, diff, pct, status, _ in sorted(fails, key=lambda r: -r[4]):
            print(f"{name:<35} {db2:>10,} {wago:>10,} {diff:>+8,} {pct:>6.2f}%")

    # WARNs
    warns = [(n, d, w, df, p, s, c) for n, d, w, df, p, s, c in results if s == "WARN"]
    if warns:
        print(f"\n--- WARNINGS ({len(warns)}) ---")
        print(f"{'Table':<35} {'DB2':>10} {'Wago':>10} {'Diff':>8} {'%':>7}")
        print("-" * 75)
        for name, db2, wago, diff, pct, status, _ in sorted(warns, key=lambda r: -r[4]):
            print(f"{name:<35} {db2:>10,} {wago:>10,} {diff:>+8,} {pct:>6.2f}%")

    # Errors
    if errors:
        print(f"\n--- ERRORS ({len(errors)}) ---")
        for name, err in sorted(errors):
            print(f"  {name}: {err}")

    # Missing CSVs (DB2 exists but no Wago CSV — expected for some TC-only tables)
    if missing_csv:
        print(f"\n--- DB2 with no Wago CSV ({len(missing_csv)}) ---")
        for name, count in sorted(missing_csv):
            print(f"  {name} ({count:,} records)")

    # Missing DB2s (Wago CSV exists but no runtime DB2)
    if missing_db2:
        print(f"\n--- Wago CSV with no runtime DB2 ({len(missing_db2)}) ---")
        for name in sorted(missing_db2):
            print(f"  {name}")

    # Full PASS table
    passes = [(n, d, w, df, p, s, c) for n, d, w, df, p, s, c in results if s == "PASS"]
    print(f"\n--- ALL PASS ({len(passes)}) ---")
    print(f"{'Table':<35} {'DB2':>10} {'Wago':>10} {'Diff':>8} {'%':>7}")
    print("-" * 75)
    for name, db2, wago, diff, pct, status, _ in sorted(passes, key=lambda r: r[0]):
        print(f"{name:<35} {db2:>10,} {wago:>10,} {diff:>+8,} {pct:>6.2f}%")

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
