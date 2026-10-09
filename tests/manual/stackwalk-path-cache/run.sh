#!/usr/bin/env bash
set -euo pipefail

if (( $# < 1 || $# > 3 )); then
  echo "usage: $0 host|container [count] [compat|walk|name|liboffset|both]" >&2
  exit 2
fi

cd "$(dirname "$0")"
kind=$1
count=${2:-16}
mode=${3:-both}
case "$kind" in host|container) ;; *) echo "invalid kind: $kind" >&2; exit 2;; esac
case "$mode" in compat|walk|name|liboffset|both) ;; *) echo "invalid mode: $mode" >&2; exit 2;; esac

output=${OUTPUT_DIR:-$PWD/results/$kind-$mode}
test ! -e "$output"
mkdir -p "$output/malloc"
pids=()

cleanup() {
  if ((${#pids[@]})); then
    kill "${pids[@]}" 2>/dev/null || true
    wait "${pids[@]}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

for _ in $(seq "$count"); do
  if [[ $kind == container ]]; then
    unshare -Urm chroot "$PWD/root" /app/target &
  else
    LD_LIBRARY_PATH="$PWD/root/app" "$PWD/root/app/target" &
  fi
  pids+=("$!")
done

sleep 1
for pid in "${pids[@]}"; do
  test -r "/proc/$pid/status"
done

if [[ $mode == compat ]]; then
  ./walk16 "${pids[@]}" > "$output/summary.txt" 2> "$output/walk.err"
else
  MALLOC_INFO_DIR="$output/malloc" \
    ./walk_many "$mode" "${pids[@]}" \
    > "$output/stages.tsv" 2> "$output/walk.err"
fi

echo "$output"
