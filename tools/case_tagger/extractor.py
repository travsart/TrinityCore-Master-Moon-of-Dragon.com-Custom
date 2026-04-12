"""
Deterministic metadata extractor for Case_Reference files.

Extracts: date, doc_type, people, tags, description, filing_relevance, evidence_strength.
All extraction is regex/heuristic — no AI calls.
"""

import re
from dataclasses import dataclass, field
from pathlib import Path

import yaml


@dataclass
class ExtractedMetadata:
    date: str = ""
    doc_type: str = ""
    people: list[str] = field(default_factory=list)
    tags: list[str] = field(default_factory=list)
    description: str = ""
    filing_relevance: list[str] = field(default_factory=list)
    evidence_strength: str = "reference_only"


def load_vocabulary(config_path: Path) -> dict:
    """Load controlled vocabulary from YAML config."""
    with open(config_path, encoding="utf-8") as f:
        return yaml.safe_load(f)


# ---------------------------------------------------------------------------
# Date extraction
# ---------------------------------------------------------------------------

_DATE_PATTERNS = [
    # ISO-like in filename or content: 2026-03-14, 2026_03_14, 2026.03.14
    re.compile(r"(20[12]\d)[-_./](0[1-9]|1[0-2])[-_./](0[1-9]|[12]\d|3[01])"),
    # US format: March 14, 2026 / Mar 14, 2026
    re.compile(
        r"(Jan(?:uary)?|Feb(?:ruary)?|Mar(?:ch)?|Apr(?:il)?|May|Jun(?:e)?|"
        r"Jul(?:y)?|Aug(?:ust)?|Sep(?:tember)?|Oct(?:ober)?|Nov(?:ember)?|Dec(?:ember)?)"
        r"\s+(0?[1-9]|[12]\d|3[01]),?\s+(20[12]\d)",
        re.IGNORECASE,
    ),
    # DD Mon YYYY: 14 March 2026
    re.compile(
        r"(0?[1-9]|[12]\d|3[01])\s+"
        r"(Jan(?:uary)?|Feb(?:ruary)?|Mar(?:ch)?|Apr(?:il)?|May|Jun(?:e)?|"
        r"Jul(?:y)?|Aug(?:ust)?|Sep(?:tember)?|Oct(?:ober)?|Nov(?:ember)?|Dec(?:ember)?)"
        r",?\s+(20[12]\d)",
        re.IGNORECASE,
    ),
]

_MONTH_MAP = {
    "jan": "01", "january": "01", "feb": "02", "february": "02",
    "mar": "03", "march": "03", "apr": "04", "april": "04",
    "may": "05", "jun": "06", "june": "06", "jul": "07", "july": "07",
    "aug": "08", "august": "08", "sep": "09", "september": "09",
    "oct": "10", "october": "10", "nov": "11", "november": "11",
    "dec": "12", "december": "12",
}


def extract_date(filename: str, content: str) -> str:
    """Extract date from filename first, then content. Returns YYYY-MM-DD or ''."""
    # Priority 1: filename
    m = _DATE_PATTERNS[0].search(filename)
    if m:
        return f"{m.group(1)}-{m.group(2)}-{m.group(3)}"

    # Priority 2: first 30 lines of content (header area)
    header = "\n".join(content.split("\n")[:30])

    # ISO pattern
    m = _DATE_PATTERNS[0].search(header)
    if m:
        return f"{m.group(1)}-{m.group(2)}-{m.group(3)}"

    # Month Day, Year
    m = _DATE_PATTERNS[1].search(header)
    if m:
        month = _MONTH_MAP.get(m.group(1).lower()[:3], "")
        if month:
            return f"{m.group(3)}-{month}-{int(m.group(2)):02d}"

    # Day Month Year
    m = _DATE_PATTERNS[2].search(header)
    if m:
        month = _MONTH_MAP.get(m.group(2).lower()[:3], "")
        if month:
            return f"{m.group(3)}-{month}-{int(m.group(1)):02d}"

    # Priority 3: anywhere in content (less confident, still ISO only)
    m = _DATE_PATTERNS[0].search(content)
    if m:
        return f"{m.group(1)}-{m.group(2)}-{m.group(3)}"

    return ""


# ---------------------------------------------------------------------------
# Doc type classification
# ---------------------------------------------------------------------------

