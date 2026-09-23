#!/bin/zsh
# Control helper. Starts xmrig from this process (not launchd).
# launchd cannot run binaries under Desktop (TCC hang: 0% CPU, empty logs).
# Subcommands: status | start | stop | kill | restart | nice | job | perf [...] | config [...]
#              | bench [...] | fleet [...] | remote [...] | events [...] | update | release | role | help
# The job file is rendered for THIS Mac at every start (bin/machine.sh).
# perf / config: change threads and the other machine.local settings; applies now
#   (restarts xmrig if it is running, without a Desktop summary).
# bench: offline thread sweep (bin/xmr_bench_sweep.sh); refuses while mining.
# fleet: every Mac's miner at once (bin/fleet.py; `fleet here` checks this Mac).
# remote / events: the main Mac starts and stops the others (bin/control.py, signed commands);
#   every start and stop lands in logs/events.jsonl with why ($MINER_WHY) and who asked ($MINER_BY).
# release / update / role: one main Mac releases (commit, then push main and GitHub's stable
#   branch); every other Mac is a follower that becomes that release when XMR Miner opens there
#   (bin/miner-ui.sh runs `update --on-open`) and leaves xmrig stopped until you press s.
set -uo pipefail
# zsh runs `&` jobs at nice +5 (BG_NICE, on even in scripts), which put xmrig below every
# other app. Off, it starts at nice 0; apply_nice can still only go lower with root.
setopt no_bg_nice

ROOT="${0:A:h:h}"
XMRIG="$ROOT/bin/xmrig"
PLIST="$ROOT/com.minerv3.xmrig.plist"
LOGS="$ROOT/logs"
PIDFILE="$LOGS/xmrig.pid"
API="http://127.0.0.1:18088/2/summary"
HTTP_PORT=18088
# install.sh bakes this folder's path into the first two, so on every other Mac they differ from git.
GENERATED=(bin/miner.applescript "XMR Miner.app" bin/xmrig)
NOTICE="$LOGS/update-notice.json"  # what the last update did, for the UI's next window
FOLLOWER_PUSHURL="DISABLED--this-Mac-follows-releases-from-the-main-Mac"

source "$ROOT/bin/machine.sh"
read_fleet_token "$ROOT" >/dev/null

mkdir -p "$LOGS"

# GET the local API. With fleet.token the xmrig API wants "Bearer <token>"; an xmrig
# started before the token existed answers 401 to any Authorization header, so retry bare.
api_curl() {
  local url="$1"; shift
  if [[ -n "${FLEET_TOKEN:-}" ]]; then
    curl -sf -H "Authorization: Bearer $FLEET_TOKEN" "$@" "$url" 2>/dev/null && return 0
  fi
  curl -sf "$@" "$url" 2>/dev/null
}

xmrig_pids() {
  pgrep -x xmrig 2>/dev/null || true
}

listen_pids() {
  lsof -nP -iTCP:"$HTTP_PORT" -sTCP:LISTEN -t 2>/dev/null || true
}

api_up() {
  api_curl "$API" --max-time 1 -o /dev/null
}

is_up() {
  [[ -n $(xmrig_pids) ]] && return 0
  api_up && return 0
  [[ -n $(listen_pids) ]] && return 0
  return 1
}

NICE_TARGET=-10

# One line in logs/events.jsonl: start / stop, why (window, remote, cli, restart, settings, update)
# and who asked. Never fails the command it describes.
note() {
  python3 "$ROOT/bin/control.py" note "$@" >/dev/null 2>&1 || true
}

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

# Re-render the job file from machine.local and, when xmrig is running on a job that
# changed, restart it so the setting applies now. A restart is not a stop: no Desktop summary.
apply_settings() {
  local problems before after
  problems=$(local_problems "$ROOT")
  if [[ -n $problems ]]; then
    echo "machine.local: these lines are skipped (fix them: ./bin/minerctl.sh config edit):"
    print -r -- "$problems" | sed 's/^/  /'
  fi
  before=$(cat "$PLIST" 2>/dev/null || true)
  write_job_file "$ROOT" --if-changed || return 1
  after=$(cat "$PLIST" 2>/dev/null || true)
  if ! is_up; then
    echo "Saved. Applies on the next start (s in the UI, or ./bin/minerctl.sh start)."
    return 0
  fi
  if [[ "$before" == "$after" ]]; then
    echo "No change to the job; xmrig keeps running."
    return 0
  fi
  echo "Restarting xmrig with $THREADS threads, $MODE mode..."
  api_curl "$API" --max-time 2 | note stop --why settings --api-stdin
  kill_all
  export MINER_WHY=settings
  exec "$ROOT/bin/minerctl.sh" start
}

