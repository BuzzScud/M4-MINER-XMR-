#!/bin/zsh
# Control helper. Starts xmrig from this process (not launchd).
# launchd cannot run binaries under Desktop (TCC hang: 0% CPU, empty logs).
# Subcommands: status | start | stop | kill | nice
set -uo pipefail

ROOT="${0:A:h:h}"
XMRIG="$ROOT/bin/xmrig"
PLIST="$ROOT/com.minerv3.xmrig.plist"
LOGS="$ROOT/logs"
PIDFILE="$LOGS/xmrig.pid"
API="http://127.0.0.1:18088/2/summary"
HTTP_PORT=18088

mkdir -p "$LOGS"

xmrig_pids() {
  pgrep -x xmrig 2>/dev/null || true
}

listen_pids() {
  lsof -nP -iTCP:"$HTTP_PORT" -sTCP:LISTEN -t 2>/dev/null || true
}

api_up() {
  curl -sf --max-time 1 -o /dev/null "$API" 2>/dev/null
}

is_up() {
  [[ -n $(xmrig_pids) ]] && return 0
  api_up && return 0
  [[ -n $(listen_pids) ]] && return 0
  return 1
}

NICE_TARGET=-10

current_nice() {
  local p
  p=$(xmrig_pids | head -1)
  [[ -n $p ]] || return 1
  ps -o ni= -p "$p" 2>/dev/null | tr -d ' '
}

# --cpu-priority=4 maps to nice -10 on macOS, but setpriority(-10) is EPERM
# without root and Platform_mac::setProcessPriority is a no-op. Try unprivileged
# then passwordless sudo. Never prompt: the TUI is in raw mode.
apply_nice() {
  local -a pids
  pids=("${(@f)$(xmrig_pids)}")
  if (( ${#pids} == 0 )) || [[ -z ${pids[1]:-} ]]; then
    echo "Nice: — (xmrig not up yet)"
    return 1
  fi
  if /usr/bin/renice "$NICE_TARGET" -p "${pids[@]}" >/dev/null 2>&1; then
    echo "Nice: $NICE_TARGET"
    return 0
  fi
  if sudo -n /usr/bin/renice "$NICE_TARGET" -p "${pids[@]}" >/dev/null 2>&1; then
    echo "Nice: $NICE_TARGET"
    return 0
  fi
  local ni
  ni=$(current_nice || true)
  echo "Nice: ${ni:-0} (wanted $NICE_TARGET; --cpu-priority needs root)"
  return 1
}

kill_all() {
  # pidfile is caffeinate (parent of xmrig), not xmrig itself
  if [[ -f $PIDFILE ]]; then
    kill "$(cat "$PIDFILE")" 2>/dev/null || true
  fi
  pkill -x xmrig 2>/dev/null || true
  local -a extra
  extra=("${(@f)$(listen_pids)}")
  if (( ${#extra} )); then
    kill "${extra[@]}" 2>/dev/null || true
  fi
  local i=0
  while is_up && (( i < 20 )); do
    pkill -x xmrig 2>/dev/null || true
    sleep 0.25
    i=$((i + 1))
  done
  if is_up; then
    pkill -KILL -x xmrig 2>/dev/null || true
    extra=("${(@f)$(listen_pids)}")
    if (( ${#extra} )); then
      kill -KILL "${extra[@]}" 2>/dev/null || true
    fi
    sleep 0.2
  fi
  rm -f "$PIDFILE"
}

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
    ni=$(current_nice || true)
    [[ -n $ni ]] && echo "Nice: $ni (wanted $NICE_TARGET)"
    ;;

  nice)
    if ! is_up; then echo "Not running."; exit 1; fi
    apply_nice
    ;;

  start)
    if is_up; then
      echo "Already running."
      pids=$(xmrig_pids)
      [[ -n $pids ]] && echo "PID: ${pids//$'\n'/ }"
      if api_up; then
        curl -s --max-time 2 "$API" 2>/dev/null | python3 -c '
import json,sys
try:
    d=json.load(sys.stdin)
    up=int(d.get("uptime") or 0)
    h, rem = divmod(up, 3600); m = rem // 60
    print(f"Uptime: {h}h {m}m" if h else f"Uptime: {m}m")
except Exception:
    pass
' || true
      fi
      apply_nice || true
      exit 0
    fi
    mkdir -p "$LOGS"
    : >>"$LOGS/xmrig.log" >>"$LOGS/xmrig.err.log"
    local -a cmd
    cmd=()
    while IFS= read -r -d '' a; do cmd+="$a"; done < <(args_from_plist)
    if (( ${#cmd} == 0 )); then
      echo "ERROR: could not read $PLIST"
      exit 1
    fi
    # --log-file is the only writer of xmrig.log. Console stdout used to be
    # redirected onto the same path; FileLogWriter then overwrote it from
    # offset 0 and the file stayed empty.
    local has_log=0 a
    for a in "${cmd[@]}"; do
      [[ "$a" == --log-file=* || "$a" == -l ]] && has_log=1
    done
    if (( !has_log )); then
      cmd+=("--log-file=$LOGS/xmrig.log")
    fi
    cd "$ROOT" || exit 1
    /usr/bin/caffeinate -i "${cmd[@]}" >>"$LOGS/xmrig.err.log" 2>&1 &
    echo $! > "$PIDFILE"
    echo "Started. It takes about a minute to reach full speed."
    local i=0
    while [[ -z $(xmrig_pids) ]] && (( i < 25 )); do
      sleep 0.1
      i=$((i + 1))
    done
    apply_nice || true
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
    kill_all
    if is_up; then
      echo "ERROR: xmrig still running after stop."
      exit 1
    fi
    echo "Stopped."
    ;;

esac
