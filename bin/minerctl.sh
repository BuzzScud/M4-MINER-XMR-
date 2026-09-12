#!/bin/zsh
# Control helper. Starts xmrig from this process (not launchd).
# launchd cannot run binaries under Desktop (TCC hang: 0% CPU, empty logs).
# Subcommands: status | start | stop | kill
set -uo pipefail

ROOT="${0:A:h:h}"
XMRIG="$ROOT/bin/xmrig"
PLIST="$ROOT/com.minerv3.xmrig.plist"
LOGS="$ROOT/logs"
PIDFILE="$LOGS/xmrig.pid"
API="http://127.0.0.1:18088/2/summary"

mkdir -p "$LOGS"

is_up() { pgrep -x xmrig >/dev/null 2>&1; }

args_from_plist() {
  python3 - "$PLIST" "$ROOT" <<'PY'
import os, sys, plistlib
p = plistlib.load(open(sys.argv[1], "rb"))
flex = os.path.exists(os.path.join(sys.argv[2], "flex.on"))
args = p.get("ProgramArguments") or []
if args and str(args[0]).endswith("caffeinate"):
    args = args[2:] if len(args) >= 2 and args[1] == "-i" else args[1:]
out = []
skip = 0
for i, a in enumerate(args):
    if skip:
        skip = 0
        continue
    if (not flex) or a not in ("-a", "rx/0"):
        if flex and a == "-a":
            skip = 1
            continue
        out.append(a)
print("\0".join(out))
PY
}

case "${1:-status}" in

  status)
    if ! is_up; then echo "STOPPED"; exit 0; fi
    J=$(curl -s --max-time 4 "$API" 2>/dev/null)
    if [[ -z "$J" ]]; then echo "STARTING"; exit 0; fi
    echo "$J" | python3 -c '
import json,sys
d=json.load(sys.stdin); c=d["connection"]; h=d["hashrate"]["total"][0]
up=d["uptime"]; hrs,rem=divmod(up,3600); mins=rem//60
acc=c["accepted"]; rej=c["rejected"]; pool=c["pool"]
print("RUNNING")
print(f"Speed: {h:.0f} H/s" if h else "Speed: warming up...")
print(f"Running for: {hrs}h {mins}m")
print(f"Shares: {acc} accepted, {rej} rejected")
print(f"Pool: {pool}")
'
    ;;

  start)
    if is_up; then echo "Already running."; exit 0; fi
    mkdir -p "$LOGS"
    # read argv from plist, run under caffeinate from THIS shell (has Desktop TCC)
    local -a cmd
    cmd=()
    while IFS= read -r -d '' a; do cmd+="$a"; done < <(args_from_plist)
    if (( ${#cmd} == 0 )); then
      echo "ERROR: could not read $PLIST"
      exit 1
    fi
    cd "$ROOT" || exit 1
    /usr/bin/caffeinate -i "${cmd[@]}" >>"$LOGS/xmrig.log" 2>>"$LOGS/xmrig.err.log" &
    echo $! > "$PIDFILE"
    echo "Started. It takes about a minute to reach full speed."
    ;;

  stop|kill)
    if ! is_up; then echo "Not running."; exit 0; fi
    reason="${1:-stop}"
    J=$(curl -s --max-time 2 "$API" 2>/dev/null || true)
    if [[ -n $J ]]; then
      print -r -- "$J" | python3 "$ROOT/bin/write-session-summary.py" --reason "$reason" --api-stdin || true
    else
      python3 "$ROOT/bin/write-session-summary.py" --reason "$reason" || true
    fi
    if [[ -f $PIDFILE ]]; then
      kill "$(cat "$PIDFILE")" 2>/dev/null || true
    fi
    pkill -x xmrig 2>/dev/null || true
    rm -f "$PIDFILE"
    echo "Stopped."
    ;;

esac