def extract_doc_type(filename: str, content: str, vocab: dict) -> str:
    """Classify document type using weighted keyword matching."""
    doc_types = vocab.get("doc_types", {})
    scores: dict[str, float] = {}

    # Check filename + first 50 lines for higher confidence
    header = "\n".join(content.split("\n")[:50])
    combined = f"{filename}\n{header}".lower()
    body_lower = content.lower()

    # Special handling for email: require at least 3 of the email headers
    # in the first 30 lines to classify as email (prevents false positives)
    email_headers = ["from:", "to:", "subject:", "sent:", "date:", "re:", "fw:"]
    first_30 = "\n".join(content.split("\n")[:30]).lower()
    email_header_count = sum(1 for h in email_headers if h in first_30)
    if email_header_count < 3:
        # Not enough email headers — suppress email classification
        doc_types = {k: v for k, v in doc_types.items() if k != "email"}

    for dtype, config in doc_types.items():
        weight = config.get("weight", 5)
        keywords = config.get("keywords", [])
        score = 0
        for kw in keywords:
            kw_lower = kw.lower()
            if kw_lower in combined:
                score += weight * 2  # header/filename match = 2x
            elif kw_lower in body_lower:
                score += weight
        if score > 0:
            scores[dtype] = score

    if not scores:
        return "report"  # safe default

    return max(scores, key=scores.get)


# ---------------------------------------------------------------------------
# People extraction
# ---------------------------------------------------------------------------

# Military rank patterns
_RANK_PATTERN = re.compile(
    r"\b((?:Capt|Captain|CPT|Col|Colonel|COL|Lt Col|LTC|Maj|Major|MAJ|"
    r"1st Lt|2d Lt|2nd Lt|1LT|2LT|Gen|BGen|MGen|LtGen|"
    r"CMSgt|SMSgt|MSgt|TSgt|SSgt|SrA|A1C|Amn|AB|"
    r"LCDR|CDR|CAPT|ADM|RADM|VADM|"
    r"SFC|SGM|CSM|SGT|SSG|SPC|PFC|PVT|"
    r"Dr|Mr|Ms|Mrs|Miss)\.?\s+"
    r"[A-Z][a-z]+(?:\s+[A-Z]\.?)?\s+[A-Z][a-z]+)\b"
)

# False positive names to exclude (medical terms matching rank patterns)
_NAME_BLOCKLIST = {
    "major depressive disorder", "major depression", "general anxiety",
    "general disorder", "general counsel", "general officer",
    "colonel mustard",  # just in case
}

# Simple "First Last" or "First M. Last" near case-relevant context
_NAME_PATTERN = re.compile(
    r"\b([A-Z][a-z]{2,15}\s+(?:[A-Z]\.?\s+)?[A-Z][a-z]{2,20})\b"
)


def extract_people(content: str, vocab: dict) -> list[str]:
    """Extract people mentioned in the document."""
    found: set[str] = set()
    known = vocab.get("known_people", {})

    # Check known people first (high confidence)
    content_lower = content.lower()
    for canonical, aliases in known.items():
        for alias in aliases:
            if alias.lower() in content_lower:
                found.add(canonical)
                break

    # Rank + name pattern (moderate confidence)
    for m in _RANK_PATTERN.finditer(content):
        name = m.group(1).strip()
        # Skip false positives (medical terms)
        if name.lower() in _NAME_BLOCKLIST:
            continue
        found.add(name)

    # Filter to deduplicate — if "Capt Adam Taylor" and "Adam J. Taylor" both found,
    # keep the known_people canonical form
    normalized = set()
    for name in found:
        # Check if this is a substring of a known person
        is_alias = False
        for canonical, aliases in known.items():
            if canonical in found:
                for alias in aliases:
                    if name.lower() == alias.lower() and name != canonical:
                        is_alias = True
                        break
            if is_alias:
                break
        if not is_alias:
            normalized.add(name)

    return sorted(normalized)


# ---------------------------------------------------------------------------
# Tag extraction
# ---------------------------------------------------------------------------

def extract_tags(content: str, vocab: dict) -> list[str]:
    """Extract tags from controlled vocabulary matches."""
    tags_config = vocab.get("tags", {})
    content_lower = content.lower()
    matched: list[str] = []

    for tag, keywords in tags_config.items():
        for kw in keywords:
            if kw.lower() in content_lower:
                matched.append(tag)
                break  # one match per tag is enough

    return sorted(matched)


