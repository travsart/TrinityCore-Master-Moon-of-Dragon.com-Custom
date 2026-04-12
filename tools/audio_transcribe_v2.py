#!/usr/bin/env python3
"""Audio transcription v2 — faster-whisper backend with GPU support.

Upgrades over v1 (audio_transcribe.py):
  - faster-whisper (CTranslate2) instead of openai-whisper: 4-5x faster
  - GPU auto-detect with automatic cuBLAS/cuDNN DLL path setup
  - Initial prompt support for domain vocabulary (attorney names, AFIs, etc.)
  - Word-level timestamps via forced alignment
  - VAD (voice activity detection) filtering to skip silence
  - Per-segment JSON output with confidence scores

Backwards compatible: same CLI flags as v1, same cache dir layout, same
sidecar .txt/.json output format. Keep v1 as fallback for openai-whisper.

Requirements:
  pip install faster-whisper
  pip install nvidia-cublas-cu12 nvidia-cudnn-cu12    # for GPU on Windows
  ffmpeg in PATH

GPU note for RTX 5090 (Blackwell, CC 12.0):
  faster-whisper uses CTranslate2 which has its own CUDA bindings (NOT torch).
  The cuBLAS/cuDNN DLLs come from the nvidia pip packages. This script sets
  PATH before importing faster_whisper so Windows finds the DLLs.

Usage:
  # Single file, GPU auto-detect
  python tools/audio_transcribe_v2.py path/to/file.m4a

  # Directory with medium model + initial prompt
  python tools/audio_transcribe_v2.py "<dir>" --model medium --initial-prompt "Capt Adam Taylor, Col Jon D. Earles, Col Danielle J. Cermak, Laura Iandoli LCSW, Lt Col Johnston, Veritas Military Law, Joshua Tolin, Cannon Air Force Base, DHA, NPDB, QAI, PRHP, AFBCMR, IHPP"

  # Force CPU (for comparison)
  python tools/audio_transcribe_v2.py <dir> --device cpu

  # Stats only
  python tools/audio_transcribe_v2.py <dir> --stats

Models (CPU vs GPU on RTX 5090):
  tiny      39M   ~60x / ~100x realtime
  base      74M   ~15x / ~30x realtime    (v1 tested here)
  small    244M    ~5x / ~20x realtime
  medium   769M    ~1.5x / ~15x realtime  (recommended for legal)
  large-v3 1550M   ~0.7x / ~10x realtime  (best accuracy)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

# === CRITICAL: set PATH before importing faster_whisper ===
# ctranslate2.dll loads cuBLAS during its own init, not via os.add_dll_directory
_nvidia_root = Path(sys.prefix) / "Lib" / "site-packages" / "nvidia"
if _nvidia_root.exists():
    _dll_dirs = [
        _nvidia_root / "cublas" / "bin",
        _nvidia_root / "cudnn" / "bin",
        _nvidia_root / "cuda_nvrtc" / "bin",
    ]
    _existing = [str(d) for d in _dll_dirs if d.exists()]
    if _existing:
        os.environ["PATH"] = ";".join(_existing) + ";" + os.environ.get("PATH", "")

DEFAULT_CACHE_ROOT = Path(__file__).resolve().parents[1] / ".cache" / "transcribed"
DEFAULT_EXTS = {".m4a", ".mp3", ".wav", ".ogg", ".flac", ".aac", ".wma", ".opus"}

# Case-specific vocabulary — improves proper name recognition
DEFAULT_INITIAL_PROMPT = (
    "Capt Adam J. Taylor, Col Jon D. Earles, Col Danielle J. Cermak, "
    "Laura Iandoli LCSW, Lt Col Johnston, Joshua Tolin, Veritas Military Law, "
    "Cannon Air Force Base, 27th Special Operations Medical Group, "
    "DHA, NPDB, QAI, PRHP, AFBCMR, IHPP, DCSA, NARSUM, MEB, PCS, "
    "Privileging Authority, summary suspension, clinical privileges, "
    "Article 15, non-judicial punishment."
)


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
        return {"source": "", "model": "", "backend": "", "entries": {}}
    try:
        return json.loads(mf.read_text(encoding="utf-8"))
    except Exception:
        return {"source": "", "model": "", "backend": "", "entries": {}}


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


def detect_device(requested: str) -> tuple[str, str]:
    """Return (device, compute_type) based on request and availability."""
    if requested == "cpu":
        return "cpu", "int8"
    # auto or cuda
    try:
        import ctranslate2
        if ctranslate2.get_cuda_device_count() > 0:
            return "cuda", "float16"
    except Exception:
        pass
    if requested == "cuda":
        print("WARN: --device cuda requested but no CUDA device available. Falling back to CPU.", file=sys.stderr)
    return "cpu", "int8"


def transcribe_one(model, src_root: Path, rel: str, files_dir: Path,
                   language: str | None, initial_prompt: str | None,
                   beam_size: int, word_timestamps: bool) -> tuple[bool, str, float, int]:
    abs_src = src_root / rel if src_root.is_dir() else src_root
    t0 = time.perf_counter()
    try:
        segments, info = model.transcribe(
            str(abs_src),
            language=language,
            beam_size=beam_size,
            initial_prompt=initial_prompt,
            word_timestamps=word_timestamps,
            vad_filter=True,
            vad_parameters={"min_silence_duration_ms": 500},
        )
        # segments is a generator; force evaluation
        seg_list = list(segments)
    except Exception as e:
        return False, f"faster-whisper error: {e}", time.perf_counter() - t0, 0

    elapsed = time.perf_counter() - t0
    text = "".join(s.text for s in seg_list).strip()

    out_txt = files_dir / (rel + ".txt")
    out_json = files_dir / (rel + ".json")
    out_txt.parent.mkdir(parents=True, exist_ok=True)

    header = (
        f"Source: {rel}\n"
        f"Language: {info.language} (prob {info.language_probability:.2f})\n"
        f"Duration-sec: {info.duration:.1f}\n"
        f"Segments: {len(seg_list)}\n"
        f"Backend: faster-whisper ({model.model.device})\n"
        f"Transcribe-sec: {elapsed:.1f}\n"
        f"Speedup: {info.duration/elapsed:.1f}x realtime\n\n"
    )
    out_txt.write_text(header + text, encoding="utf-8", newline="\n")

    json_segments = []
    for s in seg_list:
        seg_dict = {
            "start": s.start,
            "end": s.end,
            "text": s.text,
            "avg_logprob": s.avg_logprob,
            "no_speech_prob": s.no_speech_prob,
        }
        if word_timestamps and s.words:
            seg_dict["words"] = [
                {"start": w.start, "end": w.end, "word": w.word, "probability": w.probability}
                for w in s.words
            ]
        json_segments.append(seg_dict)

    out_json.write_text(
        json.dumps(
            {
                "source": rel,
                "language": info.language,
                "language_probability": info.language_probability,
                "duration": info.duration,
                "transcribe_sec": round(elapsed, 2),
                "speedup": round(info.duration / elapsed, 1),
                "text": text,
                "segments": json_segments,
            },
            indent=2,
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    return True, "ok", elapsed, len(seg_list)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="Audio file or directory")
    parser.add_argument("--out", help="Cache/output dir (default: .cache/transcribed/<slug>/)")
    parser.add_argument("--model", default="base",
                        help="tiny | base | small | medium | large-v3 (default: base)")
    parser.add_argument("--language", default=None, help="Language hint (default: auto)")
    parser.add_argument("--device", choices=["auto", "cuda", "cpu"], default="auto")
    parser.add_argument("--compute-type", default=None,
                        help="Override compute type (float16 for GPU, int8 for CPU)")
    parser.add_argument("--beam-size", type=int, default=5)
    parser.add_argument("--initial-prompt", default=DEFAULT_INITIAL_PROMPT,
                        help="Domain vocab prompt (default: case-specific preset). Pass empty string to disable.")
    parser.add_argument("--no-word-timestamps", action="store_true",
                        help="Disable word-level timestamps (faster)")
    parser.add_argument("--types", default=",".join(sorted(DEFAULT_EXTS)))
    parser.add_argument("--stats", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    src = Path(args.source).resolve()
    if not src.exists():
        print(f"ERROR: {src} does not exist", file=sys.stderr)
        return 1

    cache_dir = cache_dir_for(src, Path(args.out).resolve() if args.out else None)
    files_dir = cache_dir / "files"
    files_dir.mkdir(parents=True, exist_ok=True)

    include_exts = {e.strip().lower() for e in args.types.split(",") if e.strip()}
    device, default_compute = detect_device(args.device)
    compute_type = args.compute_type or default_compute

    print(f"Source:       {src}", file=sys.stderr)
    print(f"Cache:        {cache_dir}", file=sys.stderr)
    print(f"Model:        {args.model}", file=sys.stderr)
    print(f"Device:       {device} ({compute_type})", file=sys.stderr)
    print(f"Beam size:    {args.beam_size}", file=sys.stderr)
    print(f"Word-ts:      {not args.no_word_timestamps}", file=sys.stderr)
    if args.initial_prompt:
        print(f"Initial prompt: {args.initial_prompt[:80]}{'...' if len(args.initial_prompt) > 80 else ''}", file=sys.stderr)

    manifest = load_manifest(cache_dir)
    manifest["source"] = str(src)
    entries = manifest.get("entries", {})

    current = scan_source(src, include_exts)

    # Invalidate on model or backend change
    backend_key = f"faster-whisper:{args.model}:{device}:{compute_type}"
    prev_backend = manifest.get("backend", "")
    # Treat missing backend (legacy v1 cache) OR changed backend as invalidation
    backend_changed = prev_backend != backend_key

    new_files = []
    changed = []
    unchanged = []
    for rel, meta in current.items():
        prev = entries.get(rel)
        if args.force or backend_changed or prev is None:
            new_files.append(rel)
        elif prev.get("mtime") != meta["mtime"] or prev.get("size") != meta["size"]:
            changed.append(rel)
        else:
            unchanged.append(rel)

    print(f"Scan:      {len(current):>4} files", file=sys.stderr)
    print(f"  new:     {len(new_files):>4}", file=sys.stderr)
    print(f"  changed: {len(changed):>4}", file=sys.stderr)
    print(f"  cached:  {len(unchanged):>4}", file=sys.stderr)
    if backend_changed:
        print(f"  (backend: {prev_backend} -> {backend_key}, cache invalidated)", file=sys.stderr)

    if args.stats:
        return 0

    to_process = new_files + changed
    if not to_process:
        print("Nothing to transcribe - cache is current.", file=sys.stderr)
        manifest["backend"] = backend_key
        manifest["model"] = args.model
        manifest["last_run"] = int(time.time())
        save_manifest(cache_dir, manifest)
        return 0

    print(f"Loading faster-whisper model '{args.model}' on {device}...", file=sys.stderr)
    from faster_whisper import WhisperModel
    model = WhisperModel(args.model, device=device, compute_type=compute_type)

    initial_prompt = args.initial_prompt if args.initial_prompt else None
    word_timestamps = not args.no_word_timestamps

    t0 = time.perf_counter()
    ok_count = 0
    failed = 0
    for i, rel in enumerate(to_process, 1):
        print(f"  [{i}/{len(to_process)}] {rel}", file=sys.stderr)
        success, msg, elapsed, n_segs = transcribe_one(
            model, src, rel, files_dir, args.language, initial_prompt,
            args.beam_size, word_timestamps,
        )
        meta = current[rel]
        entries[rel] = {
            "mtime": meta["mtime"],
            "size": meta["size"],
            "ok": success,
            "msg": msg if not success else "ok",
            "transcribe_sec": round(elapsed, 1),
            "segments": n_segs,
        }
        if success:
            ok_count += 1
            print(f"      ok ({elapsed:.1f}s, {n_segs} segments)", file=sys.stderr)
        else:
            failed += 1
            print(f"      FAILED: {msg}", file=sys.stderr)

    total = time.perf_counter() - t0
    print(f"Transcribed: {ok_count} ok, {failed} failed in {total:.1f}s", file=sys.stderr)

    manifest["backend"] = backend_key
    manifest["model"] = args.model
    manifest["entries"] = entries
    manifest["last_run"] = int(time.time())
    save_manifest(cache_dir, manifest)
    print(f"Cache ready: {files_dir}", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
