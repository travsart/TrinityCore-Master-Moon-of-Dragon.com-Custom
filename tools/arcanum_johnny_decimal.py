#!/usr/bin/env python3
"""
Arcanum Wiki: Johnny Decimal Extended (XXX.YYY) Reorganization
Renames all folders and files to a consistent numeric sort order.
Creates index.md in every folder.
Updates all cross-references.
"""

import os, re, shutil
from pathlib import Path
from collections import defaultdict

ARCANUM = Path(r"C:/Users/atayl/VoxCore/doc/arcanum")
STAGING = Path(r"C:/Users/atayl/VoxCore/doc/arcanum_staging")

# ============================================================================
# JOHNNY DECIMAL FOLDER MAP
# Area 100-199: Foundation
# Area 200-299: API & Data Flow
# Area 300-399: Capabilities
# Area 400-499: Extension Points
# Area 500-599: Multi-Agent & Integration
# Area 600-699: Interface & Config
# Area 700-799: Infrastructure
# Area 800-899: Monitoring
# Area 900-999: Reference
# ============================================================================

AREAS = {
    100: "Foundation",
    200: "API & Data Flow",
    300: "Capabilities",
    400: "Extension Points",
    500: "Multi-Agent & Integration",
    600: "Interface & Config",
    700: "Infrastructure",
    800: "Monitoring",
    900: "Reference",
}

FOLDER_ORDER = [
    # (old_name, category_num)
    ('core',        110),
    ('internals',   120),
    ('api',         210),
    ('query',       220),
    ('tools',       310),
    ('commands',    320),
    ('skills',      330),
    ('hooks',       410),
    ('permissions', 420),
    ('mcp',         430),
    ('plugins',     440),
    ('agents',      510),
    ('bridge',      520),
    ('ui',          610),
    ('config',      620),
    ('services',    630),
    ('sandbox',     710),
    ('networking',  720),
    ('git',         730),
    ('limits',      810),
    ('telemetry',   820),
    ('hidden',      910),
    ('guides',      920),
    ('source',      930),
]

# Custom topic ordering for key folders (topic_base -> priority)
# Lower = earlier. Unspecified topics default to 500 (alphabetical).
CUSTOM_ORDER = {
    'core': {
        'architecture': 10, 'glossary': 20, 'system_prompt': 30,
        'claude_md_injection': 40, 'context_window': 50, 'token_counting': 60,
        'memory_overview': 70, 'memory_selector': 80, 'memory_limits': 90,
        'compaction_overview': 100, 'compaction_tiers': 110,
        'compaction_instructions': 120, 'rules_system': 130, 'autodream': 140,
    },
    'tools': {
        'pipeline_overview': 10, 'tool_agent': 20, 'tool_ask_user': 30,
        'tool_bash': 40, 'tool_cron': 50, 'tool_edit': 60, 'tool_glob': 70,
        'tool_grep': 80, 'tool_lsp': 90, 'tool_mcp': 100, 'tool_misc': 110,
        'tool_notebook_edit': 120, 'tool_plan_mode': 130, 'tool_read': 140,
        'tool_skill': 150, 'tool_tasks': 160, 'tool_teams': 170,
        'tool_web_fetch': 180, 'tool_web_search': 190, 'tool_worktree': 200,
        'tool_write': 210,
    },
    'agents': {
        'coordinator_overview': 10, 'subagent_types': 20, 'fork_mode': 30,
        'swarm_overview': 40, 'swarm_backends': 50, 'swarm_messaging': 60,
        'teams_and_tasks': 70,
    },
    'hidden': {
        'voice_mode': 10, 'computer_use': 20, 'buddy_system': 30,
        'ultraplan': 40, 'teleport': 50, 'deep_link': 60,
        'claude_in_chrome': 70, 'dxt_extensions': 80, 'thinkback': 90,
        'stickers_and_good_claude': 100, 'moreright': 110,
    },
}


def get_base_and_part(filename):
    """Extract base topic and part number. 'foo_pt2.md' -> ('foo', 2)."""
    name = filename.replace('.md', '')
    m = re.match(r'^(.+?)_pt(\d+)$', name)
    if m:
        return m.group(1), int(m.group(2))
    return name, 0


def get_topic_priority(topic, folder_name):
    """Get sort priority for a topic within its folder."""
    custom = CUSTOM_ORDER.get(folder_name, {})
    if topic in custom:
        return custom[topic]
    # Default: overview first, then alphabetical
    if 'overview' in topic:
        return 5
    if 'index' in topic:
        return 1
    return 500


