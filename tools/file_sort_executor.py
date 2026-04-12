#!/usr/bin/env python3
"""
File Sort Executor — reads a JSON move plan and executes file moves.

Usage:
    python file_sort_executor.py plan.json                    # dry-run (default)
    python file_sort_executor.py plan.json --execute          # actually move files
    python file_sort_executor.py plan.json --execute --delete # also delete marked files
    python file_sort_executor.py --from-inventory inventory.md --output plan.json  # parse .md inventory

Plan format (JSON):
[
    {"action": "move", "source": "path/to/file", "dest": "path/to/dest/", "reason": "..."},
    {"action": "delete", "source": "path/to/file", "reason": "duplicate of X"},
    {"action": "skip", "source": "path/to/file", "reason": "needs manual review"},
    {"action": "secure_delete", "source": "path/to/file", "reason": "contains credentials"}
]
"""

import argparse
import json
import os
import re
import shutil
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Known destination shorthands — keeps inventory tables compact
# ---------------------------------------------------------------------------
DEST_ALIASES = {
    "Case_Reference/": r"C:\Users\atayl\Desktop\Case_Reference\\",
    "VoxCore/": r"C:\Users\atayl\VoxCore\\",
    "VoxCore ": r"C:\Users\atayl\VoxCore\\",
    "Excluded/": r"C:\Users\atayl\Desktop\Excluded\\",
    "Desktop/": r"C:\Users\atayl\Desktop\\",
}

# Patterns that mean "this row is NOT a real file action"
SKIP_PATTERNS = re.compile(
    r"^(File|---|------|-+|Action|Target|Target Folder|Why|Reason|Size|Item)$",
    re.IGNORECASE,
)

# Patterns that mean "delete"
DELETE_PATTERNS = re.compile(r"\*\*DELETE\*\*|^DELETE$|^DELETE\b", re.IGNORECASE)

# Patterns that mean "review / manual / skip"
REVIEW_PATTERNS = re.compile(
    r"NEEDS MANUAL REVIEW|ARCHIVE|password manager|SECURITY SENSITIVE",
    re.IGNORECASE,
)


def _resolve_dest(raw_dest: str, source_dir: str) -> str:
    """Expand shorthand destination to absolute path."""
    dest = raw_dest.strip().rstrip("/\\")
    if not dest:
        return ""

    # Try each alias prefix
    for prefix, expansion in DEST_ALIASES.items():
        if dest.startswith(prefix):
            remainder = dest[len(prefix):]
            return os.path.join(expansion, remainder)

    # If it looks absolute already, use as-is
    if os.path.isabs(dest):
        return dest

    # Fall back to treating as subdir of source_dir
    return os.path.join(source_dir, dest)


def _classify_action(target: str, reason: str) -> str:
    """Determine action from target/reason text."""
    combined = f"{target} {reason}"

    # Password manager check FIRST — trumps DELETE pattern
    if "password manager" in combined.lower():
        return "secure_delete"
    if DELETE_PATTERNS.search(target):
        return "delete"
    if REVIEW_PATTERNS.search(combined):
        return "skip"
    return "move"


