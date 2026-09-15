#!/bin/zsh
# =============================================================================
# Machine detection — the one place that knows which Mac this is.
#
# Sourced by install.sh, bin/minerctl.sh, bin/xmr_bench_sweep.sh and
# bin/miner-ui.sh. Run it directly to see the profile:   ./bin/machine.sh
#
# Rules
#   Apple Silicon   threads = every core (P + E)        the M4 peaks at 10 = 4P + 6E
#   Intel           threads = min(cores, L3 / 2 MiB)    one RandomX scratchpad per 2 MiB of L3
#   RAM >= 8 GB     fast mode (2 GB dataset)            under 8 GB: light mode (256 MB)
#   worker          minerv3-<chip>-<ram>gb              minerv3-m4-16gb, minerv3-i7-8700b-16gb
#   binary          bin/xmrig is universal (arm64 + x86_64); the kernel picks the slice
#
# The job file (com.minerv3.xmrig.plist) is rendered from these values at
# install and again at every start, so a folder copied to another Mac just
# works. Do not edit the plist by hand; put overrides in machine.local
# (gitignored, next to install.sh), one KEY=value per line, or use
# `minerctl perf` / `minerctl config set` to write them for you:
#   THREADS=8          or auto | max (every logical CPU) | eco (half of auto) | 75% (of logical CPUs)
#   MODE=light         or fast | auto
#   WORKER=minerv3-studio
#   POOL=host:port     default gulf.moneroocean.stream:20016
#   BACKUP=host:port   or off; default de.moneroocean.stream:20016 (same pool and balance, another
#                      server). xmrig moves to it after 5 failed tries on POOL (~25 s) and back to
#                      POOL as soon as it answers. Off by itself when POOL is not a MoneroOcean
#                      server, so earnings never split across two pools.
#   TLS=off            only for a pool port without TLS (applies to POOL and BACKUP)
#   YIELD=on           let other apps have the CPU first (drops --cpu-no-yield; lower H/s)
#   PAUSE=120          pause while the keyboard or mouse is in use, mine again after N s idle
#                      (10-3600, default 120; off mines while you work: --pause-on-active)
#   LAN=off            keep this Mac's API on 127.0.0.1 (the fleet view cannot see it)
#
# Fleet: with fleet.token present the API listens on the LAN (0.0.0.0:18088),
# needs that token, and reports the worker name instead of the host name.
# Restricted mode stays on: read-only, and /1/config (which holds the wallet) is 403.
# =============================================================================

POOL_DEFAULT="gulf.moneroocean.stream:20016"
POOL="$POOL_DEFAULT"
BACKUP_DEFAULT="de.moneroocean.stream:20016"
LABEL="com.minerv3.xmrig"
SETTING_KEYS=(THREADS MODE WORKER POOL BACKUP TLS YIELD PAUSE LAN)

# True when VALUE is a valid machine.local setting for KEY.
setting_ok() {
  local k="$1" v="$2"
  case "$k" in
    THREADS) [[ "$v" == (auto|max|eco) || "$v" == <1-> || "$v" == <1-100>% ]] ;;
    MODE)    [[ "$v" == (fast|light|auto) ]] ;;
    WORKER)  [[ -n "$v" && "$v" != *[^A-Za-z0-9._-]* ]] ;;
    POOL)    [[ "$v" == *:<1-65535> && -n "${v%:*}" && "${v%:*}" != *[^A-Za-z0-9.-]* ]] ;;
    BACKUP)  [[ "$v" == off ]] || setting_ok POOL "$v" ;;
    TLS|YIELD|LAN) [[ "$v" == (on|off) ]] ;;
    PAUSE)   [[ "$v" == off || "$v" == <10-3600> ]] ;;
    *) return 1 ;;
  esac
}

# What each key takes, for error messages.
setting_help() {
  case "$1" in
    THREADS) print -r -- "THREADS=<1-${CORES:-N}> | auto | max | eco | <1-100>%" ;;
    MODE)    print -r -- "MODE=fast | light | auto" ;;
    WORKER)  print -r -- "WORKER=<letters, digits, . _ ->" ;;
    POOL)    print -r -- "POOL=<host>:<port>" ;;
    BACKUP)  print -r -- "BACKUP=<host>:<port> | off" ;;
    TLS)     print -r -- "TLS=on | off" ;;
    YIELD)   print -r -- "YIELD=on | off" ;;
    PAUSE)   print -r -- "PAUSE=<10-3600 seconds idle> | off" ;;
    LAN)     print -r -- "LAN=on | off" ;;
    *)       print -r -- "keys: ${SETTING_KEYS[*]}" ;;
  esac
}

