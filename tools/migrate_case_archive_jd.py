#!/usr/bin/env python3
"""
tools/migrate_case_archive_jd.py

Case_Reference Johnny Decimal XXX_YYY restructure migration.

Executes the plan defined in:
    AI_Studio/Reports/case_archive_jd_proposal.md

STAGING + SWAP approach (following tools/arcanum_johnny_decimal.py):
    1. Build index of current state with sha256 hashes
    2. Copy Case_Reference/ into Case_Reference_staging/ with new folder names
    3. Verify every file's hash matches its source
    4. Atomic swap: rename old -> backup, staging -> primary
    5. Regenerate index with new paths
    6. Generate _MIGRATION_LOG.md audit trail

Core guarantees:
    - NEVER renames individual files (chain-of-custody preservation)
    - Folder renames only
    - Stdlib only (no dependencies)
    - Dry-run by default; --commit required for destructive ops
    - Pre/post hash verification aborts on any mismatch
    - Backup preserved until explicitly deleted
    - Respects FRAGILE_AREAS (do-not-touch list from the SME tab handoff)

Subcommands:
    index       Build _INDEX.json from current state (read-only)
    plan        Print the rename plan in human-readable form (no file ops)
    migrate     Full migration: stage -> verify -> (swap if --commit)
                  --dry-run (default): stages + verifies, stops before swap
                  --commit: proceeds through atomic swap
    verify      Hash-check all files against _INDEX.json
    delete      Execute delete candidates (requires --confirm)

Author: JD-Planner tab (Claude Code)
Date: 2026-04-09
Python: 3.11+
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import os
import shutil
import sys
import tarfile
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Constants: paths
# ---------------------------------------------------------------------------
BASE_PATH = Path(r"C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference")
STAGING_PATH = BASE_PATH.parent / "Case_Reference_staging"
BACKUP_DIR = Path(r"C:/Users/atayl/Desktop/_backups")
INDEX_FILE = BASE_PATH / "_INDEX.json"
INDEX_MD = BASE_PATH / "_INDEX.md"
MIGRATION_LOG = BASE_PATH / "_MIGRATION_LOG.md"
SCHEMA_VERSION = "1.0"
PROPOSAL_REF = "AI_Studio/Reports/case_archive_jd_proposal.md"

# ---------------------------------------------------------------------------
# Constants: the migration plan
# ---------------------------------------------------------------------------
# Direct folder renames. Source folder name -> target folder name.
# Source filenames INSIDE these folders are NOT touched.
RENAME_PLAN: dict[str, str] = {
    "10_TIMELINE_AND_NARRATIVES":   "110_timeline_and_narratives",
    "13_ANALYSIS_AND_BRIEFS":       "120_analysis_and_briefs",
    "__MASTER DOCUMENTS":           "130_master_documents",
    "ChatGPT Analysis":             "140_chatgpt_analysis",
    "06_CLINICAL_RECORDS":          "210_clinical_records",
    "03_MEB_IDES":                  "220_meb_ides",
    "01_APPEALS_AND_QAI":           "230_appeals_and_qai",
    "15_NJP_AND_DISCIPLINE":        "310_njp_art15",
    "09_SECURITY_CLEARANCE":        "340_security_clearance",
    "02_IG_WHISTLEBLOWER":          "410_ig_whistleblower",
    "08_CONGRESSIONAL":             "430_congressional",
    "files 23":                     "500_filing_drafts",
    "11_EMAILS":                    "610_emails",
    "Today_Emails":                 "620_today_emails",
    "05_EVIDENCE_SCREENSHOTS":      "630_evidence_screenshots",
    "07_MILITARY_RECORDS":          "710_military_records",
    "04_LEGAL_CORRESPONDENCE":      "720_legal_correspondence",
    "14_AFW2_OWF_TRANSITION":       "810_afw2_owf_transition",
    "12_GOFUNDME_AND_ADVOCACY":     "820_gofundme_advocacy",
    "__Archive":                    "900_archive",
    "99_MASTER_SYNTHESIS_OUTPUT":   "920_master_synthesis_output",
}

# New empty folders to create in staging.
NEW_FOLDERS: list[str] = [
    "100_case_foundation",
    "150_session_outputs",
    "910_ocr_outputs",
    "999_tools",
]

# Folders whose contents should be MERGED INTO an already-renamed target.
# Source folder name -> target folder (post-rename name).
# These are folders we're eliminating, whose files need to land somewhere.
CONSOLIDATIONS: dict[str, str] = {
    # Pure-subset duplicate (2 files, both in primary) - merge into primary
    "02_IG_AND_WHISTLEBLOWER":   "410_ig_whistleblower",
    # OCR-output stub folder - its derivative files go to the outputs folder
    "02_NJP":                    "910_ocr_outputs",
    # Same story
    "07_CONGRESSIONAL":          "910_ocr_outputs",
    # Audit outputs roll up into the ocr_outputs area under a subfolder
    "99_EMAIL_AUDIT_OUTPUT":     "910_ocr_outputs/email_audit",
    "99_SOURCE_AUDIT_OUTPUT":    "910_ocr_outputs/source_audit",
}

# Root-level loose files to move into a folder.
# Filename at root -> target folder (post-rename name).
ROOT_FILE_MOVES: dict[str, str] = {
    "SESSION_FINDINGS_2026-03-14.md":        "150_session_outputs",
    "SESSION_FINDINGS_2026-03-15.md":        "150_session_outputs",
    "SESSION_FINDINGS_2026-04-08.md":        "150_session_outputs",
    "SESSION_HANDOFF_20260316.md":           "150_session_outputs",
    "REAL_ACTIONS_THAT_CHANGE_YOUR_LIFE.md": "130_master_documents",
    "transcribe_audio.py":                   "999_tools",
    "transcribe_all_cpu.py":                 "999_tools",
}

# Folders flagged for deletion (require --confirm on `delete` subcommand).
# Before deleting, we hash-verify every file in here exists identically elsewhere.
DELETE_CANDIDATES: dict[str, str] = {
    # 8 IMG_*.jpeg duplicates - the exact same files live in Today_Emails/05_Tolin_Update/
    "securityclearancestuff_": "620_today_emails/05_Tolin_Update",
}

# "Fragile areas" = subtrees we should NOT descend into or re-process.
# These are preserved AS-IS during the staging copy (shutil.copytree picks them up
# as part of their parent folder's subtree, but we don't second-guess their contents).
# Stored as paths relative to BASE_PATH.
FRAGILE_AREAS: list[str] = [
    "11_EMAILS/cusersatayldesktopcase_reference11_emails_1",
    "11_EMAILS/takeout-20260315T192351Z-3-001",
    "99_MASTER_SYNTHESIS_OUTPUT/New folder",
]

# Top-level files/folders we ignore entirely during indexing/staging (junk).
SKIP_NAMES: set[str] = {
    ".DS_Store",
    "Thumbs.db",
    "desktop.ini",
    "_INDEX.json",
    "_INDEX.md",
    "_MIGRATION_LOG.md",
}

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
def setup_logging(verbose: bool = False) -> logging.Logger:
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    return logging.getLogger("jd_migrate")


log = logging.getLogger("jd_migrate")


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------
@dataclass
class IndexEntry:
    """One file in the archive, plus metadata for integrity checking."""
    path: str                # relative to base, forward slashes
    filename_original: str   # unchanged - chain-of-custody
    size_bytes: int
    sha256: str
    mtime_iso: str
    category: str            # JD category number as string, e.g. "230"
    is_derivative: bool = False  # OCR output, meta.json sidecar, etc.
    notes: str = ""          # free-form annotation (e.g. "fragile-area")


@dataclass
class MigrationStats:
    """Summary counters updated throughout migration."""
    files_walked: int = 0
    files_hashed: int = 0
    files_staged: int = 0
    folders_renamed: int = 0
    folders_consolidated: int = 0
    root_files_moved: int = 0
    fragile_areas_preserved: int = 0
    hash_mismatches: list[str] = field(default_factory=list)
    skipped_unknown_top_entries: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Hashing
# ---------------------------------------------------------------------------
def sha256_of(path: Path, chunk_size: int = 1 << 16) -> str:
    """SHA-256 of a file, streaming read."""
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(chunk_size):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------
def rel_to_base(path: Path, base: Path) -> str:
    """Return a POSIX-style path relative to base."""
    return str(path.relative_to(base)).replace("\\", "/")


def is_fragile(rel_path: str) -> bool:
    """True if rel_path is inside any FRAGILE_AREA."""
    for fragile in FRAGILE_AREAS:
        if rel_path == fragile or rel_path.startswith(fragile + "/"):
            return True
    return False


def get_top_folder(rel_path: str) -> str:
    """First segment of a relative path ('foo/bar/baz.txt' -> 'foo')."""
    return rel_path.split("/", 1)[0] if "/" in rel_path else rel_path


def resolve_category_from_top(top: str) -> str:
    """Look up the JD category for a top-level folder name.

    Handles both pre-rename (e.g. '01_APPEALS_AND_QAI') and post-rename
    (e.g. '230_appeals_and_qai') inputs.
    """
    # Already post-rename?
    if len(top) >= 3 and top[:3].isdigit():
        return top[:3]
    # Pre-rename, in RENAME_PLAN?
    if top in RENAME_PLAN:
        return RENAME_PLAN[top].split("_", 1)[0]
    # In a CONSOLIDATION?
    if top in CONSOLIDATIONS:
        tgt = CONSOLIDATIONS[top]
        return tgt.split("_", 1)[0].split("/", 1)[0]
    # Root file being moved?
    if top in ROOT_FILE_MOVES:
        return ROOT_FILE_MOVES[top].split("_", 1)[0]
    # Delete candidate?
    if top in DELETE_CANDIDATES:
        return "000"  # flagged for deletion
    # Unknown
    return "???"


def detect_derivative(path: Path) -> bool:
    """Heuristic: is this file an OCR/extraction pipeline derivative?

    Rules:
    - '.meta.json' sidecar files are always derivatives
    - '.txt' files that have a sibling '.pdf' or '.docx' are OCR output
    """
    name = path.name
    if name.endswith(".meta.json"):
        return True
    if name.endswith(".txt"):
        for ext in (".pdf", ".docx", ".eml", ".msg"):
            if path.with_suffix(ext).exists():
                return True
    return False


# ---------------------------------------------------------------------------
# Walking
# ---------------------------------------------------------------------------
def walk_files(base: Path) -> list[Path]:
    """All real files under base, excluding SKIP_NAMES."""
    out: list[Path] = []
    for p in base.rglob("*"):
        if not p.is_file():
            continue
        if p.name in SKIP_NAMES:
            continue
        out.append(p)
    return out


# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------
def preflight(base: Path, staging: Path, stats: MigrationStats) -> bool:
    """Verify the migration can proceed. Returns True if OK, False if abort."""
    ok = True

    # Base must exist
    if not base.exists() or not base.is_dir():
        log.error(f"Base path does not exist: {base}")
        return False

    # Staging must NOT exist (would collide)
    if staging.exists():
        log.error(
            f"Staging path already exists: {staging}\n"
            f"Delete it manually if a prior run left it behind, then re-run."
        )
        return False

    # Every source in RENAME_PLAN must exist
    missing_sources: list[str] = []
    for src in RENAME_PLAN:
        if not (base / src).exists():
            missing_sources.append(src)
    if missing_sources:
        log.warning(f"Sources in RENAME_PLAN not found (will be skipped):")
        for m in missing_sources:
            log.warning(f"  - {m}")

    # Every consolidation target must exist AFTER rename
    # (i.e. the source of the target's rename must exist now)
    for src, tgt_folder in CONSOLIDATIONS.items():
        tgt_top = tgt_folder.split("/", 1)[0]
        reverse_map = {v: k for k, v in RENAME_PLAN.items()}
        if tgt_top in NEW_FOLDERS:
            continue  # new folder we create
        if tgt_top in reverse_map:
            if not (base / reverse_map[tgt_top]).exists():
                log.error(
                    f"Consolidation source '{src}' targets '{tgt_folder}' "
                    f"which requires '{reverse_map[tgt_top]}' to exist"
                )
                ok = False

    # No unexpected top-level entries we haven't accounted for
    accounted: set[str] = (
        set(RENAME_PLAN.keys())
        | set(CONSOLIDATIONS.keys())
        | set(ROOT_FILE_MOVES.keys())
        | set(DELETE_CANDIDATES.keys())
        | SKIP_NAMES
    )
    for entry in base.iterdir():
        name = entry.name
        if name in accounted:
            continue
        if name in SKIP_NAMES:
            continue
        # Flag unknown top-level entries for manual review
        stats.skipped_unknown_top_entries.append(name)
        log.warning(f"UNKNOWN top-level entry (will be left in place): {name}")

    return ok


# ---------------------------------------------------------------------------
# Index building
# ---------------------------------------------------------------------------
def build_index(base: Path, stats: MigrationStats) -> dict[str, Any]:
    """Walk the archive, hash every file, produce _INDEX.json content.

    Assigns JD codes XXX_YYY where XXX is the category (from RENAME_PLAN
    lookup on the top folder) and YYY is an auto-incrementing counter within
    that category. Files in FRAGILE_AREAS get flagged but still indexed.
    """
    files = walk_files(base)
    log.info(f"Found {len(files)} files under {base}")
    stats.files_walked = len(files)

    category_counters: dict[str, int] = {}
    entries: dict[str, IndexEntry] = {}

    for i, f in enumerate(sorted(files)):
        rel = rel_to_base(f, base)
        top = get_top_folder(rel)
        category = resolve_category_from_top(top)

        if category not in category_counters:
            category_counters[category] = 0
        category_counters[category] += 1
        jd_id = f"{category}_{category_counters[category]:03d}"

        if i % 100 == 0 and i > 0:
            log.info(f"  hashed {i}/{len(files)}...")

        try:
            stat = f.stat()
            entry = IndexEntry(
                path=rel,
                filename_original=f.name,
                size_bytes=stat.st_size,
                sha256=sha256_of(f),
                mtime_iso=datetime.fromtimestamp(
                    stat.st_mtime, tz=timezone.utc
                ).isoformat(),
                category=category,
                is_derivative=detect_derivative(f),
                notes="fragile-area" if is_fragile(rel) else "",
            )
            entries[jd_id] = entry
            stats.files_hashed += 1
        except (OSError, PermissionError) as e:
            msg = f"Cannot hash {rel}: {e}"
            log.warning(msg)
            stats.errors.append(msg)

    # Build category manifest
    categories: dict[str, dict[str, str]] = {}
    for src, tgt in RENAME_PLAN.items():
        cat = tgt.split("_", 1)[0]
        name = tgt.split("_", 1)[1] if "_" in tgt else tgt
        categories[cat] = {"name": name, "path": tgt + "/", "source": src}
    for new in NEW_FOLDERS:
        cat = new.split("_", 1)[0]
        name = new.split("_", 1)[1] if "_" in new else new
        categories[cat] = {"name": name, "path": new + "/", "source": "(new)"}

    return {
        "schema_version": SCHEMA_VERSION,
        "generated": datetime.now(timezone.utc).isoformat(),
        "generator": "tools/migrate_case_archive_jd.py",
        "proposal_ref": PROPOSAL_REF,
        "base_path": str(base),
        "total_files": len(entries),
        "rename_plan_applied": _detect_applied(base),
        "categories": dict(sorted(categories.items())),
        "entries": {k: asdict(v) for k, v in sorted(entries.items())},
    }


def _detect_applied(base: Path) -> bool:
    """True if the JD rename plan appears to already be applied to `base`.

    Uses the heuristic: if any of the RENAME_PLAN TARGETS exist AND none of
    the SOURCES exist, we're post-migration.
    """
    targets_present = sum(1 for v in RENAME_PLAN.values() if (base / v).exists())
    sources_present = sum(1 for k in RENAME_PLAN.keys() if (base / k).exists())
    return targets_present > 0 and sources_present == 0


# ---------------------------------------------------------------------------
# Index persistence
# ---------------------------------------------------------------------------
def write_index(index_data: dict[str, Any], out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(index_data, indent=2, ensure_ascii=False)
    out_path.write_text(text, encoding="utf-8", newline="\n")
    log.info(f"Wrote index: {out_path} ({len(text):,} bytes, {index_data['total_files']} entries)")


def write_index_md(index_data: dict[str, Any], out_path: Path) -> None:
    """Human-readable table of contents generated from the JSON index."""
    lines: list[str] = []
    lines.append(f"# Case_Reference — Index (auto-generated)")
    lines.append(f"")
    lines.append(f"> Generated: {index_data['generated']}  ")
    lines.append(f"> Total files: {index_data['total_files']}  ")
    lines.append(f"> Source: `{index_data['generator']}`  ")
    lines.append(f"> Proposal: `{index_data['proposal_ref']}`")
    lines.append(f"")
    lines.append(f"**Do not edit by hand.** Regenerate with:  ")
    lines.append(f"`python tools/migrate_case_archive_jd.py index`")
    lines.append("")

    # Group entries by category
    by_cat: dict[str, list[tuple[str, dict[str, Any]]]] = {}
    for jd_id, entry in index_data["entries"].items():
        cat = entry["category"]
        by_cat.setdefault(cat, []).append((jd_id, entry))

    for cat in sorted(by_cat.keys()):
        cat_info = index_data["categories"].get(cat, {})
        cat_name = cat_info.get("name", "(unknown)")
        entries = by_cat[cat]
        lines.append(f"## {cat} — {cat_name} ({len(entries)} files)")
        lines.append("")
        lines.append("| JD | Path | Size | Derivative? |")
        lines.append("|---|---|---|---|")
        for jd_id, entry in entries[:200]:  # cap at 200 per category for readability
            deriv = "Y" if entry.get("is_derivative") else ""
            size_kb = entry["size_bytes"] // 1024
            lines.append(f"| {jd_id} | `{entry['path']}` | {size_kb}KB | {deriv} |")
        if len(entries) > 200:
            lines.append(f"| ... | *({len(entries) - 200} more, see `_INDEX.json`)* | | |")
        lines.append("")

    text = "\n".join(lines)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(text, encoding="utf-8", newline="\n")
    log.info(f"Wrote index markdown: {out_path} ({len(text):,} bytes)")


# ---------------------------------------------------------------------------
# Plan printer
# ---------------------------------------------------------------------------
def print_plan() -> None:
    print("=" * 76)
    print("CASE ARCHIVE JOHNNY DECIMAL MIGRATION -- PLAN")
    print("=" * 76)
    print(f"Base: {BASE_PATH}")
    print(f"Staging: {STAGING_PATH}")
    print(f"Proposal: {PROPOSAL_REF}")
    print()

    print("-- DIRECT FOLDER RENAMES --")
    print(f"{'OLD':<40} -> NEW")
    print("-" * 76)
    for src, tgt in RENAME_PLAN.items():
        exists = "" if (BASE_PATH / src).exists() else "  [MISSING]"
        print(f"{src:<40} -> {tgt}{exists}")
    print()

    print("-- NEW FOLDERS (created empty, populated by moves below) --")
    for new in NEW_FOLDERS:
        print(f"  {new}")
    print()

    print("-- CONSOLIDATIONS (source contents merged into target) --")
    print(f"{'SOURCE':<40} -> TARGET")
    print("-" * 76)
    for src, tgt in CONSOLIDATIONS.items():
        exists = "" if (BASE_PATH / src).exists() else "  [MISSING]"
        print(f"{src:<40} -> {tgt}{exists}")
    print()

    print("-- ROOT FILE MOVES --")
    print(f"{'FILE':<50} -> FOLDER")
    print("-" * 76)
    for f, tgt in ROOT_FILE_MOVES.items():
        exists = "" if (BASE_PATH / f).exists() else "  [MISSING]"
        print(f"{f:<50} -> {tgt}{exists}")
    print()

    print("-- FRAGILE AREAS (preserved as-is, not re-processed) --")
    for fragile in FRAGILE_AREAS:
        exists = "" if (BASE_PATH / fragile).exists() else "  [MISSING]"
        print(f"  {fragile}{exists}")
    print()

    print("-- DELETE CANDIDATES (require separate `delete --confirm` run) --")
    for src, verify_against in DELETE_CANDIDATES.items():
        exists = "" if (BASE_PATH / src).exists() else "  [MISSING]"
        print(f"  {src}{exists}")
        print(f"    verified against: {verify_against}")
    print()

    print("=" * 76)
    print("Run `migrate --dry-run` to execute the staging build and hash-verify.")
    print("Run `migrate --commit` ONLY after --dry-run passes clean.")
    print("=" * 76)


# ---------------------------------------------------------------------------
# Staging build
# ---------------------------------------------------------------------------
def build_staging(base: Path, staging: Path, stats: MigrationStats) -> bool:
    """Copy the archive into a new folder structure.

    Uses shutil.copytree for whole-subtree copies (preserves mtimes).
    FRAGILE areas are copied as-is via copytree.
    Returns True on success.
    """
    log.info(f"Creating staging at: {staging}")
    staging.mkdir(parents=True, exist_ok=False)

    # Phase A: direct renames
    log.info("Phase A: direct folder renames...")
    for src, tgt in RENAME_PLAN.items():
        src_path = base / src
        tgt_path = staging / tgt
        if not src_path.exists():
            log.warning(f"  [SKIP] {src} does not exist")
            continue
        log.info(f"  {src} -> {tgt}")
        shutil.copytree(src_path, tgt_path, dirs_exist_ok=False)
        stats.folders_renamed += 1

    # Phase B: new empty folders
    log.info("Phase B: new folders...")
    for new in NEW_FOLDERS:
        (staging / new).mkdir(parents=True, exist_ok=True)
        log.info(f"  created {new}")

    # Phase C: consolidations (merge source files into target)
    log.info("Phase C: consolidations...")
    for src, tgt_folder in CONSOLIDATIONS.items():
        src_path = base / src
        tgt_path = staging / tgt_folder
        if not src_path.exists():
            log.warning(f"  [SKIP] {src} does not exist")
            continue
        tgt_path.mkdir(parents=True, exist_ok=True)
        # Copy each file from src into tgt, preserving any subfolder structure
        for item in src_path.rglob("*"):
            if not item.is_file():
                continue
            rel = item.relative_to(src_path)
            dst = tgt_path / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            if dst.exists():
                # Conflict: target already has a file with this path.
                # For pure-subset duplicates (02_IG_AND_WHISTLEBLOWER), this is
                # EXPECTED — the target already has the file from the primary.
                # Verify hashes match before skipping.
                if sha256_of(item) == sha256_of(dst):
                    log.debug(f"  [dup-ok] {rel} already in {tgt_folder}")
                    continue
                else:
                    msg = (
                        f"HASH MISMATCH on consolidation: {src}/{rel} already "
                        f"exists in {tgt_folder} with different content"
                    )
                    log.error(msg)
                    stats.errors.append(msg)
                    return False
            shutil.copy2(item, dst)
            stats.files_staged += 1
        log.info(f"  {src} -> {tgt_folder}")
        stats.folders_consolidated += 1

    # Phase D: root file moves
    log.info("Phase D: root file moves...")
    for filename, tgt_folder in ROOT_FILE_MOVES.items():
        src_file = base / filename
        if not src_file.exists():
            log.warning(f"  [SKIP] {filename} does not exist at root")
            continue
        tgt_path = staging / tgt_folder
        tgt_path.mkdir(parents=True, exist_ok=True)
        dst = tgt_path / filename
        shutil.copy2(src_file, dst)
        log.info(f"  {filename} -> {tgt_folder}/{filename}")
        stats.root_files_moved += 1

    # Phase E: count fragile areas (for reporting - they copied as part of Phase A)
    log.info("Phase E: fragile areas accounted for...")
    for fragile in FRAGILE_AREAS:
        top = get_top_folder(fragile)
        if top in RENAME_PLAN:
            new_top = RENAME_PLAN[top]
            rest = fragile.split("/", 1)[1] if "/" in fragile else ""
            staged_fragile = staging / new_top / rest if rest else staging / new_top
            if staged_fragile.exists():
                log.info(f"  preserved: {fragile} -> {new_top}/{rest}")
                stats.fragile_areas_preserved += 1
            else:
                log.warning(f"  fragile area NOT found after staging: {fragile}")
        else:
            log.warning(f"  fragile area has no parent in RENAME_PLAN: {fragile}")

    log.info(
        f"Staging built: {stats.folders_renamed} renamed, "
        f"{stats.folders_consolidated} consolidated, "
        f"{stats.root_files_moved} root files moved, "
        f"{stats.fragile_areas_preserved} fragile areas preserved"
    )
    return True


# ---------------------------------------------------------------------------
# Hash verification
# ---------------------------------------------------------------------------
def verify_staging(
    base: Path,
    staging: Path,
    pre_index: dict[str, Any],
    stats: MigrationStats,
) -> bool:
    """For every file in pre_index, find it in staging and verify its hash.

    Returns True if every file is accounted for with a matching hash.
    """
    log.info("Verifying staging against source hashes...")

    # Walk staging, hash every file, build lookup by sha256
    staging_files = walk_files(staging)
    log.info(f"  staging has {len(staging_files)} files")
    staging_by_hash: dict[str, list[str]] = {}
    for f in staging_files:
        try:
            h = sha256_of(f)
            rel = rel_to_base(f, staging)
            staging_by_hash.setdefault(h, []).append(rel)
        except (OSError, PermissionError) as e:
            msg = f"Cannot hash staged file {f}: {e}"
            log.error(msg)
            stats.errors.append(msg)
            return False

    # Every pre_index entry must have a matching hash in staging
    missing: list[str] = []
    for jd_id, entry in pre_index["entries"].items():
        src_rel = entry["path"]
        src_hash = entry["sha256"]
        if src_hash not in staging_by_hash:
            missing.append(f"{jd_id} ({src_rel}, hash {src_hash[:12]})")
            stats.hash_mismatches.append(src_rel)

    if missing:
        log.error(f"HASH VERIFICATION FAILED: {len(missing)} files missing from staging")
        for m in missing[:10]:
            log.error(f"  - {m}")
        if len(missing) > 10:
            log.error(f"  ... and {len(missing) - 10} more")
        return False

    log.info(f"Hash verification PASSED: all {len(pre_index['entries'])} files matched")
    return True


# ---------------------------------------------------------------------------
# Atomic swap
# ---------------------------------------------------------------------------
def swap(base: Path, staging: Path) -> Path:
    """Rename base -> backup, staging -> base. Returns backup path."""
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup = base.parent / f"Case_Reference_backup_{ts}"

    log.info(f"Swap: {base} -> {backup}")
    base.rename(backup)

    log.info(f"Swap: {staging} -> {base}")
    staging.rename(base)

    return backup


# ---------------------------------------------------------------------------
# Migration log
# ---------------------------------------------------------------------------
def write_migration_log(
    log_path: Path,
    pre_index: dict[str, Any],
    post_index: dict[str, Any],
    stats: MigrationStats,
    backup_path: Path,
) -> None:
    """Write _MIGRATION_LOG.md with full before/after and the rename mapping."""
    lines: list[str] = []
    lines.append(f"# Case_Reference Migration Log")
    lines.append(f"")
    lines.append(f"**Date**: {datetime.now(timezone.utc).isoformat()}  ")
    lines.append(f"**Script**: `tools/migrate_case_archive_jd.py`  ")
    lines.append(f"**Proposal**: `{PROPOSAL_REF}`  ")
    lines.append(f"**Backup**: `{backup_path}`  ")
    lines.append(f"")
    lines.append(f"## Summary")
    lines.append(f"")
    lines.append(f"- Files walked: **{stats.files_walked}**")
    lines.append(f"- Files hashed: **{stats.files_hashed}**")
    lines.append(f"- Folders renamed: **{stats.folders_renamed}**")
    lines.append(f"- Folders consolidated: **{stats.folders_consolidated}**")
    lines.append(f"- Root files moved: **{stats.root_files_moved}**")
    lines.append(f"- Fragile areas preserved: **{stats.fragile_areas_preserved}**")
    lines.append(f"- Hash mismatches: **{len(stats.hash_mismatches)}**")
    lines.append(f"- Errors: **{len(stats.errors)}**")
    lines.append(f"")

    lines.append(f"## Folder Rename Table")
    lines.append(f"")
    lines.append(f"| Old | New |")
    lines.append(f"|---|---|")
    for src, tgt in RENAME_PLAN.items():
        lines.append(f"| `{src}/` | `{tgt}/` |")
    lines.append(f"")

    lines.append(f"## Consolidations")
    lines.append(f"")
    lines.append(f"| Source | Target |")
    lines.append(f"|---|---|")
    for src, tgt in CONSOLIDATIONS.items():
        lines.append(f"| `{src}/` | `{tgt}/` |")
    lines.append(f"")

    lines.append(f"## Root File Moves")
    lines.append(f"")
    lines.append(f"| File | New Location |")
    lines.append(f"|---|---|")
    for f, tgt in ROOT_FILE_MOVES.items():
        lines.append(f"| `{f}` | `{tgt}/{f}` |")
    lines.append(f"")

    if stats.skipped_unknown_top_entries:
        lines.append(f"## Unknown Top-Level Entries (left in place)")
        lines.append(f"")
        for entry in stats.skipped_unknown_top_entries:
            lines.append(f"- `{entry}`")
        lines.append(f"")

    if stats.errors:
        lines.append(f"## Errors")
        lines.append(f"")
        for err in stats.errors:
            lines.append(f"- {err}")
        lines.append(f"")

    lines.append(f"## Integrity")
    lines.append(f"")
    lines.append(
        f"All {stats.files_hashed} files were sha256-hashed before the swap "
        f"and verified to exist with matching hashes in the staging directory. "
        f"No file content was modified. No file was renamed (filenames preserved "
        f"at their original values for chain-of-custody)."
    )
    lines.append(f"")

    text = "\n".join(lines)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(text, encoding="utf-8", newline="\n")
    log.info(f"Wrote migration log: {log_path}")


# ---------------------------------------------------------------------------
# Delete candidates
# ---------------------------------------------------------------------------
def execute_deletes(base: Path, confirm: bool) -> bool:
    """Delete folders in DELETE_CANDIDATES after hash-verifying their contents
    exist identically in the designated verification target.
    """
    if not confirm:
        log.error("Refusing to delete without --confirm flag")
        return False

    any_failure = False
    for src_name, verify_against in DELETE_CANDIDATES.items():
        src = base / src_name
        verify_target = base / verify_against
        if not src.exists():
            log.warning(f"[SKIP] {src_name} does not exist")
            continue
        if not verify_target.exists():
            log.error(f"Cannot verify {src_name}: target {verify_against} does not exist")
            any_failure = True
            continue

        # Every file in src must exist in verify_target with matching hash
        log.info(f"Verifying {src_name} against {verify_against}...")
        all_ok = True
        for f in src.rglob("*"):
            if not f.is_file():
                continue
            rel = f.relative_to(src)
            candidate = verify_target / rel.name  # flat name match
            if not candidate.exists():
                log.error(f"  MISSING in verify target: {rel.name}")
                all_ok = False
                continue
            if sha256_of(f) != sha256_of(candidate):
                log.error(f"  HASH MISMATCH: {rel.name}")
                all_ok = False

        if not all_ok:
            log.error(f"Refusing to delete {src_name}: verification failed")
            any_failure = True
            continue

        log.info(f"Verification passed. Deleting {src_name}...")
        shutil.rmtree(src)
        log.info(f"  deleted: {src}")

    return not any_failure


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def cmd_index(args: argparse.Namespace) -> int:
    """Build _INDEX.json and _INDEX.md from the current state."""
    setup_logging(args.verbose)
    stats = MigrationStats()
    if not BASE_PATH.exists():
        log.error(f"Base path does not exist: {BASE_PATH}")
        return 2
    index_data = build_index(BASE_PATH, stats)

    out_json = Path(args.output) if args.output else INDEX_FILE
    out_md = out_json.with_suffix(".md") if args.output else INDEX_MD

    write_index(index_data, out_json)
    write_index_md(index_data, out_md)

    log.info(f"DONE. {stats.files_hashed} files indexed.")
    if stats.errors:
        log.warning(f"{len(stats.errors)} errors during indexing")
        return 1
    return 0


def cmd_plan(args: argparse.Namespace) -> int:
    """Print the rename plan in human-readable form."""
    print_plan()
    return 0


def cmd_migrate(args: argparse.Namespace) -> int:
    """Full migration: index -> stage -> verify -> (swap if --commit)."""
    setup_logging(args.verbose)
    stats = MigrationStats()

    log.info("=" * 60)
    log.info(f"MIGRATION — mode: {'COMMIT' if args.commit else 'DRY-RUN'}")
    log.info("=" * 60)

    # Step 1: preflight
    if not preflight(BASE_PATH, STAGING_PATH, stats):
        log.error("Preflight checks failed. Aborting.")
        return 2

    # Step 2: build source index (pre-migration)
    log.info("")
    log.info("STEP 1/5: Building source index (pre-migration)...")
    pre_index = build_index(BASE_PATH, stats)
    # Write pre-index to a temp location so we can inspect it if needed
    pre_index_path = BASE_PATH.parent / "_INDEX_pre_migration.json"
    write_index(pre_index, pre_index_path)

    # Step 3: build staging
    log.info("")
    log.info("STEP 2/5: Building staging directory...")
    try:
        if not build_staging(BASE_PATH, STAGING_PATH, stats):
            log.error("Staging build failed. Aborting.")
            _cleanup_staging(STAGING_PATH)
            return 3
    except Exception as e:
        log.error(f"Exception during staging: {e}")
        _cleanup_staging(STAGING_PATH)
        return 3

    # Step 4: verify staging hashes
    log.info("")
    log.info("STEP 3/5: Verifying staging hashes...")
    if not verify_staging(BASE_PATH, STAGING_PATH, pre_index, stats):
        log.error("Hash verification failed. Aborting. Staging left for inspection.")
        return 4

    # Step 5: swap (only if --commit)
    if not args.commit:
        log.info("")
        log.info("DRY-RUN COMPLETE. Staging preserved at:")
        log.info(f"  {STAGING_PATH}")
        log.info("")
        log.info("To proceed with the atomic swap, re-run with --commit")
        log.info("To discard staging, delete it manually")
        return 0

    log.info("")
    log.info("STEP 4/5: Atomic swap (base -> backup, staging -> base)...")
    backup_path = swap(BASE_PATH, STAGING_PATH)

    # Step 6: post-migration index
    log.info("")
    log.info("STEP 5/5: Rebuilding index with new paths...")
    post_index = build_index(BASE_PATH, stats)
    write_index(post_index, INDEX_FILE)
    write_index_md(post_index, INDEX_MD)

    # Migration log
    write_migration_log(MIGRATION_LOG, pre_index, post_index, stats, backup_path)

    # Move the pre-migration index into the new archive for reference
    if pre_index_path.exists():
        shutil.move(str(pre_index_path), str(BASE_PATH / "_INDEX_pre_migration.json"))

    log.info("")
    log.info("=" * 60)
    log.info("MIGRATION COMPLETE")
    log.info("=" * 60)
    log.info(f"New archive: {BASE_PATH}")
    log.info(f"Backup:      {backup_path}")
    log.info(f"Index:       {INDEX_FILE}")
    log.info(f"Log:         {MIGRATION_LOG}")
    log.info("")
    log.info("Next steps:")
    log.info("  1. Review _MIGRATION_LOG.md")
    log.info("  2. Spot-check a few folders in the new archive")
    log.info("  3. Update memory/case-*.md references (separate pass)")
    log.info("  4. Once satisfied, delete the backup to reclaim space")
    log.info("")
    return 0


def _cleanup_staging(staging: Path) -> None:
    """Remove staging directory on failure. Logs but does not raise."""
    if staging.exists():
        try:
            log.info(f"Cleaning up staging: {staging}")
            shutil.rmtree(staging)
        except Exception as e:
            log.error(f"Could not clean up staging: {e}")


def cmd_verify(args: argparse.Namespace) -> int:
    """Hash-check every file in BASE_PATH against _INDEX.json."""
    setup_logging(args.verbose)
    if not INDEX_FILE.exists():
        log.error(f"No index file found at {INDEX_FILE}. Run `index` first.")
        return 2

    index_data = json.loads(INDEX_FILE.read_text(encoding="utf-8"))
    log.info(f"Verifying {index_data['total_files']} files...")

    fail = 0
    for jd_id, entry in index_data["entries"].items():
        path = BASE_PATH / entry["path"]
        if not path.exists():
            log.error(f"MISSING: {entry['path']}")
            fail += 1
            continue
        actual = sha256_of(path)
        if actual != entry["sha256"]:
            log.error(f"HASH MISMATCH: {entry['path']}")
            log.error(f"  expected: {entry['sha256']}")
            log.error(f"  actual:   {actual}")
            fail += 1

    if fail:
        log.error(f"VERIFY FAILED: {fail} files with problems")
        return 1
    log.info(f"VERIFY PASSED: all {index_data['total_files']} files match")
    return 0


def cmd_delete(args: argparse.Namespace) -> int:
    """Execute the delete candidates after hash-verifying their duplicate targets."""
    setup_logging(args.verbose)
    if execute_deletes(BASE_PATH, args.confirm):
        log.info("Delete complete.")
        return 0
    log.error("Delete failed or aborted.")
    return 1


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Case_Reference Johnny Decimal migration tool.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Proposal: {PROPOSAL_REF}",
    )
    sub = p.add_subparsers(dest="command", required=True)

    # index
    p_index = sub.add_parser("index", help="Build _INDEX.json and _INDEX.md (read-only)")
    p_index.add_argument("--output", help="Output path (default: Case_Reference/_INDEX.json)")
    p_index.add_argument("-v", "--verbose", action="store_true")
    p_index.set_defaults(func=cmd_index)

    # plan
    p_plan = sub.add_parser("plan", help="Print the rename plan in human form")
    p_plan.set_defaults(func=cmd_plan)
    p_plan.add_argument("-v", "--verbose", action="store_true")

    # migrate
    p_migrate = sub.add_parser(
        "migrate",
        help="Full migration: index -> stage -> verify -> (swap if --commit)",
    )
    p_migrate.add_argument(
        "--commit",
        action="store_true",
        help="Proceed through the atomic swap. Without this, dry-run only.",
    )
    p_migrate.add_argument("-v", "--verbose", action="store_true")
    p_migrate.set_defaults(func=cmd_migrate)

    # verify
    p_verify = sub.add_parser("verify", help="Hash-check files against _INDEX.json")
    p_verify.add_argument("-v", "--verbose", action="store_true")
    p_verify.set_defaults(func=cmd_verify)

    # delete
    p_delete = sub.add_parser(
        "delete",
        help="Execute DELETE_CANDIDATES (requires --confirm)",
    )
    p_delete.add_argument(
        "--confirm",
        action="store_true",
        help="Required flag for destructive delete operations.",
    )
    p_delete.add_argument("-v", "--verbose", action="store_true")
    p_delete.set_defaults(func=cmd_delete)

    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
