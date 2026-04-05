"""Tests for VoxSniffer CombatAudit report generation."""

import sys
from pathlib import Path

import pytest

# Ensure project root is on path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from voxsniffer.parsers.savedvariables_loader import SavedVariablesLoader
from voxsniffer.audit_report import (
    extract_audit_records,
    group_by_obs_type,
    generate_report,
    render_proc_timeouts,
    render_spell_stats,
    render_session_metadata,
    render_conclusion,
)

FIXTURE_PATH = Path(__file__).parent / "fixtures" / "combat_audit_sample.lua"


@pytest.fixture
def loader():
    """Load the sample SavedVariables fixture."""
    sv = SavedVariablesLoader(FIXTURE_PATH)
    sv.load()
    return sv


def test_fixture_loads(loader):
    """Fixture file parses successfully."""
    assert loader.db is not None
    assert loader.db.get("schema_version") == 1


def test_extract_audit_records(loader):
    """CombatAudit records are extracted from the fixture."""
    records = extract_audit_records(loader)
    assert len(records) == 10  # 10 records in the fixture chunk
    # All should have 't' field
    for rec in records:
        assert "t" in rec


def test_group_by_obs_type(loader):
    """Records group correctly by observation type."""
    records = extract_audit_records(loader)
    groups = group_by_obs_type(records)

    assert "talent_snapshot" in groups
    assert "player_combat" in groups
    assert "proc_fulfilled" in groups
    assert "proc_timeout" in groups
    assert "spell_stats" in groups
    assert "aura_timeline" in groups
    assert "audit_summary" in groups

    assert len(groups["talent_snapshot"]) == 1
    assert len(groups["player_combat"]) == 2
    assert len(groups["proc_timeout"]) == 2
    assert len(groups["proc_fulfilled"]) == 1
    assert len(groups["spell_stats"]) == 2
    assert len(groups["aura_timeline"]) == 1
    assert len(groups["audit_summary"]) == 1


def test_render_proc_timeouts_with_data(loader):
    """Proc timeout section renders with proper table structure."""
    records = extract_audit_records(loader)
    groups = group_by_obs_type(records)
    result = render_proc_timeouts(groups["proc_timeout"])

    assert "## Proc Timeouts" in result
    assert "Likely Broken" in result
    assert "348" in result  # Immolate trigger
    assert "108685" in result  # expected proc
    assert "172" in result  # Corruption trigger
    assert "17941" in result  # Shadow Trance
    assert "MEDIUM" in result  # confidence


def test_render_proc_timeouts_empty():
    """Proc timeout section renders cleanly with no data."""
    result = render_proc_timeouts([])
    assert "No proc timeouts detected" in result


def test_render_spell_stats_with_data(loader):
    """Spell stats section renders table with damage/healing columns."""
    records = extract_audit_records(loader)
    groups = group_by_obs_type(records)
    result = render_spell_stats(groups["spell_stats"])

    assert "## Spell Statistics" in result
    assert "Chaos Bolt" in result
    assert "Conflagrate" in result
    assert "245,000" in result  # Chaos Bolt total damage
    assert "85,000" in result  # Conflagrate total damage


def test_render_session_metadata(loader):
    """Session metadata renders class, spec, duration, procs info."""
    records = extract_audit_records(loader)
    groups = group_by_obs_type(records)
    summary = groups["audit_summary"][-1]
    snapshot = groups["talent_snapshot"][-1]

    result = render_session_metadata(summary, snapshot, len(records))

    assert "WARLOCK" in result
    assert "Destruction" in result
    assert "Testlock" in result
    assert "1:30" in result
    assert "4500" in result  # spell power


def test_render_conclusion_with_timeouts(loader):
    """Conclusion section categorizes timeouts correctly."""
    records = extract_audit_records(loader)
    groups = group_by_obs_type(records)

    result = render_conclusion(
        groups["proc_timeout"],
        groups["proc_fulfilled"],
        groups["spell_stats"],
        groups.get("talent_anomaly", []),
    )

    assert "## Conclusion" in result
    assert "Needs More Samples" in result  # both timeouts are medium confidence


def test_render_conclusion_all_fulfilled():
    """Conclusion with no timeouts shows 'No Issue Observed'."""
    fulfilled = [{"p": {"triggerSpellId": 1, "expectedSpellId": 2}}]
    result = render_conclusion([], fulfilled, [], [])
    assert "No Issue Observed" in result


def test_render_conclusion_no_data():
    """Conclusion with no data shows 'Insufficient Data'."""
    result = render_conclusion([], [], [], [])
    assert "Insufficient Data" in result


def test_full_report_generation(loader):
    """Full report generates valid Markdown with all sections."""
    report = generate_report(loader)

    assert "# VoxSniffer Combat Audit Report" in report
    assert "## Session Metadata" in report
    assert "## Proc Timeouts" in report
    assert "## Spell Statistics" in report
    assert "## Conclusion" in report
    # Should have content
    assert len(report) > 500


def test_full_report_no_records():
    """Report with no CombatAudit data returns a minimal message."""
    # Create a loader with empty data
    loader = SavedVariablesLoader(FIXTURE_PATH)
    loader.load()
    # Override to return empty
    loader.db["chunks"] = {}

    report = generate_report(loader)
    assert "No CombatAudit records found" in report