# THREADS spec -> a thread count for this Mac, 1..CORES. Needs CORES and AUTO_THREADS.
resolve_threads() {
  local v="$1" n
  case "$v" in
    auto)  n=$AUTO_THREADS ;;
    max)   n=$CORES ;;
    eco)   n=$(( (AUTO_THREADS + 1) / 2 )) ;;
    <->%)  n=$(( (CORES * ${v%\%} + 99) / 100 )) ;;
    <->)   n=$v ;;
    *)     return 1 ;;
  esac
  (( n < 1 )) && n=1
  (( n > CORES )) && n=$CORES
  print -r -- "$n"
}

# machine.local editing: one KEY=value per line; comments and other keys are kept.
local_get() {
  local f="$1/machine.local" line
  [[ -f "$f" ]] || return 1
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%#*}"
    line="${line//[[:space:]]/}"
    [[ "$line" == "$2="* ]] && { print -r -- "${line#*=}"; return 0; }
  done < "$f"
  return 1
}

local_unset() {
  local f="$1/machine.local" tmp
  [[ -f "$f" ]] || return 0
  tmp=$(mktemp "${TMPDIR:-/tmp}/machinelocal.XXXXXX") || return 1
  grep -vE "^[[:space:]]*$2[[:space:]]*=" "$f" > "$tmp"
  mv -f "$tmp" "$f"
}

local_set() {
  local root="$1" k="$2" v="$3"
  local_unset "$root" "$k" || return 1
  print -r -- "$k=$v" >> "$root/machine.local"
}

# Lines in machine.local that machine_detect ignores, one per line ("line N: text").
local_problems() {
  local f="$1/machine.local" line raw n=0 k v
  [[ -f "$f" ]] || return 0
  while IFS= read -r raw || [[ -n "$raw" ]]; do
    n=$(( n + 1 ))
    line="${raw%%#*}"
    line="${line//[[:space:]]/}"
    [[ -z "$line" ]] && continue
    if [[ "$line" != *=* ]]; then
      print -r -- "line $n: $raw   (not KEY=value)"
      continue
    fi
    k="${line%%=*}"; v="${line#*=}"
    if (( ! ${SETTING_KEYS[(Ie)$k]} )); then
      print -r -- "line $n: $raw   (unknown key; $(setting_help ''))"
    elif ! setting_ok "$k" "$v"; then
      print -r -- "line $n: $raw   (want $(setting_help "$k"))"
    fi
  done < "$f"
}

# "Apple M1 Pro" -> m1-pro     "Intel(R) Core(TM) i7-8700B CPU @ 3.20GHz" -> i7-8700b
chip_slug() {
  local s="${1:l}"
  s="${s//\(r\)/}"
  s="${s//\(tm\)/}"
  local -a words keep
  words=(${=s})
  keep=()
  local w
  for w in "${words[@]}"; do
    case "$w" in
      apple|intel|core|cpu|processor|@|*ghz) continue ;;
    esac
    keep+=("$w")
  done
  s="${(j:-:)keep}"
  s="${s//[^a-z0-9-]/}"
  print -r -- "${s:-cpu}"
}

