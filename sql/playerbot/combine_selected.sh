#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 [-o output.sql] file|dir [file2 ...]"
  exit 1
}

OUT="combined_selected.sql"
while getopts ":o:" opt; do
  case $opt in
    o) OUT="$OPTARG" ;;
    *) usage ;;
  esac
done
shift $((OPTIND-1))

if [ $# -eq 0 ]; then
  usage
fi

FILES=()
for arg in "$@"; do
  if [ -d "$arg" ]; then
    while IFS= read -r -d '' f; do
      FILES+=("$f")
    done < <(find "$arg" -maxdepth 1 -type f -name '*.sql' -print0 | sort -z)
  elif [ -f "$arg" ]; then
    FILES+=("$arg")
  else
    echo "Warning: '$arg' not found or not a file/dir" >&2
  fi
done

if [ ${#FILES[@]} -eq 0 ]; then
  echo "No SQL files found to combine." >&2
  exit 1
fi

TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

echo "-- =====================================================" > "$TMP"
echo "-- Combined SQL generated on $(date -u +'%Y-%m-%d %H:%M:%SZ')" >> "$TMP"
echo "-- Sources:" >> "$TMP"
for f in "${FILES[@]}"; do
  echo "--  $f" >> "$TMP"
done
echo "-- =====================================================\n" >> "$TMP"

for f in "${FILES[@]}"; do
  echo "-- ----- Begin file: $f -----" >> "$TMP"
  cat "$f" >> "$TMP"
  echo "-- ----- End file: $f -----" >> "$TMP"
done

mv "$TMP" "$OUT"
echo "Wrote $OUT with ${#FILES[@]} files."