# Threads, CPU share and live speed for `perf` with no argument.
perf_print() {
  local pct=$(( (THREADS * 100 + CORES / 2) / CORES ))
  local src="auto"
  [[ "$THREADS_SPEC" != auto ]] && src="THREADS=$THREADS_SPEC"
  echo "Threads: $THREADS of $CORES logical CPUs  ($src; auto is $AUTO_THREADS)"
  echo "CPU:     about $pct% of this Mac while mining ($((THREADS * 100))% on xmrig's row in Activity Monitor)"
  echo "Mode:    $MODE   Yield: $YIELD"
  if api_up; then
    api_curl "$API" --max-time 2 | python3 -c '
import json,sys
try:
    d=json.load(sys.stdin); t=d["hashrate"]["total"]
    v=[x for x in t if x]
    print(f"Now:     {v[0]:,.0f} H/s (10s)" + (f", {v[-1]:,.0f} H/s longest window" if len(v) > 1 else ""))
except Exception:
    pass
' || true
  else
    echo "Now:     not mining"
  fi
  if [[ "$ARCH" != arm64 && "$L3" -gt 0 ]] && (( THREADS > AUTO_THREADS )); then
    echo "Note:    RandomX wants 2 MB of L3 per thread and this CPU has $(( L3 / 1048576 )) MB, so threads above"
    echo "         $AUTO_THREADS add CPU load faster than H/s (and heat). ./bin/minerctl.sh bench measures it."
  fi
  echo
  echo "Change:  ./bin/minerctl.sh perf up | down | max | eco | auto | <1-$CORES> | <1-100>%"
}

config_usage() {
  cat <<EOF
Usage: ./bin/minerctl.sh config [show]            effective settings + machine.local
       ./bin/minerctl.sh config set KEY=value ...  e.g. config set THREADS=4 MODE=fast
       ./bin/minerctl.sh config unset KEY ...      back to the automatic value
       ./bin/minerctl.sh config edit               open machine.local in \$EDITOR (nano), then apply
       ./bin/minerctl.sh config apply              re-read machine.local now
       ./bin/minerctl.sh config reset              delete machine.local (everything auto)
       ./bin/minerctl.sh config path               print the machine.local path
Keys:  ${SETTING_KEYS[*]}
Changes apply at once; a running xmrig restarts (about a minute to full speed).
EOF
}

# A commented machine.local for `config edit` when there is none yet.
local_template() {
  cat <<EOF
# machine.local: tuning for this Mac only (gitignored). One KEY=value per line; # starts a comment.
# Save and quit the editor; the miner applies it (a running xmrig restarts).
#
# THREADS=auto   1-$CORES | auto ($AUTO_THREADS here) | max ($CORES) | eco (half of auto) | 75% (of logical CPUs)
# MODE=auto      fast (2 GB dataset, 8 GB+ RAM) | light (256 MB, much slower) | auto
# WORKER=$WORKER
# POOL=$POOL_DEFAULT
# BACKUP=$BACKUP_DEFAULT   host:port | off: tried after 5 failures on POOL, dropped when POOL answers
# TLS=on         off only for a pool port without TLS
# YIELD=off      on: other apps get the CPU first (lower H/s)
# PAUSE=120      pause while the keyboard or mouse is in use, mine after N s idle (10-3600) | off
# LAN=on         off: API on 127.0.0.1 only (the fleet view cannot see this Mac)
EOF
}