def parse_inventory_md(md_path: str) -> list[dict]:
    """Parse a markdown inventory file into a JSON move plan.

    Handles multiple table formats:
      | File | Size | Target | Reason |     (4-col move tables)
      | File | Action | Reason |            (3-col action tables)
      | File | Reason |                     (2-col delete/dupe tables)

    Also handles composite entries and section headers gracefully.
    """
    plan = []
    seen_sources = set()
    text = Path(md_path).read_text(encoding="utf-8")

    # Find the source directory from the inventory header
    source_dir = ""
    m = re.search(r"\*\*Source:\*\*\s*`([^`]+)`", text)
    if m:
        source_dir = m.group(1).rstrip("/\\")

    # Current section context (helps classify ambiguous rows)
    current_section = ""

    for line in text.splitlines():
        # Track section headers for context
        section_match = re.match(r"^###?\s+(.+)", line)
        if section_match:
            current_section = section_match.group(1).strip().upper()
            continue

        # Must be a table row
        if not line.strip().startswith("|"):
            continue

        # Split cells
        cells = [c.strip() for c in line.split("|")]
        # Remove empty first/last from leading/trailing pipes
        cells = [c for c in cells if c]

        if len(cells) < 2:
            continue

        filename = cells[0].strip("`")

        # Skip header/separator rows
        if SKIP_PATTERNS.match(filename) or filename.startswith("-"):
            continue
        # Skip if filename contains only dashes or stars
        if re.match(r"^[-*]+$", filename):
            continue
        # Skip summary/prose rows (action column contains full sentences)
        if any(
            len(c) > 80 and " " in c
            for c in cells[1:]
        ):
            continue
        # Skip rows from space-recovery or aggregation tables
        if re.match(
            r"^(Delete |Unpack |Move |Total|\*\*)",
            filename,
        ):
            continue
        # Skip table headers that slip through
        if filename.lower() in ("folder", "action"):
            continue

        # Skip composite/aggregate entries that aren't real filenames
        # e.g., "logo/cover jpegs (2 files)", "Extracted zip files (6 zips)"
        if re.search(r"\(\d+ (files?|zips?|items?)\)", filename, re.IGNORECASE):
            continue
        # Skip entries with ellipsis truncation
        if "..." in filename and not os.path.exists(
            os.path.join(source_dir, filename) if source_dir else filename
        ):
            # Try to glob-match truncated names
            if source_dir:
                import glob

                prefix = filename.split("...")[0]
                suffix = filename.split("...")[-1] if "..." in filename else ""
                matches = glob.glob(
                    os.path.join(source_dir, f"{prefix}*{suffix}")
                )
                if len(matches) == 1:
                    filename = os.path.basename(matches[0])
                else:
                    continue  # Can't resolve — skip

        source = os.path.join(source_dir, filename) if source_dir else filename

        # Deduplicate — same source may appear in multiple tables
        if source in seen_sources:
            continue
        seen_sources.add(source)

        # Determine action and destination based on column count
        if len(cells) >= 4:
            # | File | Size | Target | Reason |
            target = cells[2]
            reason = cells[3]
        elif len(cells) == 3:
            # | File | Target/Action | Reason |  OR  | File | Size | Target |
            # Heuristic: if cell[1] looks like a size, shift
            if re.match(r"^\d+\s*(KB|MB|GB|B|bytes?)$", cells[1], re.IGNORECASE):
                target = cells[2]
                reason = ""
            else:
                target = cells[1]
                reason = cells[2]
        else:
            # | File | Reason | — 2-col tables are typically DELETE or REVIEW
            target = ""
            reason = cells[1]

        # 2-column rows with no target should default to skip, not move
        if len(cells) <= 2 and not target:
            if "delete" not in current_section.lower() and "duplicate" not in current_section.lower():
                action = "skip"
                plan.append(
                    {"action": action, "source": source, "dest": "", "reason": reason}
                )
                continue

        # Use section context to help classify
        section_lower = current_section.lower()
        if not target and ("security" in section_lower or "password manager" in section_lower):
            action = "secure_delete"
        elif not target and ("delete" in section_lower or "duplicate" in section_lower):
            action = "delete"
        elif not target and ("review" in section_lower or "manual" in section_lower):
            action = "skip"
        else:
            action = _classify_action(target, reason)

        # Build destination path for moves
        dest = ""
        if action == "move":
            dest = _resolve_dest(target, source_dir)
            # Sanity: if resolved dest looks like a reason (contains common
            # English words but no path separators), this is a parser
            # misclassification — downgrade to skip
            if dest and not os.sep in dest and not "/" in dest:
                action = "skip"
                reason = f"[parser: ambiguous target '{target}'] {reason}"
                dest = ""
            # Another guard: if dest contains words like "duplicate",
            # "identical", "plaintext", it's a reason not a path
            if dest and re.search(
                r"(identical|duplicate|plaintext|re-downloadable|superseded|"
                r"redundant|stale|wrong|already|zero value)",
                dest,
                re.IGNORECASE,
            ):
                action = "skip"
                reason = f"[parser: reason-as-path '{target}'] {reason}"
                dest = ""

        plan.append(
            {
                "action": action,
                "source": source,
                "dest": dest,
                "reason": reason,
            }
        )

    return plan


def _path_size(p: Path) -> int:
    """Return byte size of a file, or recursive total size of a directory.

    Used to detect silent no-clobber / partial-move bugs by comparing source
    size before the move against destination size after. Returns -1 if the
    path can't be stat'd (missing, permission denied, etc.).
    """
    try:
        if p.is_file():
            return p.stat().st_size
        if p.is_dir():
            total = 0
            for child in p.rglob("*"):
                if child.is_file():
                    try:
                        total += child.stat().st_size
                    except OSError:
                        pass  # unreadable file — best-effort
            return total
    except OSError:
        return -1
    return -1


