"""VoxCore redaction recovery diagnostic + extraction package.

Non-destructive forensic analysis of PDFs with visible redactions to
determine whether the underlying text is recoverable.

Modules:
    diagnose  — first-pass diagnostic: metadata, text layer, OCGs, byte residue
    extract   — recover surviving text from pages with intact text layers
"""

__all__ = ["diagnose", "extract"]
