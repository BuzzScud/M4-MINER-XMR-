#!/bin/zsh
# Find the best RandomX thread count on this Mac. Offline --bench, no pool.
set -uo pipefail

ROOT="${0:A:h:h}"
XMRIG="${XMRIG:-$ROOT/bin/xmrig}"
PORT=18099
WARMUP=70
SAMPLES=6
MODE="${RX_MODE:-fast}"

[[ -x "$XMRIG" ]] || { echo "xmrig not found at $XMRIG"; exit 1; }

THREAD_LIST=("$@")
[[ ${#THREAD_LIST[@]} -eq 0 ]] && THREAD_LIST=(4 5 6 7 8 9 10)

echo
echo "RandomX thread sweep — mode=$MODE, $(sysctl -n machdep.cpu.brand_string)"
echo "  $(sysctl -n hw.perflevel0.logicalcpu)P + $(sysctl -n hw.perflevel1.logicalcpu)E cores, $(( $(sysctl -n hw.memsize) / 1073741824 )) GB RAM"
echo

printf "  %-8s %-12s %s\n" "threads" "hashrate" "notes"
printf "  %-8s %-12s %s\n" "-------" "----------" "-----"

BEST_T=0; BEST_H=0

for T in "${THREAD_LIST[@]}"; do
  LOG=$(mktemp -t xmrbench)
  "$XMRIG" --bench=10M \
           --randomx-mode="$MODE" \
           --threads="$T" \
           --cpu-priority=4 \
           --randomx-init=8 \
           --http-host=127.0.0.1 --http-port="$PORT" \
           --no-color > "$LOG" 2>&1 &
  PID=$!

  sleep "$WARMUP"

  H=0
  for _ in $(seq 1 $SAMPLES); do
    S=$(curl -s --max-time 5 "http://127.0.0.1:$PORT/2/summary" 2>/dev/null)
    V=$(printf '%s' "$S" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    t = (d.get('hashrate') or {}).get('total') or []
    vals = [x for x in t if isinstance(x, (int, float))]
    print(max(vals) if vals else 0)
except Exception:
    print(0)
" 2>/dev/null)
    [[ -n "$V" ]] && (( ${V%.*} > ${H%.*} )) && H="$V"
    sleep 5
  done

  kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null

  PAGES=$(grep -m1 -iE "huge pages" "$LOG" | sed 's/.*huge pages//' | tr -s ' ' | cut -c1-28)
  printf "  %-8s %-12s %s\n" "$T" "$(printf '%.1f H/s' "$H")" "${PAGES:-}"
  rm -f "$LOG"

  if (( ${H%.*} > ${BEST_H%.*} )); then BEST_H="$H"; BEST_T="$T"; fi
done

echo
printf "  best: %s threads at %.1f H/s\n" "$BEST_T" "$BEST_H"
echo