# ------------------------------------------------------------------ releases
# git fetch REFSPEC... from origin, waiting at most $1 seconds (0 = no limit). Never asks for a
# password. On failure FETCH_ERR holds a short reason.
fetch_refs() {
  local secs=$1 err pid n=0
  shift
  FETCH_ERR=""
  err=$(mktemp "${TMPDIR:-/tmp}/minerfetch.XXXXXX") || return 1
  GIT_TERMINAL_PROMPT=0 git -C "$ROOT" fetch --quiet --no-tags origin "$@" 2>"$err" &
  pid=$!
  while (( secs > 0 )) && kill -0 $pid 2>/dev/null; do
    if (( n >= secs * 4 )); then
      pkill -P $pid 2>/dev/null
      kill $pid 2>/dev/null
      wait $pid 2>/dev/null
      rm -f "$err"
      FETCH_ERR="GitHub did not answer in $secs s"
      return 1
    fi
    sleep 0.25
    n=$(( n + 1 ))
  done
  if wait $pid; then
    rm -f "$err"
    return 0
  fi
  # git's first fatal line says why; the lines after it are generic advice
  FETCH_ERR=$(grep -m1 -E '^(fatal|error): ' "$err" | sed -E 's/^(fatal|error): //' | cut -c1-200)
  [[ -n $FETCH_ERR ]] || FETCH_ERR=$(grep -v '^[[:space:]]*$' "$err" | tail -1 | cut -c1-200)
  rm -f "$err"
  [[ -n $FETCH_ERR ]] || FETCH_ERR="git fetch failed"
  return 1
}

# A note for the UI's next window; bin/miner-ui.py shows it once, then deletes it.
#   update_notice ok FROM TO WAS_UP SAVED INSTALL_OK        update_notice fail REASON
update_notice() {
  python3 - "$NOTICE" "$ROOT" "$@" <<'PY' || true
import json, os, subprocess, sys, time
path, root, status, rest = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
def git(*a):
    return subprocess.run(["git", "-C", root, *a], capture_output=True, text=True).stdout.strip()
n = {"status": status, "ts": time.time()}
if status == "ok":
    old, new, was_up, saved, install_ok = rest
    rng = f"{old}..{new}"
    n.update(frm=old[:7], to=new[:7], count=int(git("rev-list", "--count", rng) or 0),
             commits=git("log", "-3", "--format=%s", rng).splitlines(),
             was_up=was_up == "1", saved=saved, install_ok=install_ok == "1")
else:
    n.update(reason=rest[0] if rest else "", at=git("log", "-1", "--format=%h"))
with open(path + ".tmp", "w", encoding="utf-8") as f:
    json.dump(n, f)
os.replace(path + ".tmp", path)
PY
}

# A follower tracks GitHub's stable branch and cannot push (only the main Mac releases).
follower_git_config() {
  git -C "$ROOT" config branch.stable.remote origin
  git -C "$ROOT" config branch.stable.merge refs/heads/stable
  git -C "$ROOT" config remote.origin.pushurl "$FOLLOWER_PUSHURL"
}

# Follower: become exactly the latest release (origin/stable), reinstall, leave xmrig stopped.
# Edits and commits made on this Mac are saved first (git stash / a backup/<date> branch).
# $1 = 1 when the app is opening: quiet when up to date, 15 s for GitHub, a note for the UI.
follower_update() {
  local rc
  if ! mkdir "$LOGS/update.lock" 2>/dev/null; then
    # Two windows opened at once. A lock older than 10 minutes was left by a crash.
    if ! { [[ -n $(find "$LOGS/update.lock" -maxdepth 0 -mmin +10 2>/dev/null) ]] &&
           rmdir "$LOGS/update.lock" 2>/dev/null && mkdir "$LOGS/update.lock" 2>/dev/null; }; then
      (( $1 )) || echo "Another update is running; try again in a minute."
      return 0
    fi
  fi
  follower_sync "$1"
  rc=$?
  rmdir "$LOGS/update.lock" 2>/dev/null
  return $rc
}