# Sets: ARCH CHIP CORES PHYS PCORE ECORE RAMGB L3 CORES_LABEL THREADS AUTO_THREADS THREADS_SPEC
#       MODE MODE_SPEC RXINIT WORKER POOL BACKUP TLS YIELD PAUSE LAN ROLE
machine_detect() {
  local root="$1"
  ARCH=$(uname -m)
  CHIP=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown CPU")
  CORES=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 1)
  PHYS=$(sysctl -n hw.physicalcpu 2>/dev/null || echo "$CORES")
  PCORE=$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || echo "")
  ECORE=$(sysctl -n hw.perflevel1.logicalcpu 2>/dev/null || echo "")
  RAMGB=$(( $(sysctl -n hw.memsize 2>/dev/null || echo 0) / 1073741824 ))
  L3=$(sysctl -n hw.l3cachesize 2>/dev/null || echo 0)
  [[ "$L3" == <-> ]] || L3=0

  if [[ -n "$PCORE" && -n "$ECORE" ]]; then
    CORES_LABEL="$CORES logical (${PCORE}P + ${ECORE}E)"
  elif (( L3 > 0 )); then
    CORES_LABEL="$CORES logical, $PHYS physical, L3 $(( L3 / 1048576 )) MB"
  else
    CORES_LABEL="$CORES logical, $PHYS physical"
  fi

  if [[ "$ARCH" == "arm64" ]]; then
    THREADS=$CORES
  elif (( L3 > 0 )); then
    THREADS=$(( L3 / 2097152 ))
    (( THREADS > CORES )) && THREADS=$CORES
    (( THREADS < 1 )) && THREADS=1
  else
    THREADS=$PHYS
  fi

  AUTO_THREADS=$THREADS
  THREADS_SPEC="auto"

  MODE="fast"
  (( RAMGB < 8 )) && MODE="light"
  MODE_SPEC="auto"
  RXINIT=$CORES
  WORKER="minerv3-$(chip_slug "$CHIP")-${RAMGB}gb"
  POOL="$POOL_DEFAULT"
  BACKUP="$BACKUP_DEFAULT"
  TLS="on"
  YIELD="off"
  PAUSE="120"
  LAN="on"

  # machine.local overrides (invalid lines are skipped; `minerctl config` lists them)
  local f="$root/machine.local" line k v backup_set=0
  if [[ -f "$f" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      line="${line%%#*}"
      line="${line//[[:space:]]/}"
      [[ -z "$line" || "$line" != *=* ]] && continue
      k="${line%%=*}"; v="${line#*=}"
      [[ "$k" == WORKER ]] && v="${v//[^A-Za-z0-9._-]/}"
      setting_ok "$k" "$v" || continue
      case "$k" in
        THREADS) THREADS=$(resolve_threads "$v"); THREADS_SPEC=$v ;;
        MODE)    MODE_SPEC=$v; [[ "$v" != auto ]] && MODE=$v ;;
        WORKER)  WORKER=$v ;;
        POOL)    POOL=$v ;;
        BACKUP)  BACKUP=$v; backup_set=1 ;;
        TLS)     TLS=$v ;;
        YIELD)   YIELD=$v ;;
        PAUSE)   PAUSE=$v ;;
        LAN)     LAN=$v ;;
      esac
    done < "$f"
  fi
  # the default backup is a MoneroOcean server: behind another pool it would split the balance
  (( ! backup_set )) && [[ "${POOL%:*}" != *moneroocean.stream ]] && BACKUP=off
  [[ "$BACKUP" == "$POOL" ]] && BACKUP=off
  read_role "$root"
  return 0
}

# Sets ROLE: main (releases go out from this Mac) or follower (the default: opening the app makes
# it the latest release). Kept in this checkout's git config (miner.role), not machine.local, so
# `minerctl config reset` can never turn the main Mac into a follower. No .git = follower, and
# git is not run at all (on a Mac without the developer tools it would pop up an installer).
read_role() {
  ROLE=""
  [[ -e "$1/.git" ]] && ROLE=$(git -C "$1" config --get miner.role 2>/dev/null)
  [[ "$ROLE" == main ]] || ROLE=follower
}

# Sets WALLET from $XMR_WALLET or wallet.local. Prints the fix and returns 1 when missing.
read_wallet() {
  local root="$1" f="$1/wallet.local"
  if [[ -n "${XMR_WALLET:-}" ]]; then
    WALLET="$XMR_WALLET"
  elif [[ -f "$f" ]]; then
    WALLET=$(grep -v '^#' "$f" | grep -v '^[[:space:]]*$' | head -1 | tr -d '[:space:]')
  else
    WALLET=""
  fi
  if [[ -z "$WALLET" || "$WALLET" == YOUR_XMR_ADDRESS ]]; then
    echo "ERROR: public XMR receive address missing."
    echo "  Copy wallet.local.example to wallet.local and put the address in it."
    echo "  Do not put spend or view keys in this folder."
    return 1
  fi
  return 0
}

