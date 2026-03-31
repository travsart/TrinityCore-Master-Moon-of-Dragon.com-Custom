"""
Create VoxCore desktop shortcuts — organized into folders with WoW icons.

Rebuilds all shortcuts from scratch: cleans old ones, creates themed folders
with custom icons, and places named/numbered shortcuts inside.

Usage:
    python create_shortcuts.py            # Full rebuild (clean + create)
    python create_shortcuts.py --clean    # Only remove old shortcuts/folders
"""
import os
import shutil
import subprocess
import sys
import win32com.client

from PIL import Image

# ═══════════════════════════════════════════════════════════════════
#  PATHS
# ═══════════════════════════════════════════════════════════════════

import sys
try:
    sys.path.append(str(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "bootstrap"))))
    from resolve_roots import find_project_root
    ROOT = str(find_project_root())
except ImportError:
    ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

ICON_SRC = os.path.join(ROOT, r"wago\att_icons_export\8K_Format\wow_icons\large")
ICON_DST = os.path.join(ROOT, r"tools\shortcuts\icons")
DESKTOP  = os.path.join(os.environ["USERPROFILE"], "Desktop")

RUNTIME  = os.path.join(ROOT, r"out\build\x64-RelWithDebInfo\bin\RelWithDebInfo")
TOOLS    = os.path.join(ROOT, "tools")
SC_DIR   = os.path.join(TOOLS, "shortcuts")
WAGO     = os.path.join(ROOT, "wago")
EXTTOOLS = os.path.join(ROOT, "ExtTools")
CHROME   = r"C:\Program Files\Google\Chrome\Application\chrome.exe"

FOLDERS = [
    "VC Server", "VC Build", "VC Pipeline",
    "VC Packets", "VC Audits", "VC Web", "VC Tools",
    "VC Data",  # legacy duplicate folder — included so clean_all() removes it
]

# ═══════════════════════════════════════════════════════════════════
#  ICON CONVERSION
# ═══════════════════════════════════════════════════════════════════

def png_to_ico(png_name: str) -> str:
    """Convert a PNG from the icon library to ICO, return path."""
    os.makedirs(ICON_DST, exist_ok=True)
    base = os.path.splitext(png_name)[0]
    ico_path = os.path.join(ICON_DST, base + ".ico")
    if os.path.exists(ico_path):
        return ico_path
    src = os.path.join(ICON_SRC, png_name)
    img = Image.open(src).convert("RGBA")
    # Cover-crop non-square images: scale so short edge fills 256, center-crop
    w, h = img.size
    if w != h:
        target = 256
        scale = target / min(w, h)
        img = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
        nw, nh = img.size
        left = (nw - target) // 2
        top = (nh - target) // 2
        img = img.crop((left, top, left + target, top + target))
    sizes = [(256, 256), (48, 48), (32, 32), (16, 16)]
    imgs = [img.resize(s, Image.LANCZOS) for s in sizes]
    imgs[0].save(ico_path, format="ICO", sizes=sizes, append_images=imgs[1:])
    return ico_path

# ═══════════════════════════════════════════════════════════════════
#  SHORTCUT / FOLDER HELPERS
# ═══════════════════════════════════════════════════════════════════

def make_shortcut(folder_path, name, target, args=None, work_dir=None,
                  desc=None, icon_png=None):
    """Create a .lnk shortcut inside a folder."""
    ws = win32com.client.Dispatch("WScript.Shell")
    lnk_path = os.path.join(folder_path, f"{name}.lnk")
    s = ws.CreateShortcut(lnk_path)
    s.TargetPath = target
    if args:
        s.Arguments = args
    if work_dir:
        s.WorkingDirectory = work_dir
    if desc:
        s.Description = desc[:259]
    if icon_png:
        ico = png_to_ico(icon_png)
        s.IconLocation = f"{ico},0"
    s.Save()
    print(f"    {name}")


def make_folder(name, icon_png):
    """Create a desktop folder with a custom WoW icon via desktop.ini."""
    folder_path = os.path.join(DESKTOP, name)
    os.makedirs(folder_path, exist_ok=True)

    ico_path = png_to_ico(icon_png)

    ini_path = os.path.join(folder_path, "desktop.ini")
    if os.path.exists(ini_path):
        subprocess.run(["attrib", "-S", "-H", ini_path], capture_output=True)
        os.remove(ini_path)

    with open(ini_path, "w", encoding="utf-8") as f:
        f.write("[.ShellClassInfo]\n")
        f.write(f"IconResource={ico_path},0\n")

    subprocess.run(["attrib", "+S", "+H", ini_path], capture_output=True)
    subprocess.run(["attrib", "+R", folder_path], capture_output=True)

    print(f"\n  [{name}]")
    return folder_path


