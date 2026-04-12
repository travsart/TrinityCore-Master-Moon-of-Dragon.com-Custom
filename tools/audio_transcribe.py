#!/usr/bin/env python3
"""Audio transcription via local Whisper (openai-whisper).

Transcribes .m4a / .mp3 / .wav / .ogg / .flac files to sidecar .txt files.
Cached by (path, mtime, size) so repeat runs skip unchanged files.

Requirements:
    pip install openai-whisper
    ffmpeg in PATH (used by whisper to decode audio)

GPU note for RTX 5090 (Blackwell, CC 12.0):
    Stable PyTorch ships with CUDA 12.4 which does not support Blackwell.
    For GPU acceleration install PyTorch nightly with CUDA 12.8+:
        pip install --pre torch torchvision torchaudio --index-url https://download.pytorch.org/whl/nightly/cu128
    CPU inference still works - medium model is about 1x realtime on a
    modern desktop, so a 30-minute recording takes about 30 minutes.

Usage:
    # Single file
    python tools/audio_transcribe.py path/to/recording.m4a

    # Directory (recursive by default)
    python tools/audio_transcribe.py "C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference"

    # Custom model and language
    python tools/audio_transcribe.py <dir> --model medium --language en

    # Stats only - show what would be transcribed
    python tools/audio_transcribe.py <dir> --stats

    # Custom output dir (default: .cache/transcribed/<slug>_<hash>/)
    python tools/audio_transcribe.py <dir> --out ./my_transcripts/

Output per file:
    <out>/files/<relpath>.txt      # plain text transcript
    <out>/files/<relpath>.json     # full whisper segments with timestamps
    <out>/manifest.json            # cache state
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

DEFAULT_CACHE_ROOT = Path(__file__).resolve().parents[1] / ".cache" / "transcribed"
DEFAULT_EXTS = {".m4a", ".mp3", ".wav", ".ogg", ".flac", ".aac", ".wma", ".opus"}


def cache_dir_for(src: Path, custom: Path | None) -> Path:
    if custom:
        return custom
    src_abs = str(src.resolve()).replace("\\", "/")
    h = hashlib.sha1(src_abs.encode("utf-8")).hexdigest()[:10]
    slug = src.name.replace(" ", "_") or "root"
    return DEFAULT_CACHE_ROOT / f"{slug}_{h}"


def load_manifest(cache_dir: Path) -> dict:
    mf = cache_dir / "manifest.json"
    if not mf.exists():
        return {"source": "", "model": "", "entries": {}}
    try:
        return json.loads(mf.read_text(encoding="utf-8"))
    except Exception:
        return {"source": "", "model": "", "entries": {}}


def save_manifest(cache_dir: Path, manifest: dict) -> None:
    cache_dir.mkdir(parents=True, exist_ok=True)
    (cache_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def scan_source(src: Path, include_exts: set[str]) -> dict[str, dict]:
    if src.is_file():
        ext = src.suffix.lower()
        if ext in include_exts:
            st = src.stat()
            return {src.name: {"mtime": int(st.st_mtime), "size": st.st_size}}
        return {}
    out = {}
    for dirpath, _dirs, filenames in os.walk(src):
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext not in include_exts:
                continue
            abs_path = os.path.join(dirpath, fn)
            try:
                st = os.stat(abs_path)
            except OSError:
                continue
            rel = os.path.relpath(abs_path, src).replace("\\", "/")
            out[rel] = {"mtime": int(st.st_mtime), "size": st.st_size}
    return out


def transcribe_one(model, src_root: Path, rel: str, files_dir: Path, language: str | None) -> tuple[bool, str, float]:
    """Transcribe a single file. Returns (ok, message, duration_sec)."""
    abs_src = src_root / rel if src_root.is_dir() else src_root
    t0 = time.perf_counter()
    try:
        # fp16 only works on GPU; on CPU whisper auto-disables it
        result = model.transcribe(
            str(abs_src),
            language=language,
            fp16=False,
            verbose=False,
        )
    except Exception as e:
        return False, f"whisper error: {e}", time.perf_counter() - t0

    text = result.get("text", "").strip()
    segments = result.get("segments", [])

    out_txt = files_dir / (rel + ".txt")
    out_json = files_dir / (rel + ".json")
    out_txt.parent.mkdir(parents=True, exist_ok=True)

    header = (
        f"Source: {rel}\n"
        f"Language: {result.get('language', 'unknown')}\n"
        f"Segments: {len(segments)}\n"
        f"Duration-sec: {segments[-1].get('end', 0) if segments else 0:.1f}\n\n"
    )
    out_txt.write_text(header + text, encoding="utf-8", newline="\n")

    out_json.write_text(
        json.dumps(
            {
                "source": rel,
                "language": result.get("language"),
                "text": text,
                "segments": [
                    {"start": s.get("start"), "end": s.get("end"), "text": s.get("text")}
                    for s in segments
                ],
            },
            indent=2,
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    return True, "ok", time.perf_counter() - t0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="Audio file or directory")
    parser.add_argument("--out", help="Cache/output directory (default: .cache/transcribed/<slug>/)")
    parser.add_argument("--model", default="base",
                        help="Whisper model: tiny | base | small | medium | large-v3 (default: base)")
    parser.add_argument("--language", default=None,
                        help="Language hint, e.g. 'en' (default: auto-detect per file)")
    parser.add_argument("--types", default=",".join(sorted(DEFAULT_EXTS)),
                        help="Comma-separated audio extensions")
    parser.add_argument("--stats", action="store_true", help="Show cache stats, no transcription")
    parser.add_argument("--force", action="store_true", help="Ignore cache, re-transcribe everything")
    args = parser.parse_args()

    src = Path(args.source).resolve()
    if not src.exists():
        print(f"ERROR: {src} does not exist", file=sys.stderr)
        return 1

    cache_dir = cache_dir_for(src, Path(args.out).resolve() if args.out else None)
    files_dir = cache_dir / "files"
    files_dir.mkdir(parents=True, exist_ok=True)

    include_exts = {e.strip().lower() for e in args.types.split(",") if e.strip()}

    print(f"Source: {src}", file=sys.stderr)
    print(f"Cache:  {cache_dir}", file=sys.stderr)
    print(f"Model:  {args.model}", file=sys.stderr)

    manifest = load_manifest(cache_dir)
    manifest["source"] = str(src)
    entries = manifest.get("entries", {})

    current = scan_source(src, include_exts)

    # If model changed, invalidate cache
    prev_model = manifest.get("model", "")
    model_changed = prev_model and prev_model != args.model

    new_files = []
    changed = []
    unchanged = []
    for rel, meta in current.items():
        prev = entries.get(rel)
        if args.force or model_changed or prev is None:
            new_files.append(rel)
        elif prev.get("mtime") != meta["mtime"] or prev.get("size") != meta["size"]:
            changed.append(rel)
        else:
            unchanged.append(rel)

    print(f"Scan:      {len(current):>4} files", file=sys.stderr)
    print(f"  new:     {len(new_files):>4}", file=sys.stderr)
    print(f"  changed: {len(changed):>4}", file=sys.stderr)
    print(f"  cached:  {len(unchanged):>4}", file=sys.stderr)
    if model_changed:
        print(f"  (model changed: {prev_model} -> {args.model}, cache invalidated)", file=sys.stderr)

    if args.stats:
        return 0

    to_process = new_files + changed
    if not to_process:
        print("Nothing to transcribe - cache is current.", file=sys.stderr)
        manifest["model"] = args.model
        manifest["last_run"] = int(time.time())
        save_manifest(cache_dir, manifest)
        return 0

    # Load whisper model once
    print(f"Loading Whisper model '{args.model}'...", file=sys.stderr)
    import whisper
    import torch
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}", file=sys.stderr)
    model = whisper.load_model(args.model, device=device)

    t0 = time.perf_counter()
    ok_count = 0
    failed = 0
    for i, rel in enumerate(to_process, 1):
        print(f"  [{i}/{len(to_process)}] {rel}", file=sys.stderr)
        success, msg, duration = transcribe_one(
            model, src, rel, files_dir, args.language
        )
        meta = current[rel]
        entries[rel] = {
            "mtime": meta["mtime"],
            "size": meta["size"],
            "ok": success,
            "msg": msg if not success else "ok",
            "transcribe_sec": round(duration, 1),
        }
        if success:
            ok_count += 1
            print(f"      ok ({duration:.1f}s)", file=sys.stderr)
        else:
            failed += 1
            print(f"      FAILED: {msg}", file=sys.stderr)

    elapsed = time.perf_counter() - t0
    print(f"Transcribed: {ok_count} ok, {failed} failed in {elapsed:.1f}s", file=sys.stderr)

    manifest["model"] = args.model
    manifest["entries"] = entries
    manifest["last_run"] = int(time.time())
    save_manifest(cache_dir, manifest)
    print(f"Cache ready: {files_dir}", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