# Sets FLEET_TOKEN from fleet.token (first line that is not a comment), or "" when absent.
# Only URL-safe characters: the token goes into the plist and an HTTP header unescaped.
read_fleet_token() {
  local f="$1/fleet.token"
  FLEET_TOKEN=""
  [[ -f "$f" ]] || return 0
  FLEET_TOKEN=$(grep -v '^#' "$f" | grep -v '^[[:space:]]*$' | head -1 | tr -d '[:space:]')
  if [[ -n "$FLEET_TOKEN" && "$FLEET_TOKEN" == *[^A-Za-z0-9._~+/=-]* ]]; then
    echo "WARNING: fleet.token has characters outside A-Z a-z 0-9 . _ ~ + / = -; ignoring it (API stays on 127.0.0.1)."
    FLEET_TOKEN=""
  fi
  return 0
}

# 0.0.0.0 only with a token; without one the API never leaves this Mac.
api_host() {
  if [[ "${LAN:-on}" == on && -n "${FLEET_TOKEN:-}" ]]; then
    print -r -- "0.0.0.0"
  else
    print -r -- "127.0.0.1"
  fi
}

# True when bin/xmrig carries a slice for this CPU.
xmrig_has_arch() {
  local bin="$1" arch="$2"
  lipo "$bin" -verify_arch "$arch" >/dev/null 2>&1
}

# The launchd job for this Mac, from the values machine_detect + read_wallet set.
render_job_file() {
  local root="$1" logs="$1/logs" token_line="" tls_line="" backup_lines="" yield_line="" pause_line=""
  if [[ -n "${FLEET_TOKEN:-}" ]]; then
    token_line=$'\n'"        <string>--http-access-token=$FLEET_TOKEN</string>"
  fi
  [[ "${TLS:-on}" == on ]] && tls_line=$'\n'"        <string>--tls</string>"
  # Second pool: -u -p -a -k --tls after an -o belong to that pool only, so it repeats them.
  # xmrig tries POOL 5 times, 5 s apart, then this one; POOL keeps retrying and wins back when it answers.
  if [[ "${BACKUP:-off}" != off ]]; then
    backup_lines=$'\n'"        <string>-o</string><string>$BACKUP</string>"
    backup_lines+=$'\n'"        <string>-u</string><string>$WALLET.$WORKER</string>"
    backup_lines+=$'\n'"        <string>-p</string><string>x</string>"
    backup_lines+=$'\n'"        <string>-a</string><string>rx/0</string>"
    backup_lines+=$'\n'"        <string>-k</string>$tls_line"
  fi
  [[ "${YIELD:-off}" == off ]] && yield_line=$'\n'"        <string>--cpu-no-yield</string>"
  # xmrig polls the keyboard/mouse idle time twice a second; the dataset and pool stay up while paused
  [[ "${PAUSE:-off}" != off ]] && pause_line=$'\n'"        <string>--pause-on-active=$PAUSE</string>"
  cat <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>$LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/bin/caffeinate</string>
        <string>-i</string>
        <string>$root/bin/xmrig</string>
        <string>-o</string><string>$POOL</string>
        <string>-u</string><string>$WALLET.$WORKER</string>
        <string>-p</string><string>x</string>
        <string>-a</string><string>rx/0</string>
        <string>-k</string>$tls_line$backup_lines
        <string>--randomx-mode=$MODE</string>
        <string>--randomx-init=$RXINIT</string>
        <string>--cpu-priority=4</string>
        <string>--threads=$THREADS</string>$yield_line$pause_line
        <string>--http-host=$(api_host)</string>
        <string>--http-port=18088</string>$token_line
        <string>--api-worker-id=$WORKER</string>
        <string>--donate-level=0</string>
        <string>--print-time=30</string>
        <string>--no-color</string>
        <string>--log-file=$logs/xmrig.log</string>
    </array>
    <key>WorkingDirectory</key><string>$root</string>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>ThrottleInterval</key><integer>30</integer>
    <key>ProcessType</key><string>Standard</string>
    <key>LowPriorityIO</key><false/>
    <key>StandardOutPath</key><string>$logs/xmrig.err.log</string>
    <key>StandardErrorPath</key><string>$logs/xmrig.err.log</string>
</dict>
</plist>
PLIST
}