def order_files(md_files, cat_num, folder_name):
    """Order files and assign XXX.YYY numbers. Returns [(new_name, old_name)]."""
    topics = defaultdict(list)
    for f in md_files:
        base, part = get_base_and_part(f)
        topics[base].append((part, f))

    sorted_topics = sorted(topics.keys(),
                           key=lambda t: (get_topic_priority(t, folder_name), t))

    result = []
    item_num = 10
    for topic in sorted_topics:
        parts = sorted(topics[topic])
        for i, (part_n, old_filename) in enumerate(parts):
            new_name = f"{cat_num}.{item_num + i:03d}_{old_filename}"
            result.append((new_name, old_filename))
        next_num = item_num + len(parts)
        item_num = ((next_num - 1) // 10 + 1) * 10

    return result


def get_area_for(cat_num):
    """Get area description for a category number."""
    area_base = (cat_num // 100) * 100
    return AREAS.get(area_base, "Unknown")


def create_folder_index(cat_num, folder_name, file_list):
    """Create XXX.000_index.md content for a folder."""
    area = get_area_for(cat_num)
    lines = ['---']
    lines.append(f'title: "{cat_num} {folder_name.title()} — Index"')
    lines.append(f'description: "Index of {folder_name} articles — {area}"')
    lines.append(f'tags: [{folder_name}, index, {area.lower().replace(" & ", "-").replace(" ", "-")}]')
    lines.append('---')
    lines.append('')
    lines.append(f'# {cat_num} {folder_name.title()}')
    lines.append(f'> Area: {area}')
    lines.append('')

    current_topic = None
    for new_name, old_name in file_list:
        base, part = get_base_and_part(old_name)
        display = base.replace('_', ' ').title()
        if part > 0:
            display += f' (Part {part})'
        lines.append(f'- [{new_name}]({new_name})')

    lines.append('')
    return '\n'.join(lines)


def create_master_index(all_folders):
    """Create root-level 000.000_index.md."""
    lines = ['---']
    lines.append('title: "Arcanum Wiki — Master Index"')
    lines.append('description: "Complete index of Claude Code internals knowledge base, Johnny Decimal XXX.YYY"')
    lines.append('tags: [index, arcanum, master, johnny-decimal]')
    lines.append('---')
    lines.append('')
    lines.append('# Arcanum Wiki — Master Index')
    lines.append('')
    lines.append('Claude Code internals knowledge base. 22 deep-dive reports distilled into')
    lines.append(f'{sum(len(v) for v in all_folders.values())} articles across {len(all_folders)} categories.')
    lines.append('')

    current_area = None
    for old_name, cat_num in FOLDER_ORDER:
        area_base = (cat_num // 100) * 100
        area_name = AREAS.get(area_base, "")
        if area_base != current_area:
            current_area = area_base
            lines.append(f'## {area_base}-{area_base+99}: {area_name}')
            lines.append('')

        folder_dir = f"{cat_num}_{old_name}"
        count = len(all_folders.get(folder_dir, []))
        idx = f"{cat_num}.000_index.md"
        lines.append(f'- **[{cat_num} {old_name.title()}]({folder_dir}/{idx})** ({count} articles)')

    lines.append('')
    return '\n'.join(lines)


def main():
    # Phase 1: Build complete rename mapping
    print("Phase 1: Building rename map...")
    rename_map = {}       # old_rel_path -> new_rel_path
    filename_map = {}     # old_filename -> new_filename
    folder_name_map = {}  # old_folder -> new_folder
    all_folder_files = {} # new_folder -> [(new_name, old_name)]

    for old_name, cat_num in FOLDER_ORDER:
        old_dir = ARCANUM / old_name
        new_folder = f"{cat_num}_{old_name}"
        folder_name_map[old_name] = new_folder

        if not old_dir.exists():
            print(f"  SKIP {old_name}/ (not found)")
            continue

        md_files = sorted(f for f in os.listdir(old_dir) if f.endswith('.md'))
        if not md_files:
            all_folder_files[new_folder] = []
            continue

        numbered = order_files(md_files, cat_num, old_name)
        all_folder_files[new_folder] = numbered

        for new_name, old_filename in numbered:
            old_rel = f"{old_name}/{old_filename}"
            new_rel = f"{new_folder}/{new_name}"
            rename_map[old_rel] = new_rel
            rename_map[old_filename] = new_name  # bare filename too
            filename_map[old_filename] = new_name

    # Handle root-level files
    root_md = sorted(f for f in os.listdir(ARCANUM)
                     if f.endswith('.md') and os.path.isfile(ARCANUM / f))
    root_numbered = order_files(root_md, 0, '_root') if root_md else []
    for new_name, old_filename in root_numbered:
        rename_map[old_filename] = new_name
        filename_map[old_filename] = new_name

    print(f"  {len(rename_map)} path mappings built")
    print(f"  {len(all_folder_files)} folders to process")

    # Phase 2: Create staging directory with new structure
    print("\nPhase 2: Creating new structure...")
    if STAGING.exists():
        shutil.rmtree(STAGING)
    STAGING.mkdir()

    files_copied = 0
    for old_name, cat_num in FOLDER_ORDER:
        old_dir = ARCANUM / old_name
        new_folder = f"{cat_num}_{old_name}"
        new_dir = STAGING / new_folder
        new_dir.mkdir(exist_ok=True)

        if not old_dir.exists():
            continue

        # Copy .md files with new names
        for new_name, old_filename in all_folder_files.get(new_folder, []):
            src = old_dir / old_filename
            dst = new_dir / new_name
            if src.exists():
                shutil.copy2(src, dst)
                files_copied += 1

        # Copy non-.md files as-is (source .txt files etc)
        for f in os.listdir(old_dir):
            if not f.endswith('.md'):
                src = old_dir / f
                dst = new_dir / f
                if src.is_file():
                    shutil.copy2(src, dst)
                    files_copied += 1

    # Copy root-level files
    for new_name, old_filename in root_numbered:
        src = ARCANUM / old_filename
        dst = STAGING / new_name
        if src.exists():
            shutil.copy2(src, dst)
            files_copied += 1

    print(f"  {files_copied} files copied")

    # Phase 3: Update cross-references in all files
    print("\nPhase 3: Updating cross-references...")
    links_updated = 0

    def replace_link(match):
        nonlocal links_updated
        link_text = match.group(1)
        link_path = match.group(2)

        if link_path.startswith('http') or link_path.startswith('#'):
            return match.group(0)

        # Try exact match (bare filename or folder/file)
        if link_path in rename_map:
            links_updated += 1
            return f'[{link_text}]({rename_map[link_path]})'

        # Try with ../ prefix stripped
        if link_path.startswith('../'):
            rel = link_path[3:]
            if rel in rename_map:
                links_updated += 1
                return f'[{link_text}](../{rename_map[rel]})'
            # Try just the filename part
            fname = rel.split('/')[-1] if '/' in rel else rel
            if fname in filename_map:
                # Reconstruct with new folder
                old_folder = rel.split('/')[0] if '/' in rel else None
                if old_folder and old_folder in folder_name_map:
                    new_path = f"../{folder_name_map[old_folder]}/{filename_map[fname]}"
                    links_updated += 1
                    return f'[{link_text}]({new_path})'

        # Try just filename part for same-folder refs
        fname = link_path.split('/')[-1]
        if fname in filename_map:
            links_updated += 1
            return f'[{link_text}]({filename_map[fname]})'

        return match.group(0)

    for root, dirs, files in os.walk(STAGING):
        for fname in files:
            if not fname.endswith('.md'):
                continue
            fpath = Path(root) / fname
            try:
                content = fpath.read_text(encoding='utf-8')
                new_content = re.sub(r'\[([^\]]*)\]\(([^)]+)\)', replace_link, content)
                if new_content != content:
                    fpath.write_text(new_content, encoding='utf-8')
            except Exception as e:
                print(f"  ERROR updating {fpath}: {e}")

    print(f"  {links_updated} links updated")

    # Phase 4: Create index files
    print("\nPhase 4: Creating index files...")
    indices_created = 0

    for old_name, cat_num in FOLDER_ORDER:
        new_folder = f"{cat_num}_{old_name}"
        new_dir = STAGING / new_folder
        if not new_dir.exists():
            new_dir.mkdir(exist_ok=True)

        file_list = all_folder_files.get(new_folder, [])
        idx_content = create_folder_index(cat_num, old_name, file_list)
        idx_path = new_dir / f"{cat_num}.000_index.md"
        idx_path.write_text(idx_content, encoding='utf-8')
        indices_created += 1

    # Master index
    master = create_master_index(all_folder_files)
    (STAGING / "000.000_index.md").write_text(master, encoding='utf-8')
    indices_created += 1

    print(f"  {indices_created} index files created")

    # Phase 5: Swap directories
    print("\nPhase 5: Swapping directories...")
    backup = ARCANUM.parent / "arcanum_backup"
    if backup.exists():
        shutil.rmtree(backup)
    ARCANUM.rename(backup)
    STAGING.rename(ARCANUM)
    print(f"  Old -> {backup}")
    print(f"  New -> {ARCANUM}")

    # Final stats
    total_files = sum(1 for _ in ARCANUM.rglob('*.md'))
    total_folders = sum(1 for d in ARCANUM.iterdir() if d.is_dir())
    print(f"\n{'='*60}")
    print(f"DONE: {total_files} .md files in {total_folders} folders")
    print(f"Backup at: {backup}")
    print(f"To finalize: delete {backup} when satisfied")
    print(f"{'='*60}")


if __name__ == '__main__':
    main()