# ---------------------------------------------------------------------------
# Filing relevance
# ---------------------------------------------------------------------------

def extract_filing_relevance(content: str, vocab: dict) -> list[str]:
    """Determine which legal filings this document is relevant to."""
    filings_config = vocab.get("filings", {})
    content_lower = content.lower()
    matched: list[str] = []

    for filing, keywords in filings_config.items():
        for kw in keywords:
            if kw.lower() in content_lower:
                matched.append(filing)
                break

    return sorted(matched)


# ---------------------------------------------------------------------------
# Evidence strength
# ---------------------------------------------------------------------------

_STRONG_SIGNALS = [
    re.compile(r"\b(signed|official|order|directive|formal complaint|sworn)\b", re.I),
    re.compile(r"\b(memorandum for record|MFR|notification of|suspension notice)\b", re.I),
    re.compile(r"\b(DD Form|SF-\d+|AF Form)\b", re.I),
]

_MODERATE_SIGNALS = [
    re.compile(r"\b(email|correspondence|confirms|corroborates|consistent with)\b", re.I),
    re.compile(r"\b(timeline|chronology|sequence of events)\b", re.I),
]

_WEAK_SIGNALS = [
    re.compile(r"\b(analysis|assessment|opinion|interpretation|suggests)\b", re.I),
]

_REFERENCE_SIGNALS = [
    re.compile(r"\b(regulation|instruction|policy|CFR|USC|DoDI|AFI|DHA-PM)\b", re.I),
    re.compile(r"\b(checklist|template|guide|manual|handbook)\b", re.I),
]


def extract_evidence_strength(content: str, doc_type: str) -> str:
    """Conservatively assess evidence strength."""
    # Regulations are always reference_only
    if doc_type == "regulation":
        return "reference_only"
    if doc_type in ("checklist", "index"):
        return "reference_only"

    header = content[:3000]  # Focus on beginning

    strong = sum(1 for p in _STRONG_SIGNALS if p.search(header))
    moderate = sum(1 for p in _MODERATE_SIGNALS if p.search(header))
    weak = sum(1 for p in _WEAK_SIGNALS if p.search(header))
    reference = sum(1 for p in _REFERENCE_SIGNALS if p.search(header))

    if strong >= 2:
        return "strong"
    if strong >= 1 and moderate >= 1:
        return "moderate"
    if moderate >= 2:
        return "moderate"
    if weak >= 1 or moderate >= 1:
        return "weak"
    if reference >= 1:
        return "reference_only"

    return "reference_only"


# ---------------------------------------------------------------------------
# Description
# ---------------------------------------------------------------------------

def extract_description(filename: str, content: str, doc_type: str) -> str:
    """Generate a one-line description (max 200 chars)."""
    stem = Path(filename).stem

    # Try to find a title or subject line
    title = ""

    # Check for markdown title
    m = re.search(r"^#\s+(.+)$", content, re.MULTILINE)
    if m:
        title = m.group(1).strip()

    # Check for Subject: line (emails)
    if not title:
        m = re.search(r"^Subject:\s*(.+)$", content, re.MULTILINE)
        if m:
            title = m.group(1).strip()

    # Fall back to cleaned filename
    if not title:
        title = stem.replace("_", " ").replace("-", " ").strip()

    # Format: doc_type: title
    desc = f"{doc_type}: {title}"

    # Collapse whitespace and truncate
    desc = re.sub(r"\s+", " ", desc).strip()
    if len(desc) > 200:
        desc = desc[:197] + "..."

    return desc


# ---------------------------------------------------------------------------
# Main extraction entry point
# ---------------------------------------------------------------------------

def extract_metadata(filepath: Path, content: str, vocab: dict) -> ExtractedMetadata:
    """Run all extractors on a single file. Returns ExtractedMetadata."""
    filename = filepath.name
    meta = ExtractedMetadata()

    meta.date = extract_date(filename, content)
    meta.doc_type = extract_doc_type(filename, content, vocab)
    meta.people = extract_people(content, vocab)
    meta.tags = extract_tags(content, vocab)
    meta.filing_relevance = extract_filing_relevance(content, vocab)
    meta.evidence_strength = extract_evidence_strength(content, meta.doc_type)
    meta.description = extract_description(filename, content, meta.doc_type)

    return meta