# write_job_file ROOT [--if-changed]
# Detects the machine, reads the wallet, writes ROOT/com.minerv3.xmrig.plist.
# With --if-changed it stays silent and leaves the file alone when nothing differs.
write_job_file() {
  local root="$1" only_if_changed="${2:-}" out="$1/$LABEL.plist" tmp
  machine_detect "$root" || return 1
  read_wallet "$root" || return 1
  read_fleet_token "$root"
  tmp=$(mktemp "${TMPDIR:-/tmp}/minerjob.XXXXXX") || return 1
  render_job_file "$root" > "$tmp"
  if [[ "$only_if_changed" == "--if-changed" && -f "$out" ]] && cmp -s "$tmp" "$out"; then
    rm -f "$tmp"
    return 0
  fi
  if ! plutil -lint "$tmp" >/dev/null 2>&1; then
    rm -f "$tmp"
    echo "ERROR: rendered job file is not a valid plist."
    return 1
  fi
  mv -f "$tmp" "$out" && chmod 644 "$out"
  echo "Job file written for this Mac: $CHIP, $THREADS threads, $MODE mode, worker $WORKER"
}

machine_print() {
  echo "  Arch:    $ARCH"
  echo "  Chip:    $CHIP"
  echo "  Cores:   $CORES_LABEL"
  echo "  Memory:  ${RAMGB} GB"
  local tsrc="auto" msrc="auto"
  [[ "$THREADS_SPEC" != auto ]] && tsrc="THREADS=$THREADS_SPEC in machine.local"
  [[ "$MODE_SPEC" != auto ]] && msrc="MODE=$MODE_SPEC in machine.local"
  echo "  Threads: $THREADS of $CORES logical  (auto is $AUTO_THREADS; now: $tsrc)"
  echo "  Mode:    $MODE  ($msrc)"
  echo "  Worker:  $WORKER"
  echo "  Pool:    $POOL$([[ "$TLS" == on ]] && echo " (TLS)" || echo " (no TLS)")"
  if [[ "$BACKUP" == off ]]; then
    echo "  Backup:  off (one pool only)"
  else
    echo "  Backup:  $BACKUP (after 5 failed tries on the pool; back to the pool when it answers)"
  fi
  echo "  Yield:   $YIELD$([[ "$YIELD" == on ]] && echo " (other apps first; lower H/s)" || echo " (--cpu-no-yield: xmrig keeps its cores)")"
  if [[ "$PAUSE" == off ]]; then
    echo "  Pause:   off (mines while you use this Mac)"
  else
    echo "  Pause:   ${PAUSE} s (pauses while the keyboard or mouse is in use; mines after ${PAUSE} s idle)"
  fi
  if [[ "$(api_host)" == 0.0.0.0 ]]; then
    echo "  API:     0.0.0.0:18088 (LAN, token from fleet.token)"
  elif [[ -n "${FLEET_TOKEN:-}" ]]; then
    echo "  API:     127.0.0.1:18088 (LAN=off in machine.local)"
  else
    echo "  API:     127.0.0.1:18088 (no fleet.token)"
  fi
  if [[ "${ROLE:-follower}" == main ]]; then
    echo "  Role:    main (releases go out from here: ./bin/minerctl.sh release)"
  else
    echo "  Role:    follower (opening XMR Miner makes it the latest release)"
  fi
}

if [[ "${ZSH_EVAL_CONTEXT:-}" == "toplevel" ]]; then
  set -uo pipefail
  _root="${0:A:h:h}"
  machine_detect "$_root"
  read_fleet_token "$_root"
  if [[ "${1:-}" == --env ]]; then
    # machine-readable, for bin/miner-ui.py
    for _k in ARCH CORES PHYS L3 RAMGB AUTO_THREADS THREADS THREADS_SPEC MODE MODE_SPEC WORKER POOL BACKUP TLS YIELD PAUSE LAN; do
      print -r -- "$_k=${(P)_k}"
    done
    exit 0
  fi
  machine_print
  if [[ -f "$_root/machine.local" ]]; then
    echo "  Overrides: machine.local"
  fi
  if xmrig_has_arch "$_root/bin/xmrig" "$ARCH"; then
    echo "  Binary:  bin/xmrig has $ARCH ($(lipo -archs "$_root/bin/xmrig" 2>/dev/null))"
  else
    echo "  Binary:  WARNING bin/xmrig has no $ARCH slice"
  fi
fi
