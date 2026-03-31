"""
VoxCore Command Center — WoW-themed launcher dashboard.

Usage:
    python app.py              # Start on http://localhost:5050
    python app.py --port 8080  # Custom port
    python app.py --prep       # Only prepare assets (icons, art), don't start server
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

from flask import Flask, jsonify, render_template, request, send_from_directory
from PIL import Image

# ═══════════════════════════════════════════════════════════════════
#  PATHS
# ═══════════════════════════════════════════════════════════════════

APP_DIR      = Path(__file__).resolve().parent
STATIC_DIR   = APP_DIR / "static"

import sys
try:
    # Try importing the common root resolver from the repo scripts/bootstrap
    sys.path.append(str(APP_DIR.parent.parent / "scripts" / "bootstrap"))
    from resolve_roots import find_project_root
    ROOT = str(find_project_root())
except ImportError:
    # Fallback if run standalone or moved
    ROOT = str(APP_DIR.parent.parent)

ICON_SRC     = Path(ROOT) / r"wago\att_icons_export\8K_Format\wow_icons\large"
ICON_WEB_DIR = STATIC_DIR / "icons"
ART_SRC      = Path(ROOT) / r"wago\att_icons_export\8K_Format\scenic_art"
ART_WEB_DIR  = STATIC_DIR / "art"

RUNTIME = os.path.join(ROOT, r"out\build\x64-RelWithDebInfo\bin\RelWithDebInfo")
TOOLS   = os.path.join(ROOT, "tools")
SC_DIR  = os.path.join(TOOLS, "shortcuts")
WAGO    = os.path.join(ROOT, "wago")
EXTTOOLS = os.path.join(ROOT, "ExtTools")
CHROME  = r"C:\Program Files\Google\Chrome\Application\chrome.exe"

ICON_WEB_SIZE = 128  # px

app = Flask(__name__, static_folder=str(STATIC_DIR))

# ═══════════════════════════════════════════════════════════════════
#  ASSET PREPARATION
# ═══════════════════════════════════════════════════════════════════

def prepare_icon(png_name: str) -> str:
    """Ensure a web-sized icon exists in both PNG and WebP, return the web filename."""
    ICON_WEB_DIR.mkdir(parents=True, exist_ok=True)
    out_png = ICON_WEB_DIR / png_name
    out_webp = ICON_WEB_DIR / png_name.replace(".png", ".webp")
    if out_png.exists() and out_webp.exists():
        return png_name
    src = ICON_SRC / png_name
    if not src.exists():
        return png_name
    try:
        img = Image.open(src).convert("RGBA")
        img_full = img.resize((ICON_WEB_SIZE, ICON_WEB_SIZE), Image.LANCZOS)
        img_full.save(out_png, "PNG", optimize=True)
        img_web = img.resize((64, 64), Image.LANCZOS)
        img_web.save(out_webp, "WEBP", quality=85, method=6)
    except Exception as e:
        print(f"  WARN: Failed to convert icon {png_name}: {e}")
    return png_name


def prepare_art():
    """Downscale art assets to web-friendly sizes."""
    ART_WEB_DIR.mkdir(parents=True, exist_ok=True)
    conversions = [
        (ART_SRC / "loading_screens" / "assaultonqueldanas.png", "bg-midnight.jpg", 1920),
        (ART_SRC / "z_expansion_logos" / "Logo_MN.png", "logo-midnight.png", 600),
    ]
    for src, dst_name, max_w in conversions:
        dst = ART_WEB_DIR / dst_name
        if dst.exists():
            continue
        if not src.exists():
            print(f"  WARN: {src} not found, skipping")
            continue
        try:
            img = Image.open(src)
            ratio = max_w / img.width
            new_size = (max_w, int(img.height * ratio))
            img = img.resize(new_size, Image.LANCZOS)
            if dst_name.endswith(".jpg"):
                img = img.convert("RGB")
                img.save(dst, "JPEG", quality=85, optimize=True)
            else:
                img.save(dst, "PNG", optimize=True)
            print(f"  Art: {dst_name} ({new_size[0]}x{new_size[1]})")
        except Exception as e:
            print(f"  WARN: Failed to process art {dst_name}: {e}")


# ═══════════════════════════════════════════════════════════════════
#  SHORTCUT DATA — mirrors create_shortcuts.py
# ═══════════════════════════════════════════════════════════════════

CATEGORIES = [
    {
        "id": "server",
        "name": "Server",
        "icon": "inv_shield_04.png",
        "desc": "Launch and manage the game server stack",
        "shortcuts": [
            {"name": "Play (Start All)", "icon": "inv_hearthstone_gold.png",
             "desc": "ONE CLICK TO PLAY — starts UniServerZ MySQL, bnetserver, worldserver, and Arctium Game Launcher in sequence.",
             "cmd": ["cmd.exe", "/k", f"{SC_DIR}\\start_all.bat"], "cwd": RUNTIME},
            {"name": "Stop All", "icon": "ability_creature_cursed_02.png",
             "desc": "Clean shutdown — kills worldserver, bnetserver, and MySQL (UniServerZ + MySQL80 service).",
             "cmd": ["cmd.exe", "/k", f"{SC_DIR}\\stop_all.bat"], "cwd": RUNTIME},
            {"name": "Worldserver (solo)", "icon": "inv_misc_head_dragon_blue.png",
             "desc": "Launch ONLY worldserver.exe (RelWithDebInfo). Assumes MySQL and bnetserver are already running. Console stays open for server commands.",
             "cmd": ["cmd.exe", "/k", "worldserver.exe"], "cwd": RUNTIME},
            {"name": "Bnetserver (solo)", "icon": "spell_nature_lightning.png",
             "desc": "Launch ONLY bnetserver.exe. Handles account auth and realm list. Must be running before worldserver.",
             "cmd": ["cmd.exe", "/k", "bnetserver.exe"], "cwd": RUNTIME},
            {"name": "Start MySQL", "icon": "inv_datacrystal06.png",
             "desc": "Start UniServerZ MySQL 9.5.0 with game databases (world, auth, characters, hotfixes, roleplay). No admin needed. Listens on port 3306.",
             "cmd": ["cmd.exe", "/k", f"{SC_DIR}\\start_mysql_uniserverz.bat"], "cwd": RUNTIME},
        ]
    },
    {
        "id": "build",
        "name": "Build",
        "icon": "inv_blacksmith_anvil.png",
        "desc": "Compile and configure the server",
        "shortcuts": [
            {"name": "1. Configure CMake", "icon": "inv_misc_gear_01.png",
             "desc": "STEP 1 — Run after adding new source files or changing CMake options. Configures both x64-Debug and x64-RelWithDebInfo presets.",
             "cmd": ["cmd.exe", "/k", f"{TOOLS}\\build\\configure.bat"], "cwd": ROOT},
            {"name": "2. Build Release", "icon": "inv_hammer_03.png",
             "desc": "STEP 2 — Full x64-RelWithDebInfo build (primary runtime, ~17s startup). This is the build you play with.",
             "cmd": ["cmd.exe", "/k", f"{TOOLS}\\build\\build.bat"], "cwd": ROOT},
            {"name": "Build Debug", "icon": "inv_hammer_05.png",
             "desc": "Full x64-Debug build (~60s startup). Use for breakpoints and step-through debugging in VS 2026.",
             "cmd": ["cmd.exe", "/k", f"{TOOLS}\\build\\build_debug.bat"], "cwd": ROOT},
            {"name": "Build Scripts Only", "icon": "inv_misc_scrollrolled01.png",
             "desc": "FAST ITERATION — builds only the 'scripts' ninja target in RelWithDebInfo. Much faster than full build when you only changed Custom/ scripts.",
             "cmd": ["cmd.exe", "/k", f"{SC_DIR}\\build_scripts_rel.bat"], "cwd": ROOT},
        ]
    },
    {
        "id": "pipeline",
        "name": "Pipeline",
        "icon": "inv_misc_treasurechest04b.png",
        "desc": "Data pipeline — run these in order after a build bump",
        "shortcuts": [
            {"name": "1. DB Snapshot", "icon": "inv_misc_book_01.png", "step": 1,
             "desc": "ALWAYS DO THIS FIRST. Creates a mysqldump backup of all 5 databases with a timestamped label. Use db_snapshot.py rollback to undo.",
             "cmd": ["cmd.exe", "/k", "python db_snapshot.py snapshot --label pre-pipeline"], "cwd": WAGO},
            {"name": "2. TACT Extract", "icon": "inv_datacrystal01.png", "step": 2,
             "desc": "Extracts raw DB2 files from local WoW CASC install → converts to CSV via DBC2CSV. --verify compares row counts. Takes ~50s.",
             "cmd": ["cmd.exe", "/k", "python tact_extract.py --verify"], "cwd": WAGO},
            {"name": "3. Merge CSV Sources", "icon": "inv_datacrystal03.png", "step": 3,
             "desc": "Combines TACT CSVs (client ground truth) with Wago CSVs (CDN hotfix content). Output: merged_csv/{build}/enUS/ — single source of truth.",
             "cmd": ["cmd.exe", "/k", "python merge_csv_sources.py"], "cwd": WAGO},
            {"name": "4. Hotfix Repair", "icon": "ability_repair.png", "step": 4,
             "desc": "Compares hotfix DB tables against merged CSVs and fixes mismatches. Run --batch 1 through 5 (change the number). ~71 MB SQL total.",
             "cmd": ["cmd.exe", "/k", "python repair_hotfix_tables.py --batch 1"], "cwd": WAGO},
            {"name": "5. Raidbots Import", "icon": "inv_misc_coin_02.png", "step": 5,
             "desc": "8-step pipeline: quest chains → fix chains → objectives → POI → item locale (6 languages) → fix orphans. Verifies zero quest chain cycles. Use --regenerate after new JSONs.",
             "cmd": ["cmd.exe", "/k", "python raidbots\\run_all_imports.py"], "cwd": WAGO},
            {"name": "6. World Health Check", "icon": "spell_holy_divinepurpose.png", "step": 6,
             "desc": "Validates referential integrity of the world DB after imports. Checks for orphaned references, missing templates, broken loot chains.",
             "cmd": ["cmd.exe", "/k", "python world_health_check.py"], "cwd": WAGO},
            {"name": "7. DB Error Parser", "icon": "ability_creature_cursed_01.png", "step": 7,
             "desc": "Parses DBErrors.log and categorizes ALL error patterns by system with counts and fix descriptions. Run after server startup.",
             "cmd": ["cmd.exe", "/k", "python tools\\parse_dberrors.py"], "cwd": ROOT},
            {"name": "8. Optimize DB", "icon": "ability_rogue_sprint.png", "step": 8,
             "desc": "OPTIONAL — ALTER TABLE FORCE on InnoDB tables >10 MB across all databases. Reclaims space after bulk imports. Reports before/after sizes.",
             "cmd": ["cmd.exe", "/k", f"{TOOLS}\\_optimize_db.bat"], "cwd": TOOLS},
            {"name": "Wago DB2 Download", "icon": "inv_misc_questionmark.png",
             "desc": "ALTERNATIVE to TACT Extract — downloads DB2 CSVs directly from wago.tools website. Slower but doesn't need a local WoW install.",
             "cmd": ["cmd.exe", "/k", "python wago_db2_downloader.py"], "cwd": WAGO},
            {"name": "DB Snapshot List-Rollback", "icon": "inv_misc_book_03.png",
             "desc": "View all database snapshots. To rollback: db_snapshot.py rollback --id N --confirm. To prune: db_snapshot.py prune --keep 5.",
             "cmd": ["cmd.exe", "/k", "python db_snapshot.py list"], "cwd": WAGO},
        ]
    },
    {
        "id": "packets",
        "name": "Packets",
        "icon": "inv_misc_spyglass_02.png",
        "desc": "Capture, parse, and analyze network packets",
        "shortcuts": [
            {"name": "Dev Session", "icon": "inv_hearthstone_gold.png",
             "desc": "THE SMART LAUNCHER — archives previous session, starts bnet+world with EXIT trap, auto-runs WPP on World.pkt when you Ctrl+C, then runs Packet Scope.",
             "cmd": ["cmd.exe", "/k", f"bash {ROOT}/tools-dev/tc-packet-tools/start-worldserver.sh"], "cwd": RUNTIME},
            {"name": "1. YMIR Sniffer", "icon": "ability_hunter_snipershot.png",
             "desc": "Retail packet sniffer for 12.0.1 build 66666. Launch BEFORE opening the game client. Captures live traffic into .pkt files.",
             "cmd": [os.path.join(EXTTOOLS, "ymir_retail_12.0.1.66666", "ymir_retail.exe")],
             "cwd": os.path.join(EXTTOOLS, "ymir_retail_12.0.1.66666")},
            {"name": "2. WowPacketParser", "icon": "inv_misc_spyglass_03.png",
             "desc": "Parses .pkt captures into human-readable text + SQL extracts. Drag-and-drop a .pkt file or use the GUI.",
             "cmd": [os.path.join(EXTTOOLS, "WowPacketParser", "WowPacketParser.exe")],
             "cwd": os.path.join(EXTTOOLS, "WowPacketParser")},
            {"name": "3. Packet Scope", "icon": "ability_rogue_bloodyeye.png",
             "desc": "Analyzes WPP output with transmog-specific decoding. Finds CMSG/SMSG_TRANSMOG packets, decodes outfit data, checks addon messages.",
             "cmd": ["cmd.exe", "/k", "python tools\\packet_scope.py"], "cwd": ROOT},
            {"name": "4. Opcode Analyzer", "icon": "inv_letter_02.png",
             "desc": "Builds opcode dictionary from Opcodes.h/cpp, cross-refs with WPP output. Use --lookup TRANSMOG to search, --top 20 for most-seen.",
             "cmd": ["cmd.exe", "/k", "python tools\\opcode_analyzer.py"], "cwd": ROOT},
        ]
    },
    {
        "id": "audits",
        "name": "Audits",
        "icon": "inv_misc_questionmark.png",
        "desc": "Data quality audits — read-only, safe to run anytime",
        "shortcuts": [
            {"name": "NPC Audit (27 checks)", "icon": "ability_hunter_pet_assist.png",
             "desc": "27 audits: levels, flags, faction, classification, type, duplicates, phases, missing, display, names, scale, speed, equipment, gossip, waypoints, SmartAI, loot, auras, and more.",
             "cmd": ["cmd.exe", "/k", "python npc_audit.py --help"], "cwd": WAGO},
            {"name": "Quest Audit (15 checks)", "icon": "inv_misc_scrollrolled01c.png",
             "desc": "15 audits: broken chains, exclusive groups, missing givers/enders, invalid objectives/rewards, missing quests, orphans, POI, questline cross-ref, addon sync.",
             "cmd": ["cmd.exe", "/k", "python quest_audit.py --help"], "cwd": WAGO},
            {"name": "GO Audit (15 checks)", "icon": "inv_misc_gear_03.png",
             "desc": "15 GameObject audits: duplicates, phases, display, type, scale, loot, quest refs, pools, events, names, SmartAI, spawntime, addon orphans, missing, faction.",
             "cmd": ["cmd.exe", "/k", "python go_audit.py --help"], "cwd": WAGO},
            {"name": "Transmog Validate", "icon": "inv_chest_plate01.png",
             "desc": "Cross-refs wow.tools.local DB2 vs MySQL hotfixes. Missing rows, FK violations, value mismatches, TransmogSet resolution, illusion IDs. Runs in <1s across 155K IMAIDs.",
             "cmd": ["cmd.exe", "/k", "python validate_transmog.py"], "cwd": WAGO},
            {"name": "Transmog Debug", "icon": "inv_chest_plate05.png",
             "desc": "Full transmog state viewer. Use: --char Hexandchill, --imaid 304252, --outfit 7, --diff 7, --log (Debug.log), --spy (addon data).",
             "cmd": ["cmd.exe", "/k", "python transmog_debug.py --help"], "cwd": WAGO},
            {"name": "Transmog Lookup", "icon": "inv_chest_plate03.png",
             "desc": "DB2 cross-reference. Use: imaid <id>, search <name>, dt (DisplayType table), reverse <itemid>, analyze <logfile>, batch <strings>.",
             "cmd": ["cmd.exe", "/k", "python transmog_lookup.py --help"], "cwd": WAGO},
            {"name": "Scraper v3 (Tor Army)", "icon": "inv_misc_web_01.png",
             "desc": "Async swarm scraper — 400 Tor instances, 600K-1M pages/hr. --smoke 50 --targets npc --workers 10 --start-tor for testing.",
             "cmd": ["cmd.exe", "/k", "python scraper_v3.py --help"], "cwd": WAGO},
        ]
    },
    {
        "id": "web",
        "name": "Web",
        "icon": "spell_arcane_portaldalaran.png",
        "desc": "Dashboards, documentation, and web tools",
        "shortcuts": [
            {"name": "VoxCore Docs", "icon": "inv_misc_book_05.png",
             "desc": "VoxCore project documentation — framework, pipeline, tooling, AI workflow, status, and results. Hosted on GitHub Pages.",
             "url": "https://voxcore84.github.io/roleplaycore-report/index.html"},
            {"name": "Task Tracker", "icon": "achievement_guildperk_workingovertime.png",
             "desc": "GitHub issue/PR tracker dashboard for VoxCore84/RoleplayCore. Shows open issues, recent PRs, task status.",
             "cmd": [CHROME, "--app=file:///C:/Users/atayl/cowork/outputs/roleplaycore-tracker.html"]},
            {"name": "Spell Audit Tracker", "icon": "spell_holy_prayerofhealing02.png",
             "desc": "Class/spec spell implementation tracker — 4,965 spells classified GREEN/YELLOW/RED/MISSING across 40 specs. Shows C++ stubs, fix categories, SimC cross-refs.",
             "cmd": [CHROME, "--app=file:///C:/Users/atayl/VoxCore/wago/audit_reports/spell_audit_report.html"]},
            {"name": "ATT Browser", "icon": "achievement_guildperk_mobilebanking.png",
             "desc": "AllTheThings database browser — Flask web UI backed by SQLite. Tree navigation, search, detail views. Opens on localhost:5051.",
             "cmd": ["cmd.exe", "/k", "python app.py --port 5051"],
             "cwd": os.path.join(WAGO, "att_browser")},
            {"name": "WebTerm", "icon": "trade_engineering.png",
             "desc": "Web-based terminal on localhost:7681. Opens in Chrome app mode. Cowork can access this to run commands remotely.",
             "cmd": ["cmd.exe", "/k", os.path.join(EXTTOOLS, "launch-webterm.bat")], "cwd": EXTTOOLS},
            {"name": "Update Website", "icon": "inv_letter_01.png",
             "desc": "Rebuild VoxCore docs site from local sources, then git add + commit + push. Deploys to GitHub Pages in ~30 seconds.",
             "cmd": ["cmd.exe", "/k", f"{ROOT}\\website\\update_site.bat"],
             "cwd": os.path.join(ROOT, "website")},
            {"name": "AI Toolkit Reference", "icon": "inv_misc_book_03.png",
             "desc": "Claude AI toolkit reference — all MCP servers, slash commands, skills, and automation used in this project.",
             "cmd": [CHROME, "--app=file:///C:/Users/atayl/VoxCore/doc/AI-Toolkit-Reference.html"]},
            {"name": "GitHub Repo", "icon": "achievement_guildperk_ladyluck.png",
             "desc": "VoxCore84/RoleplayCore — main GitHub repository. Issues, PRs, commits, CI.",
             "url": "https://github.com/VoxCore84/RoleplayCore"},
            {"name": "Discord (Dev Channel)", "icon": "ui_chat.png",
             "desc": "VoxCore development Discord channel — chat, coordination, and announcements.",
             "url": "https://discord.com/channels/1231835263861395487/1231843017732788265"},
            {"name": "Wago.tools", "icon": "inv_enchant_formulagood_01.png",
             "desc": "Wago.tools DB2 browser — online reference for all 1,097 WoW DB2 tables. Column lookups, data verification, hotfix comparisons.",
             "url": "https://wago.tools/db2"},
        ]
    },
    {
        "id": "tools",
        "name": "Tools",
        "icon": "inv_misc_wrench_01.png",
        "desc": "Utilities and quick access",
        "shortcuts": [
            {"name": "wow.tools.local", "icon": "inv_misc_spyglass_01.png",
             "desc": "Local DB2 data browser at http://localhost:5000 (build 66666). Browse all 1,097 DB2 tables. Waits for startup then opens browser.",
             "cmd": ["cmd.exe", "/k", f"{EXTTOOLS}\\WoW.tools\\start_wtl.bat"],
             "cwd": os.path.join(EXTTOOLS, "WoW.tools")},
            {"name": "DBC2CSV", "icon": "inv_datacrystal06.png",
             "desc": "GUI tool to convert raw .db2/.dbc files to CSV. Drag-and-drop or browse. Used by TACT Extract internally.",
             "cmd": [os.path.join(EXTTOOLS, "DBC2CSV", "DBC2CSV.exe")],
             "cwd": os.path.join(EXTTOOLS, "DBC2CSV")},
            {"name": "Spell Creator", "icon": "inv_wand_07.png",
             "desc": "Interactive spell creation CLI — templates, cloning, CSV lookup, hotfix SQL generation, SOAP reload. Replaces old .NET SpellCreator.",
             "cmd": ["cmd.exe", "/k", "python tools\\spell_creator.py"],
             "cwd": ROOT},
            {"name": "Restart Cowork", "icon": "inv_misc_enggizmos_03.png",
             "desc": "Force-restart Cowork (Claude Desktop) — kills processes, restarts service, clears temp. Needs admin.",
             "cmd": ["cmd.exe", "/k", f"{EXTTOOLS}\\restart-cowork.bat"], "cwd": ROOT},
            {"name": "Server Logs", "icon": "inv_misc_note_01.png",
             "desc": "Open server runtime directory — Server.log, DBErrors.log, Debug.log, GM.log, worldserver.conf, PacketLog/.",
             "cmd": ["explorer.exe", RUNTIME]},
            {"name": "Project Folder", "icon": "inv_misc_head_dragon_bronze.png",
             "desc": "Open VoxCore project root in Explorer.",
             "cmd": ["explorer.exe", ROOT]},
            {"name": "CC Sync Report", "icon": "ability_monk_roll.png",
             "desc": "Compare desktop VC folder shortcuts against Command Center cards. Shows missing, extra, and matched items.",
             "cmd": ["cmd.exe", "/k", "python command-center\\sync_from_desktop.py"], "cwd": TOOLS},
        ]
    },
]


# ═══════════════════════════════════════════════════════════════════
#  FLASK ROUTES
# ═══════════════════════════════════════════════════════════════════

@app.route("/")
def index():
    return render_template("index.html", categories=CATEGORIES)


@app.route("/sw.js")
def service_worker():
    return send_from_directory(str(STATIC_DIR), "sw.js", mimetype="application/javascript")


@app.route("/manifest.json")
def manifest():
    return send_from_directory(str(STATIC_DIR), "manifest.json", mimetype="application/manifest+json")


@app.route("/favicon.ico")
def favicon():
    return send_from_directory(str(STATIC_DIR), "favicon.ico", mimetype="image/x-icon")


@app.route("/api/launch", methods=["POST"])
def launch():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"ok": False, "error": "Invalid request"}), 400
    cat_id = data.get("category")
    item_idx = data.get("index")

    cat = next((c for c in CATEGORIES if c["id"] == cat_id), None)
    if not cat or item_idx is None or item_idx < 0 or item_idx >= len(cat["shortcuts"]):
        return jsonify({"ok": False, "error": "Invalid shortcut"}), 400

    item = cat["shortcuts"][item_idx]
    if "url" in item:
        return jsonify({"ok": False, "error": "URL shortcut — open in browser"}), 400
    cmd = item.get("cmd", [])
    cwd = item.get("cwd")

    try:
        subprocess.Popen(
            cmd,
            cwd=cwd,
            creationflags=subprocess.CREATE_NEW_CONSOLE,
            close_fds=True,
        )
        return jsonify({"ok": True, "name": item["name"]})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500


@app.route("/api/status")
def status():
    """Check if key services are running."""
    def is_running(name):
        try:
            r = subprocess.run(
                ["tasklist", "/FI", f"IMAGENAME eq {name}"],
                capture_output=True, text=True, timeout=5
            )
            return name.lower() in r.stdout.lower()
        except Exception:
            return False

    def mysql_running():
        try:
            # Check UniServerZ mysqld_z.exe first (primary), then MySQL80 service
            r = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq mysqld_z.exe"],
                capture_output=True, text=True, timeout=5
            )
            if "mysqld_z.exe" in r.stdout.lower():
                return True
            r2 = subprocess.run(
                ["sc", "query", "MySQL80"],
                capture_output=True, text=True, timeout=5
            )
            return "RUNNING" in r2.stdout
        except Exception:
            return False

    return jsonify({
        "mysql": mysql_running(),
        "worldserver": is_running("worldserver.exe"),
        "bnetserver": is_running("bnetserver.exe"),
    })


# ═══════════════════════════════════════════════════════════════════
#  STARTUP
# ═══════════════════════════════════════════════════════════════════

def prepare_all_assets():
    """Pre-generate web-sized icons and art. Skips if cache is warm."""
    all_icons = set()
    for cat in CATEGORIES:
        all_icons.add(cat["icon"])
        for item in cat["shortcuts"]:
            all_icons.add(item["icon"])

    # Fast check: if all WebP icons exist, skip everything
    ICON_WEB_DIR.mkdir(parents=True, exist_ok=True)
    missing = [i for i in all_icons if not (ICON_WEB_DIR / i.replace(".png", ".webp")).exists()]
    if not missing and (ART_WEB_DIR / "bg-midnight.webp").exists():
        print("  Assets cached — skipping prep.")
        return

    print(f"Preparing assets ({len(missing)} icons to convert)...")
    prepare_art()
    for icon in missing:
        prepare_icon(icon)
    print("  Assets ready.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=5050)
    parser.add_argument("--prep", action="store_true", help="Only prepare assets")
    args = parser.parse_args()

    prepare_all_assets()

    if args.prep:
        print("Asset preparation complete.")
        sys.exit(0)

    print(f"\n  VoxCore Command Center — http://localhost:{args.port}")
    print(f"  Press Ctrl+C to stop.\n")
    app.run(host="127.0.0.1", port=args.port, debug=False)