def clean_all():
    """Remove old flat shortcuts and VC folders from Desktop."""
    removed = 0
    for f in os.listdir(DESKTOP):
        fp = os.path.join(DESKTOP, f)
        if f.startswith("VC - ") and f.endswith(".lnk"):
            os.remove(fp)
            removed += 1
        elif f in FOLDERS and os.path.isdir(fp):
            # Un-readonly the folder first so we can delete it
            subprocess.run(["attrib", "-R", fp], capture_output=True)
            ini = os.path.join(fp, "desktop.ini")
            if os.path.exists(ini):
                subprocess.run(["attrib", "-S", "-H", ini], capture_output=True)
            shutil.rmtree(fp, ignore_errors=True)
            removed += 1
    if removed:
        print(f"  Cleaned {removed} old shortcut(s)/folder(s).")


# ═══════════════════════════════════════════════════════════════════
#  ALL SHORTCUTS
# ═══════════════════════════════════════════════════════════════════

def create_all():
    print("=" * 60)
    print("  VoxCore Desktop Shortcuts — Full Rebuild")
    print("=" * 60)

    clean_all()

    # ── 1. VC SERVER ───────────────────────────────────────────────
    # Get the server running. One-click play or individual components.
    d = make_folder("VC Server", "inv_shield_04.png")

    make_shortcut(d, "Play (Start All)",
        "cmd.exe", f'/k "{SC_DIR}\\start_all.bat"',
        RUNTIME,
        "ONE CLICK TO PLAY — starts MySQL, bnetserver, worldserver, and Arctium Game Launcher in sequence. Needs admin for MySQL service.",
        "inv_hearthstone_gold.png")

    make_shortcut(d, "Stop All",
        "cmd.exe", f'/k "{SC_DIR}\\stop_all.bat"',
        RUNTIME,
        "Clean shutdown — kills worldserver + bnetserver processes and stops the MySQL80 Windows service. Safe to run anytime.",
        "ability_creature_cursed_02.png")

    make_shortcut(d, "Worldserver (solo)",
        "cmd.exe", "/k worldserver.exe",
        RUNTIME,
        "Launch ONLY worldserver.exe (RelWithDebInfo build). Assumes MySQL and bnetserver are already running. Console stays open — type server commands directly.",
        "inv_misc_head_dragon_blue.png")

    make_shortcut(d, "Bnetserver (solo)",
        "cmd.exe", "/k bnetserver.exe",
        RUNTIME,
        "Launch ONLY bnetserver.exe (RelWithDebInfo build). Handles account auth and realm list. Must be running before worldserver.",
        "spell_nature_lightning.png")

    make_shortcut(d, "Start MySQL (solo)",
        "cmd.exe", f'/k "{SC_DIR}\\start_mysql_uniserverz.bat"',
        RUNTIME,
        "Start UniServerZ MySQL 9.5.0 with game databases. No admin needed. Use this when you want the database without starting the game servers.",
        "inv_datacrystal06.png")

    # ── 2. VC BUILD ────────────────────────────────────────────────
    # Compile the server. Numbered because configure must come before build.
    d = make_folder("VC Build", "inv_blacksmith_anvil.png")

    make_shortcut(d, "1. Configure CMake",
        "cmd.exe", f'/k "{TOOLS}\\build\\configure.bat"',
        ROOT,
        "STEP 1 — Run after adding new source files or changing CMake options. Configures both x64-Debug and x64-RelWithDebInfo presets. Only needed when cmake files change, not every build.",
        "inv_misc_gear_01.png")

    make_shortcut(d, "2. Build Release",
        "cmd.exe", f'/k "{TOOLS}\\build\\build.bat"',
        ROOT,
        "STEP 2 — Full x64-RelWithDebInfo build (primary runtime, ~17s startup). This is the build you play with. Uses cmake --build with vcvarsall x64.",
        "inv_hammer_03.png")

    make_shortcut(d, "Build Debug",
        "cmd.exe", f'/k "{TOOLS}\\build\\build_debug.bat"',
        ROOT,
        "Full x64-Debug build (~60s startup). Use for breakpoints and step-through debugging in VS 2026. Slower to start but gives full symbol info.",
        "inv_hammer_05.png")

    make_shortcut(d, "Build Scripts Only",
        "cmd.exe", f'/k "{SC_DIR}\\build_scripts_rel.bat"',
        ROOT,
        "FAST ITERATION — builds only the 'scripts' ninja target in RelWithDebInfo. Use when you only changed .cpp files in src/server/scripts/Custom/. Much faster than a full build.",
        "inv_misc_scrollrolled01.png")

    # ── 3. VC PIPELINE ─────────────────────────────────────────────
    # The big data pipeline. Run these in order after a build bump
    # or when refreshing data from external sources.
    d = make_folder("VC Pipeline", "inv_misc_treasurechest04b.png")

    make_shortcut(d, "1. DB Snapshot (backup)",
        "cmd.exe", '/k python db_snapshot.py snapshot --label pre-pipeline',
        WAGO,
        "STEP 1 — ALWAYS DO THIS FIRST. Creates a mysqldump backup of all 5 databases (auth, characters, world, hotfixes, roleplay) with a timestamped label. Use 'db_snapshot.py rollback' to undo if anything goes wrong.",
        "inv_misc_book_01.png")

    make_shortcut(d, "2. TACT Extract (DB2 to CSV)",
        "cmd.exe", '/k python tact_extract.py --verify',
        WAGO,
        "STEP 2 — Extracts raw DB2 files from your local WoW CASC install and converts them to CSV via DBC2CSV. The --verify flag compares row counts against existing CSVs. Takes ~50s. Output goes to wago_csv/ dir.",
        "inv_datacrystal01.png")

    make_shortcut(d, "3. Merge CSV Sources",
        "cmd.exe", '/k python merge_csv_sources.py',
        WAGO,
        "STEP 3 — Combines TACT-extracted CSVs (client ground truth) with Wago CSVs (CDN hotfix content). For tables where Wago has MORE rows, appends the extras. Output: merged_csv/{build}/enUS/ — the single source of truth for all downstream scripts.",
        "inv_datacrystal03.png")

    make_shortcut(d, "4. Hotfix Repair (batch 1-5)",
        "cmd.exe", '/k python repair_hotfix_tables.py --batch 1',
        WAGO,
        "STEP 4 — Compares hotfix DB tables against merged CSVs and fixes mismatches. Run with --batch 1 through --batch 5 (change the number). Each batch covers different table ranges. Generates ~71 MB SQL total across all 5 batches.",
        "ability_repair.png")

    make_shortcut(d, "5. Raidbots Import (8 steps)",
        "cmd.exe", r'/k python raidbots\run_all_imports.py',
        WAGO,
        "STEP 5 — 8-step Raidbots import: quest chains, objectives, POI, locales (6 langs), vendors, orphan fix. Use --regenerate after new JSONs.",
        "inv_misc_coin_02.png")

    make_shortcut(d, "6. World Health Check",
        "cmd.exe", '/k python world_health_check.py',
        WAGO,
        "STEP 6 — Validates referential integrity of the world DB after imports. Checks for orphaned creature/GO references, missing templates, broken loot chains, invalid spell IDs, and other cross-table consistency issues.",
        "spell_holy_divinepurpose.png")

    make_shortcut(d, "7. DB Error Parser",
        "cmd.exe", r'/k python tools\parse_dberrors.py',
        ROOT,
        "STEP 7 — Parses DBErrors.log from the last server run and categorizes ALL error patterns by system (hotfix, spell_proc, creature, SmartAI, etc.) with counts and fix descriptions. Run after server startup to see what data issues remain.",
        "ability_creature_cursed_01.png")

    make_shortcut(d, "8. Optimize DB",
        "cmd.exe", f'/k "{TOOLS}\\_optimize_db.bat"',
        TOOLS,
        "STEP 8 (OPTIONAL) — Runs ALTER TABLE FORCE on all InnoDB tables >10 MB across all 5 databases. Reclaims disk space fragmented by bulk imports/deletes. Reports before/after sizes. Safe to run anytime.",
        "ability_rogue_sprint.png")

    make_shortcut(d, "Wago DB2 Download (web)",
        "cmd.exe", '/k python wago_db2_downloader.py',
        WAGO,
        "ALTERNATIVE to TACT Extract — downloads DB2 CSVs directly from wago.tools website. Slower but doesn't need a local WoW install. Use when TACT extract is unavailable or for a specific table.",
        "inv_misc_questionmark.png")

    make_shortcut(d, "DB Snapshot List-Rollback",
        "cmd.exe", '/k python db_snapshot.py list',
        WAGO,
        "View all database snapshots with timestamps and labels. To rollback: db_snapshot.py rollback --id N --confirm. To prune old ones: db_snapshot.py prune --keep 5.",
        "inv_misc_book_03.png")

    # ── 4. VC PACKETS ──────────────────────────────────────────────
    # Packet capture → parse → analyze workflow.
    d = make_folder("VC Packets", "inv_misc_spyglass_02.png")

    make_shortcut(d, "Dev Session (auto-archive + WPP)",
        "cmd.exe", f'/k bash {ROOT}/tools-dev/tc-packet-tools/start-worldserver.sh',
        RUNTIME,
        "THE SMART LAUNCHER — archives previous session (packets, logs, SQL into timestamped folder), starts bnet+world with EXIT trap, auto-runs WPP on World.pkt when you Ctrl+C, then runs Packet Scope analysis. Best for debugging sessions.",
        "inv_hearthstone_gold.png")

    make_shortcut(d, "1. YMIR Sniffer",
        os.path.join(EXTTOOLS, r"ymir_retail_12.0.1.66666\ymir_retail.exe"), None,
        os.path.join(EXTTOOLS, "ymir_retail_12.0.1.66666"),
        "STEP 1 — Retail packet sniffer for 12.0.1 build 66666. Captures live WoW traffic into .pkt files. Launch this BEFORE opening the game client. Output: .pkt files for WPP to parse.",
        "ability_hunter_snipershot.png")

    make_shortcut(d, "2. WowPacketParser",
        os.path.join(EXTTOOLS, r"WowPacketParser\WowPacketParser.exe"), None,
        os.path.join(EXTTOOLS, "WowPacketParser"),
        "STEP 2 — Parses .pkt capture files into human-readable World_parsed.txt plus SQL extracts. Drag-and-drop a .pkt file or use the GUI. Outputs to the WPP directory.",
        "inv_misc_spyglass_03.png")

    make_shortcut(d, "3. Packet Scope",
        "cmd.exe", r'/k python tools\packet_scope.py',
        ROOT,
        "STEP 3 — Analyzes WPP output with transmog-specific decoding. Finds CMSG/SMSG_TRANSMOG packets, decodes outfit data, extracts ViewedOutfit UpdateObject fields, checks for addon messages (TMOG_LOG, TSPY_LOG). Reads from default PacketLog/ dir.",
        "ability_rogue_bloodyeye.png")

    make_shortcut(d, "4. Opcode Analyzer",
        "cmd.exe", r'/k python tools\opcode_analyzer.py',
        ROOT,
        "STEP 4 — Builds a complete opcode dictionary from Opcodes.h/cpp, then cross-references with WPP parsed output to find unhandled, unknown, and filtered opcodes. Use --lookup TRANSMOG to search, --top 20 for most-seen.",
        "inv_letter_02.png")

    # ── 5. VC AUDITS ───────────────────────────────────────────────
    # Data quality audits. All read-only, safe to run anytime.
    d = make_folder("VC Audits", "inv_misc_questionmark.png")

    make_shortcut(d, "NPC Audit (27 checks)",
        "cmd.exe", '/k python npc_audit.py --help',
        WAGO,
        "27 NPC audits: levels, flags, faction, classification, type, duplicates, phases, missing, display, names, scale, speed, equipment, gossip, waypoints, SmartAI, loot, auras, family, unitclass, title, and more. Use: npc_audit.py all --report",
        "ability_hunter_pet_assist.png")

    make_shortcut(d, "Quest Audit (15 checks)",
        "cmd.exe", '/k python quest_audit.py --help',
        WAGO,
        "15 quest audits: broken chains, exclusive groups, missing givers/enders, invalid objectives/rewards/start items, missing quests, orphaned data, POI, questline cross-ref, addon sync, duplicates. Use: quest_audit.py all --report",
        "inv_misc_scrollrolled01c.png")

    make_shortcut(d, "GO Audit (15 checks)",
        "cmd.exe", '/k python go_audit.py --help',
        WAGO,
        "15 GameObject audits: duplicates, phases, display, type, scale, loot, quest refs, pools, events, names, SmartAI, spawntime, addon orphans, missing templates, faction. Use: go_audit.py all --report",
        "inv_misc_gear_03.png")

    make_shortcut(d, "Transmog Validate (7 checks)",
        "cmd.exe", '/k python validate_transmog.py',
        WAGO,
        "Cross-references wow.tools.local DB2 exports against MySQL hotfixes. Finds missing server rows, FK violations, value mismatches, TransmogSet resolution issues, orphaned entries, illusion IDs. Runs in <1s across 155K IMAIDs.",
        "inv_chest_plate01.png")

    make_shortcut(d, "Transmog Debug",
        "cmd.exe", '/k python transmog_debug.py --help',
        WAGO,
        "Full transmog state viewer. Cross-refs character DB, outfit DB, and Wago DB2 CSVs. Use: --char Hexandchill (full state), --imaid 304252 (resolve one), --outfit 7 (outfit table), --diff 7 (equipped vs saved), --log (Debug.log), --spy (addon data).",
        "inv_chest_plate05.png")

    make_shortcut(d, "Transmog Lookup",
        "cmd.exe", '/k python transmog_lookup.py --help',
        WAGO,
        "DB2 cross-reference tool. Use: imaid <id> (full lookup), search <name> (item search), dt (DisplayType table), reverse <itemid> (find all IMAIDs), analyze <logfile> (Debug.log session summary), batch <strings> (batch lookups).",
        "inv_chest_plate03.png")

    make_shortcut(d, "Scraper v3 (Tor Army)",
        "cmd.exe", '/k python scraper_v3.py --help',
        WAGO,
        "Async swarm scraper — 400 Tor instances, 600K-1M pages/hr. Targets: npc, quest, item, spell. Features: jittered backoff, WAF auto-stop, live dashboard, resumable. Use: --smoke 50 --targets npc --workers 10 --start-tor for testing.",
        "inv_misc_web_01.png")

    # ── 6. VC WEB ──────────────────────────────────────────────────
    # Dashboards, documentation, web tools.
    d = make_folder("VC Web", "spell_arcane_portaldalaran.png")

    make_shortcut(d, "VoxCore Docs",
        CHROME, "https://voxcore84.github.io/roleplaycore-report/index.html",
        None,
        "VoxCore project documentation — framework, pipeline, tooling, AI workflow, status, and results. Hosted on GitHub Pages.",
        "inv_misc_book_05.png")

    make_shortcut(d, "Task Tracker",
        CHROME, '--app="file:///C:/Users/atayl/cowork/outputs/roleplaycore-tracker.html"',
        None,
        "GitHub issue/PR tracker dashboard for VoxCore84/RoleplayCore. Shows open issues, recent PRs, task status. Opens in a clean Chrome window.",
        "achievement_guildperk_workingovertime.png")

    make_shortcut(d, "Spell Audit Tracker",
        CHROME, '--app="file:///C:/Users/atayl/VoxCore/wago/audit_reports/spell_audit_report.html"',
        None,
        "Class/spec spell implementation tracker — 4,965 spells classified across 40 specs. Shows C++ stubs, fix categories, SimC cross-refs.",
        "spell_holy_prayerofhealing02.png")

    make_shortcut(d, "ATT Browser",
        "cmd.exe", '/k python app.py --port 5051',
        os.path.join(WAGO, "att_browser"),
        "AllTheThings database browser — Flask web UI backed by SQLite (att.db). Mirrors the in-game addon: tree navigation, search, detail views. Opens on localhost:5051.",
        "achievement_guildperk_mobilebanking.png")

    make_shortcut(d, "WebTerm",
        os.path.join(EXTTOOLS, "launch-webterm.bat"), None,
        EXTTOOLS,
        "Web-based terminal on localhost:7681 — Flask + SocketIO server with a browser frontend. Opens in Chrome app mode. Cowork (Claude Desktop) can access this to run commands remotely.",
        "trade_engineering.png")

    make_shortcut(d, "Update Website",
        "cmd.exe", f'/k "{ROOT}\\website\\update_site.bat"',
        os.path.join(ROOT, "website"),
        "Rebuild the VoxCore docs site from local sources, then git add + commit + push to deploy. Site goes live on GitHub Pages in ~30 seconds.",
        "inv_letter_01.png")

    make_shortcut(d, "AI Toolkit Reference",
        CHROME, '--app="file:///C:/Users/atayl/VoxCore/doc/AI-Toolkit-Reference.html"',
        None,
        "Claude AI toolkit reference documentation — covers all MCP servers, slash commands, skills, and automation used in this project.",
        "inv_misc_book_03.png")

    make_shortcut(d, "GitHub Repo",
        CHROME, "https://github.com/VoxCore84/RoleplayCore",
        None,
        "VoxCore84/RoleplayCore — main GitHub repository. Issues, PRs, commits, CI.",
        "achievement_guildperk_ladyluck.png")

    make_shortcut(d, "Discord (Dev Channel)",
        CHROME, "https://discord.com/channels/1231835263861395487/1231843017732788265",
        None,
        "VoxCore development Discord channel — chat, coordination, and announcements.",
        "ui_chat.png")

    make_shortcut(d, "Wago.tools",
        CHROME, "https://wago.tools/db2",
        None,
        "Wago.tools DB2 browser — online reference for all 1,097 WoW DB2 tables. Use for column lookups, data verification, and hotfix comparisons.",
        "inv_enchant_formulagood_01.png")

    # ── 7. VC TOOLS ────────────────────────────────────────────────
    # Miscellaneous utilities and quick access.
    d = make_folder("VC Tools", "inv_misc_wrench_01.png")

    make_shortcut(d, "wow.tools.local",
        "cmd.exe", f'/k "{EXTTOOLS}\\WoW.tools\\start_wtl.bat"',
        os.path.join(EXTTOOLS, "WoW.tools"),
        "Start wow.tools.local — local DB2 data browser at http://localhost:5000 (build 66666). Browse item appearances, spells, creatures, maps, and all 1,097 DB2 tables. Waits for startup then opens browser.",
        "inv_misc_spyglass_01.png")

    make_shortcut(d, "DBC2CSV",
        os.path.join(EXTTOOLS, r"DBC2CSV\DBC2CSV.exe"), None,
        os.path.join(EXTTOOLS, "DBC2CSV"),
        "GUI tool to convert raw .db2/.dbc files to CSV format. Drag-and-drop or browse for files. Used by TACT Extract internally, but useful standalone for one-off conversions.",
        "inv_datacrystal06.png")

    make_shortcut(d, "Spell Creator",
        "cmd.exe", r'/k python tools\spell_creator.py',
        ROOT,
        "Interactive spell creation CLI - templates, cloning, CSV lookup, hotfix SQL generation, SOAP reload. 11 templates, 1888 spell DB. Replaces old .NET SpellCreator.",
        "inv_wand_07.png")

    make_shortcut(d, "Restart Cowork",
        "cmd.exe", f'/k "{EXTTOOLS}\\restart-cowork.bat"',
        ROOT,
        "Force-restart the Cowork (Claude Desktop) service — kills Claude.exe + cowork-svc.exe, restarts CoworkVMService, clears temp. Use when Cowork is stuck or unresponsive. Needs admin.",
        "inv_misc_enggizmos_03.png")

    make_shortcut(d, "Server Logs",
        "explorer.exe", RUNTIME,
        None,
        "Open the server runtime directory in Explorer — contains Server.log, DBErrors.log, Debug.log, GM.log, worldserver.conf, and PacketLog/ folder.",
        "inv_misc_note_01.png")

    make_shortcut(d, "Project Folder",
        "explorer.exe", ROOT,
        None,
        "Open the VoxCore project root (C:\\Users\\atayl\\VoxCore) in Explorer.",
        "inv_misc_head_dragon_bronze.png")

    make_shortcut(d, "CC Sync Report",
        "cmd.exe", r'/k python command-center\sync_from_desktop.py',
        TOOLS,
        "Compare desktop VC folder shortcuts against Command Center cards. Shows missing, extra, and matched items.",
        "ability_monk_roll.png")

    # ── Summary ────────────────────────────────────────────────────
    total = sum(len([f for f in os.listdir(os.path.join(DESKTOP, d))
                     if f.endswith('.lnk')])
                for d in FOLDERS if os.path.isdir(os.path.join(DESKTOP, d)))
    print(f"\n{'=' * 60}")
    print(f"  Done! {len(FOLDERS)} folders, {total} shortcuts, all with WoW icons.")
    print(f"  Folder icons may need a Desktop refresh (F5) to appear.")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    if "--clean" in sys.argv:
        clean_all()
        print("Done.")
    else:
        create_all()