follower_sync() {
  local on_open=$1 target head branch edits incoming ahead stamp saved="" was_up=0 inst=1
  local -a G keep
  G=(git -C "$ROOT")
  keep=(. "${GENERATED[@]/#/:!}")
  (( on_open )) && print -n "Checking GitHub for a release… "
  if ! fetch_refs $(( on_open ? 15 : 0 )) '+refs/heads/stable:refs/remotes/origin/stable'; then
    (( on_open )) && echo
    if [[ $FETCH_ERR == *"find remote ref"* ]]; then
      (( on_open )) || echo "No release yet: GitHub has no stable branch (the main Mac makes it: ./bin/minerctl.sh release)."
      return 0
    fi
    if (( on_open )); then
      echo "$FETCH_ERR; opening the version this Mac has."
      update_notice fail "$FETCH_ERR"
      return 0
    fi
    echo "ERROR: could not fetch the release: $FETCH_ERR"
    return 1
  fi
  target=$("${G[@]}" rev-parse -q --verify 'refs/remotes/origin/stable^{commit}')
  head=$("${G[@]}" rev-parse -q --verify 'HEAD^{commit}')
  branch=$("${G[@]}" symbolic-ref -q --short HEAD)
  edits=$("${G[@]}" status --porcelain --untracked-files=no -- "${keep[@]}")
  if [[ $head == "$target" && -z $edits ]]; then
    [[ $branch == stable ]] || "${G[@]}" checkout -q -B stable "$target"
    follower_git_config
    if (( on_open )); then echo "up to date."; else echo "Up to date: release $("${G[@]}" log -1 --format='%h %s')"; fi
    return 0
  fi
  (( on_open )) && echo "new release."
  incoming=$("${G[@]}" log --format='  %h %s' "HEAD..$target")
  ahead=$("${G[@]}" rev-list --count "$target..HEAD")
  echo "Updating to release $("${G[@]}" log -1 --format='%h %s' "$target")"
  [[ -n $incoming ]] && print -r -- "$incoming"
  if is_up; then
    was_up=1
    if ! MINER_WHY=update "$ROOT/bin/minerctl.sh" stop; then
      echo "ERROR: could not stop xmrig; nothing changed."
      (( on_open )) && update_notice fail "could not stop xmrig for the update"
      return 1
    fi
  fi
  stamp=$(date +%Y%m%d-%H%M%S)
  if (( ahead > 0 )); then
    "${G[@]}" branch -f "backup/$stamp" HEAD
    saved="$ahead commit(s) made here → branch backup/$stamp"
    echo "  Saved this Mac's own $ahead commit(s) as branch backup/$stamp"
  fi
  if [[ -n $edits ]]; then
    # a fixed identity: a follower may have no git user set up, and stash makes a commit
    if ! git -c user.name=minerctl -c user.email=minerctl@localhost -C "$ROOT" \
         stash push -q -m "minerctl update $stamp: edits made on this Mac" -- "${keep[@]}"; then
      echo "ERROR: could not save this Mac's edits (git stash); nothing changed."
      (( on_open )) && update_notice fail "could not save this Mac's edits (git stash)"
      return 1
    fi
    saved="${saved:+$saved; }edits made here → git stash list"
    echo "  Saved this Mac's edits: git stash list (\"minerctl update $stamp\")"
  fi
  if ! "${G[@]}" checkout -q -f -B stable "$target"; then
    echo "ERROR: could not switch to the release."
    (( on_open )) && update_notice fail "git checkout failed"
    return 1
  fi
  follower_git_config
  "$ROOT/install.sh" --yes || inst=0
  update_notice ok "$head" "$target" "$was_up" "$saved" "$inst"
  echo "Updated to $("${G[@]}" log -1 --format='%h %s'). The miner is stopped: press s in the UI to mine."
  (( inst )) || echo "WARNING: install.sh failed; run ./install.sh in this folder."
  return 0
}