def execute_plan(plan: list[dict], dry_run: bool = True, allow_delete: bool = False):
    """Execute a file sort plan."""
    stats = {"moved": 0, "deleted": 0, "skipped": 0, "errors": 0, "missing": 0}

    for i, item in enumerate(plan, 1):
        action = item.get("action", "skip")
        source = item.get("source", "")
        dest = item.get("dest", "")
        reason = item.get("reason", "")

        if not source:
            continue

        source_path = Path(source)
        exists = source_path.exists()

        prefix = f"[{i:3d}/{len(plan)}]"

        if not exists:
            print(f"{prefix} MISSING: {source}")
            stats["missing"] += 1
            continue

        if action == "skip":
            print(f"{prefix} SKIP: {source} — {reason}")
            stats["skipped"] += 1

        elif action == "move":
            dest_dir = Path(dest)
            dest_file = dest_dir / source_path.name

            if dry_run:
                print(f"{prefix} WOULD MOVE: {source}")
                print(f"         -> {dest_file}")
                stats["moved"] += 1
            else:
                try:
                    dest_dir.mkdir(parents=True, exist_ok=True)
                    if dest_file.exists():
                        print(
                            f"{prefix} CONFLICT: {dest_file} already exists — skipping"
                        )
                        stats["skipped"] += 1
                    else:
                        # Silent-no-clobber safety: capture source size before
                        # the move, then verify dest matches afterwards. Catches
                        # the session-235 bug where a pre-created target folder
                        # silently swallowed a 26 GB move (the mv -n class of
                        # failure where nothing errors but bytes disappear).
                        src_size = _path_size(source_path)

                        shutil.move(str(source_path), str(dest_file))

                        # Post-move verification gate
                        if source_path.exists():
                            print(
                                f"{prefix} ERROR: source still present after move "
                                f"— likely silent merge into existing dest: {source_path}"
                            )
                            stats["errors"] += 1
                        elif not dest_file.exists():
                            print(
                                f"{prefix} ERROR: dest missing after move "
                                f"(move reported success but path absent): {dest_file}"
                            )
                            stats["errors"] += 1
                        else:
                            dst_size = _path_size(dest_file)
                            if src_size >= 0 and dst_size != src_size:
                                delta = dst_size - src_size
                                print(
                                    f"{prefix} WARN: size mismatch post-move "
                                    f"(src {src_size:,}B -> dst {dst_size:,}B, "
                                    f"delta {delta:+,}B): {dest_file}"
                                )
                                stats["errors"] += 1
                            else:
                                size_note = (
                                    f" ({src_size:,}B)" if src_size >= 0 else ""
                                )
                                print(
                                    f"{prefix} MOVED: {source_path.name}"
                                    f"{size_note} -> {dest_dir}"
                                )
                                stats["moved"] += 1
                except Exception as e:
                    print(f"{prefix} ERROR: {source} — {e}")
                    stats["errors"] += 1

        elif action == "delete":
            if dry_run:
                size = source_path.stat().st_size if source_path.is_file() else "dir"
                print(f"{prefix} WOULD DELETE: {source} ({size})")
                stats["deleted"] += 1
            elif allow_delete:
                try:
                    if source_path.is_dir():
                        shutil.rmtree(str(source_path))
                    else:
                        source_path.unlink()
                    print(f"{prefix} DELETED: {source}")
                    stats["deleted"] += 1
                except Exception as e:
                    print(f"{prefix} ERROR deleting: {source} — {e}")
                    stats["errors"] += 1
            else:
                print(f"{prefix} SKIP DELETE (--delete not set): {source}")
                stats["skipped"] += 1

        elif action == "secure_delete":
            print(
                f"{prefix} SECURE: {source} — MOVE TO PASSWORD MANAGER FIRST, then delete"
            )
            stats["skipped"] += 1

    # Summary
    mode = "DRY RUN" if dry_run else "EXECUTED"
    print(f"\n--- {mode} Summary ---")
    print(f"  Moved:   {stats['moved']}")
    print(f"  Deleted: {stats['deleted']}")
    print(f"  Skipped: {stats['skipped']}")
    print(f"  Missing: {stats['missing']}")
    print(f"  Errors:  {stats['errors']}")

    if dry_run:
        print("\nThis was a DRY RUN. Add --execute to actually move files.")
        if stats["deleted"]:
            print("Deletes require both --execute AND --delete flags.")

    return stats


def main():
    parser = argparse.ArgumentParser(description="Execute a file sort plan")
    parser.add_argument("plan", nargs="?", help="Path to JSON plan file")
    parser.add_argument(
        "--execute",
        action="store_true",
        help="Actually move files (default: dry-run)",
    )
    parser.add_argument(
        "--delete",
        action="store_true",
        help="Also execute delete actions (requires --execute)",
    )
    parser.add_argument(
        "--from-inventory",
        type=str,
        help="Parse a .md inventory file into a JSON plan",
    )
    parser.add_argument(
        "--output", "-o", type=str, help="Output path for generated JSON plan"
    )
    args = parser.parse_args()

    if args.from_inventory:
        plan = parse_inventory_md(args.from_inventory)
        if args.output:
            Path(args.output).write_text(
                json.dumps(plan, indent=2, ensure_ascii=False), encoding="utf-8"
            )
            print(f"Plan written to {args.output} ({len(plan)} actions)")
        else:
            print(json.dumps(plan, indent=2, ensure_ascii=False))
        return

    if not args.plan:
        parser.print_help()
        sys.exit(1)

    plan = json.loads(Path(args.plan).read_text(encoding="utf-8"))
    execute_plan(plan, dry_run=not args.execute, allow_delete=args.delete)


if __name__ == "__main__":
    main()