# What every follower runs when it opens. A release that breaks these would strand them all.
release_checks() {
  local f bad=0
  for f in "$ROOT"/install.sh "$ROOT"/bin/*.sh; do
    zsh -n "$f" 2>/dev/null || { echo "    FAIL zsh -n ${f#$ROOT/}"; bad=1; }
  done
  for f in "$ROOT"/bin/*.py; do
    python3 -c 'import ast, sys; ast.parse(open(sys.argv[1], encoding="utf-8").read())' "$f" 2>/dev/null \
      || { echo "    FAIL python syntax ${f#$ROOT/}"; bad=1; }
  done
  python3 "$ROOT/bin/miner-ui.py" --self-test >/dev/null 2>&1 || { echo "    FAIL python3 bin/miner-ui.py --self-test"; bad=1; }
  python3 "$ROOT/bin/fleet.py" --self-test >/dev/null 2>&1 || { echo "    FAIL python3 bin/fleet.py --self-test"; bad=1; }
  python3 "$ROOT/bin/control.py" --self-test >/dev/null 2>&1 || { echo "    FAIL python3 bin/control.py --self-test"; bad=1; }
  (( bad )) || echo "    ok: every script parses; the UI, fleet and control self-tests pass"
  return $bad
}

# The main Mac: commit what changed here, then push main and move GitHub's stable branch to it
# in one atomic push. Followers become that commit the next time XMR Miner opens there.
release_cmd() {
  local yes=0 first=0 msg a changes commits
  local -a G words
  G=(git -C "$ROOT")
  words=()
  for a in "$@"; do
    case "$a" in
      -y|--yes) yes=1 ;;
      *) words+=("$a") ;;
    esac
  done
  msg="${words[*]}"
  read_role "$ROOT"
  if [[ $ROLE != main ]]; then
    echo "This Mac is a follower: releases come from the main Mac."
    echo "(To make this the main Mac instead: ./bin/minerctl.sh role main. Keep only one.)"
    return 1
  fi
  if [[ "$("${G[@]}" symbolic-ref -q --short HEAD)" != main ]]; then
    echo "Releases go out from the main branch; this checkout is on $("${G[@]}" symbolic-ref -q --short HEAD || echo 'a detached HEAD')."
    return 1
  fi
  if ! fetch_refs 0 '+refs/heads/main:refs/remotes/origin/main'; then
    echo "ERROR: could not reach GitHub: $FETCH_ERR"
    return 1
  fi
  if ! fetch_refs 0 '+refs/heads/stable:refs/remotes/origin/stable'; then
    if [[ $FETCH_ERR != *"find remote ref"* ]]; then
      echo "ERROR: could not reach GitHub: $FETCH_ERR"
      return 1
    fi
    first=1
  fi
  if ! "${G[@]}" merge-base --is-ancestor refs/remotes/origin/main HEAD; then
    echo "GitHub's main has commits this Mac does not have:"
    "${G[@]}" log --format='  %h %an: %s' HEAD..refs/remotes/origin/main
    echo "Bring them in first (./bin/minerctl.sh update), then release again."
    return 1
  fi
  changes=$("${G[@]}" status --porcelain)
  commits=""
  (( first )) || commits=$("${G[@]}" log --format='    %h %s' refs/remotes/origin/stable..HEAD)
  if [[ -z $changes && -z $commits ]] && (( ! first )); then
    echo "Nothing to release: the followers already have $("${G[@]}" log -1 --format='%h %s')."
    return 0
  fi
  echo "Release from this Mac:"
  if [[ -n $changes ]]; then
    echo "  To commit:"
    print -r -- "$changes" | sed 's/^/    /'
  fi
  if (( first )); then
    echo "  First release: this makes GitHub's stable branch, which every follower tracks."
  elif [[ -n $commits ]]; then
    echo "  Commits the followers do not have yet:"
    print -r -- "$commits"
  fi
  echo "  Checks:"
  if ! release_checks; then
    echo "Nothing released: fix the failing check first."
    return 1
  fi
  if [[ -n $changes && -z $msg ]]; then
    if [[ -t 0 ]]; then
      printf "Commit message: "
      read -r msg
    fi
    if [[ -z $msg ]]; then
      echo "Nothing released: give the commit a message, e.g. ./bin/minerctl.sh release \"what changed\""
      return 1
    fi
  fi
  if (( ! yes )); then
    if [[ ! -t 0 ]]; then
      echo "Nothing released: add --yes to release without the prompt."
      return 1
    fi
    printf "Push to GitHub and release to the other Macs? [y/N] "
    read -r a
    [[ $a == [yY] ]] || { echo "Cancelled."; return 1; }
  fi
  if [[ -n $changes ]]; then
    "${G[@]}" add -A && "${G[@]}" commit -q -m "$msg" || { echo "ERROR: git commit failed; nothing released."; return 1; }
  fi
  if ! "${G[@]}" push --quiet --atomic origin HEAD:refs/heads/main HEAD:refs/heads/stable; then
    echo "ERROR: the push failed, so nothing was released (GitHub is unchanged)."
    [[ -n $changes ]] && echo "  Your changes are committed here; release again to push them."
    return 1
  fi
  echo "Released $("${G[@]}" log -1 --format='%h %s')"
  echo "Each follower gets it the next time XMR Miner opens there (quit and reopen); its miner stays stopped until s."
  return 0
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
    J=$(api_curl "$API" --max-time 4)
    if [[ -z "$J" ]]; then echo "STARTING"; exit 0; fi
    echo "$J" | python3 -c '
import json,sys
d=json.load(sys.stdin); c=d["connection"]; h=d["hashrate"]["total"][0]
up=d["uptime"]; hrs,rem=divmod(up,3600); mins=rem//60
acc=c["accepted"]; rej=c["rejected"]; pool=c["pool"]
print("RUNNING")
if d.get("paused"):
    print("Speed: paused (keyboard or mouse in use; mines again after the PAUSE idle time)")
else:
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

  job)
    # Render the job file for this Mac (only rewritten when it differs) and show the profile.
    write_job_file "$ROOT" --if-changed || exit 1
    machine_print
    ;;

  perf|threads)
    # More or fewer mining threads: the performance knob. Written to machine.local as THREADS=.
    machine_detect "$ROOT"
    arg="${2:-}"
    if [[ -z $arg ]]; then
      perf_print
      exit 0
    fi
    case "$arg" in
      up|+|more)
        if (( THREADS >= CORES )); then echo "Already at the top: $THREADS of $CORES threads."; exit 0; fi
        spec=$(( THREADS + 1 )) ;;
      down|-|less)
        if (( THREADS <= 1 )); then echo "Already at the bottom: 1 thread."; exit 0; fi
        spec=$(( THREADS - 1 )) ;;
      *)
        spec="$arg" ;;
    esac
    if ! setting_ok THREADS "$spec"; then
      echo "Not a thread setting: $arg"
      echo "Use: perf up | down | max | eco | auto | <1-$CORES> | <1-100>%"
      exit 1
    fi
    old=$THREADS
    if [[ "$spec" == auto ]]; then
      local_unset "$ROOT" THREADS
    else
      local_set "$ROOT" THREADS "$spec"
    fi
    machine_detect "$ROOT"
    echo "Threads: $old → $THREADS of $CORES  (THREADS=$THREADS_SPEC)"
    apply_settings
    ;;

  config)
    sub="${2:-show}"
    (( $# >= 2 )) && shift 2 || shift $#
    case "$sub" in
      show)
        machine_detect "$ROOT"
        machine_print
        echo
        if [[ -f "$ROOT/machine.local" ]] && grep -qvE '^[[:space:]]*(#.*)?$' "$ROOT/machine.local"; then
          echo "machine.local:"
          grep -vE '^[[:space:]]*(#.*)?$' "$ROOT/machine.local" | sed 's/^/  /'
        else
          echo "machine.local: no overrides (every setting is automatic)"
        fi
        problems=$(local_problems "$ROOT")
        if [[ -n $problems ]]; then
          echo "Skipped (invalid):"
          print -r -- "$problems" | sed 's/^/  /'
        fi
        echo
        echo "Edit: ./bin/minerctl.sh config set KEY=value | unset KEY | edit | reset   (config help)"
        ;;
      set)
        if (( $# == 0 )); then config_usage; exit 1; fi
        machine_detect "$ROOT"
        keys=(); vals=(); bad=0
        for kv in "$@"; do
          k="${${kv%%=*}:u}"; v="${kv#*=}"
          if [[ "$kv" != *=* ]] || (( ! ${SETTING_KEYS[(Ie)$k]} )) || ! setting_ok "$k" "$v"; then
            echo "Not a valid setting: $kv   (want $(setting_help "$k"))"
            bad=1
            continue
          fi
          keys+=("$k"); vals+=("$v")
        done
        (( bad )) && { echo "Nothing written."; exit 1; }
        for i in {1..${#keys}}; do
          local_set "$ROOT" "${keys[$i]}" "${vals[$i]}"
          echo "Set ${keys[$i]}=${vals[$i]}"
        done
        apply_settings
        ;;
      unset)
        if (( $# == 0 )); then config_usage; exit 1; fi
        for k in "$@"; do
          k="${k:u}"
          if (( ! ${SETTING_KEYS[(Ie)$k]} )); then echo "Unknown key: $k  (keys: ${SETTING_KEYS[*]})"; exit 1; fi
          local_unset "$ROOT" "$k"
          echo "Unset $k (automatic again)"
        done
        apply_settings
        ;;
      edit)
        machine_detect "$ROOT"
        [[ -f "$ROOT/machine.local" ]] || local_template > "$ROOT/machine.local"
        editor=(${=${VISUAL:-${EDITOR:-nano}}})
        "${editor[@]}" "$ROOT/machine.local" || { echo "Editor exited with an error; nothing applied."; exit 1; }
        [[ "${1:-}" == --no-apply ]] && exit 0  # the UI applies it itself, to follow a restart
        apply_settings
        ;;
      apply)
        apply_settings
        ;;
      reset)
        if [[ -f "$ROOT/machine.local" ]]; then
          echo "Removing machine.local:"
          sed 's/^/  /' "$ROOT/machine.local"
          rm -f "$ROOT/machine.local"
        else
          echo "No machine.local; already automatic."
        fi
        apply_settings
        ;;
      path)
        echo "$ROOT/machine.local"
        ;;
      help|-h|--help)
        config_usage
        ;;
      *)
        echo "Unknown: config $sub"
        config_usage
        exit 1
        ;;
    esac
    ;;

  restart)
    if ! is_up; then echo "Not running."; exit 1; fi
    api_curl "$API" --max-time 2 | note stop --why restart --by "${MINER_BY:-}" --api-stdin
    kill_all
    export MINER_WHY=restart
    exec "$ROOT/bin/minerctl.sh" start
    ;;

  bench)
    # Offline thread sweep: mines nothing, needs every core, so it will not run beside the miner.
    shift
    if is_up; then
      echo "The miner is running. Stop it first (./bin/minerctl.sh stop), then bench again."
      exit 1
    fi
    exec "$ROOT/bin/xmr_bench_sweep.sh" "$@"
    ;;

  help|-h|--help)
    cat <<EOF
Usage: ./bin/minerctl.sh <command>
  status             running or not, speed, shares
  start | stop       mine / stop (stop writes a Desktop summary)
  restart            stop + start, no summary
  perf [how]         threads: up | down | max | eco | auto | <N> | <N>%   (no argument: show)
  config [...]       show or edit this Mac's settings (config help)
  bench [N ...]      offline thread sweep to find the best thread count (miner must be stopped)
  job                render the job file and print this Mac's profile
  nice               try nice -10 on xmrig
  fleet [...]        every Mac on this wallet
  remote start|stop|restart <mac|all>   main Mac: start or stop another Mac (m2, i7, all …)
  remote setup       main Mac: make the signing key (then release, so the others get control.pub)
  events [-n N] [--here] [--all]        timed log: pool errors, rejected shares, starts, stops
  release ["msg"]    main Mac: commit everything here, push, release it to the others (--yes: no prompt)
  update             follower: become the latest release now (opening XMR Miner does it too); miner left stopped
                     main: fast-forward main from GitHub, reinstall, restart if it was running
  role [main|follower]  which this Mac is (unset = follower)
EOF
    ;;

  fleet)
    # Every Mac at once: LAN API (token) + the pool's per-worker view. Read-only.
    shift
    exec python3 "$ROOT/bin/fleet.py" "$@"
    ;;

  remote)
    # The main Mac starts / stops / restarts another Mac through that Mac's control helper.
    shift
    if [[ "${1:-}" == setup ]]; then
      exec python3 "$ROOT/bin/control.py" keygen "${@:2}"
    fi
    exec python3 "$ROOT/bin/control.py" send "$@"
    ;;

  events)
    shift
    exec python3 "$ROOT/bin/control.py" events "$@"
    ;;

  update)
    # follower (the default): become exactly the latest release, reinstall, leave xmrig stopped.
    #   --on-open is bin/miner-ui.sh as the app opens: quiet, 15 s for GitHub, a note for the UI.
    # main: fast-forward main from GitHub, reinstall, restart xmrig if it was running. Opening the
    #   app never updates the main Mac: releases come from it.
    on_open=0
    [[ "${2:-}" == --on-open ]] && on_open=1
    G=(git -C "$ROOT")
    if ! "${G[@]}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      (( on_open )) && exit 0
      echo "ERROR: $ROOT is not a git checkout."
      echo "Clone https://github.com/BuzzScud/M4-MINER-XMR-.git and run ./install.sh there."
      exit 1
    fi
    read_role "$ROOT"
    if [[ $ROLE != main ]]; then
      follower_update "$on_open"
      exit $?
    fi
    (( on_open )) && exit 0
    "${G[@]}" fetch --quiet || { echo "ERROR: git fetch failed (network?)."; exit 1; }
    incoming=$("${G[@]}" log --oneline 'HEAD..@{u}')
    if [[ -z $incoming ]]; then
      echo "Already up to date: $("${G[@]}" log -1 --format='%h %s')"
      exit 0
    fi
    # The generated files are reset before the pull and rebuilt after it; any other local
    # edit stops the update.
    edits=$("${G[@]}" status --porcelain --untracked-files=no -- . "${GENERATED[@]/#/:!}")
    if [[ -n $edits ]]; then
      echo "Local edits would be overwritten:"
      echo "$edits"
      echo "Commit or stash them (git -C \"$ROOT\" stash), then update again."
      exit 1
    fi
    echo "Updating:"
    echo "$incoming"
    was_up=0
    if is_up; then
      was_up=1
      MINER_WHY=update "$ROOT/bin/minerctl.sh" stop || exit 1
    fi
    "${G[@]}" checkout -- "${GENERATED[@]}"
    if ! "${G[@]}" merge --ff-only --quiet '@{u}'; then
      echo "ERROR: cannot fast-forward (local commits?). Resolve with git, then run ./install.sh."
      exit 1
    fi
    "$ROOT/install.sh" --yes || exit 1
    if (( was_up )); then
      export MINER_WHY=update
      exec "$ROOT/bin/minerctl.sh" start
    fi
    ;;

  release)
    shift
    release_cmd "$@"
    exit $?
    ;;

  role)
    G=(git -C "$ROOT")
    if ! "${G[@]}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      echo "ERROR: $ROOT is not a git checkout, so it cannot send or follow releases."
      exit 1
    fi
    case "${2:-}" in
      "")
        read_role "$ROOT"
        if [[ $ROLE == main ]]; then
          echo "main: releases go out from this Mac (./bin/minerctl.sh release); opening XMR Miner never updates it."
        else
          echo "follower: opening XMR Miner makes this Mac the latest release from the main Mac."
        fi
        ;;
      main)
        "${G[@]}" config miner.role main || exit 1
        "${G[@]}" config --unset remote.origin.pushurl 2>/dev/null
        echo "This is the main Mac now: ./bin/minerctl.sh release sends updates to the others. Keep only one."
        ;;
      follower)
        "${G[@]}" config miner.role follower || exit 1
        "${G[@]}" config remote.origin.pushurl "$FOLLOWER_PUSHURL"
        echo "This Mac follows releases now: the next open (or ./bin/minerctl.sh update) makes it the latest one."
        ;;
      *)
        echo "Usage: ./bin/minerctl.sh role [main | follower]"
        exit 1
        ;;
    esac
    ;;

  start)
    if is_up; then
      echo "Already running."
      pids=$(xmrig_pids)
      [[ -n $pids ]] && echo "PID: ${pids//$'\n'/ }"
      if api_up; then
        api_curl "$API" --max-time 2 | python3 -c '
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
    # Job file for this Mac: threads, mode, worker and paths from bin/machine.sh.
    # Silent when nothing changed; a folder copied from another Mac corrects itself here.
    write_job_file "$ROOT" --if-changed || exit 1
    xattr -d com.apple.quarantine "$XMRIG" 2>/dev/null || true
    if ! xmrig_has_arch "$XMRIG" "$(uname -m)"; then
      echo "ERROR: bin/xmrig has no $(uname -m) slice (has: $(lipo -archs "$XMRIG" 2>/dev/null))."
      exit 1
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
    # Own session (setsid, via the system perl): otherwise xmrig shares the Terminal window's
    # process group and closing that window SIGHUPs it. nohup can't help, xmrig traps SIGHUP.
    # perl execs caffeinate in place, so $! is the same pid the pidfile always held.
    /usr/bin/perl -MPOSIX -e 'POSIX::setsid(); exec @ARGV or die "exec: $!\n"' \
      /usr/bin/caffeinate -i "${cmd[@]}" >>"$LOGS/xmrig.err.log" 2>&1 &
    echo $! > "$PIDFILE"
    echo "Started. It takes about a minute to reach full speed."
    note start --why "${MINER_WHY:-cli}" --by "${MINER_BY:-}"
    # This Mac's control helper (not on the main Mac): while xmrig runs, the main Mac can stop it
    # even after the window closes. Detached, so the start does not wait for it.
    python3 "$ROOT/bin/control.py" ensure >/dev/null 2>&1 &!
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
    J=$(api_curl "$API" --max-time 2 || true)
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
    if [[ -n $J ]]; then
      print -r -- "$J" | note stop --why "${MINER_WHY:-cli}" --by "${MINER_BY:-}" --api-stdin
    else
      note stop --why "${MINER_WHY:-cli}" --by "${MINER_BY:-}"
    fi
    echo "Stopped."
    ;;

  *)
    echo "Unknown command: $1   (./bin/minerctl.sh help)"
    exit 1
    ;;

esac
