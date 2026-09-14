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
# (gitignored, next to install.sh), one KEY=value per line:
#   THREADS=8
#   MODE=light
#   WORKER=minerv3-studio
#   LAN=off            keep this Mac's API on 127.0.0.1 (the fleet view cannot see it)
#
# Fleet: with fleet.token present the API listens on the LAN (0.0.0.0:18088),
# needs that token, and reports the worker name instead of the host name.
# Restricted mode stays on: read-only, and /1/config (which holds the wallet) is 403.
# =============================================================================

POOL="gulf.moneroocean.stream:20016"
LABEL="com.minerv3.xmrig"

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

# Sets: ARCH CHIP CORES PHYS PCORE ECORE RAMGB L3 CORES_LABEL THREADS MODE RXINIT WORKER
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

  MODE="fast"
  (( RAMGB < 8 )) && MODE="light"
  RXINIT=$CORES
  WORKER="minerv3-$(chip_slug "$CHIP")-${RAMGB}gb"
  LAN="on"

  # machine.local overrides
  local f="$root/machine.local" line k v
  if [[ -f "$f" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      line="${line%%#*}"
      line="${line//[[:space:]]/}"
      [[ -z "$line" || "$line" != *=* ]] && continue
      k="${line%%=*}"; v="${line#*=}"
      case "$k" in
        THREADS) [[ "$v" == <-> ]] && (( v >= 1 )) && THREADS=$v ;;
        MODE)    [[ "$v" == fast || "$v" == light ]] && MODE=$v ;;
        WORKER)  [[ -n "$v" ]] && WORKER="${v//[^A-Za-z0-9._-]/}" ;;
        LAN)     [[ "$v" == on || "$v" == off ]] && LAN=$v ;;
      esac
    done < "$f"
  fi
  return 0
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
  local root="$1" logs="$1/logs" token_line=""
  if [[ -n "${FLEET_TOKEN:-}" ]]; then
    token_line=$'\n'"        <string>--http-access-token=$FLEET_TOKEN</string>"
  fi
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
        <string>-k</string>
        <string>--tls</string>
        <string>--randomx-mode=$MODE</string>
        <string>--randomx-init=$RXINIT</string>
        <string>--cpu-priority=4</string>
        <string>--threads=$THREADS</string>
        <string>--cpu-no-yield</string>
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
  echo "  Threads: $THREADS"
  echo "  Mode:    $MODE"
  echo "  Worker:  $WORKER"
  echo "  Pool:    $POOL"
  if [[ "$(api_host)" == 0.0.0.0 ]]; then
    echo "  API:     0.0.0.0:18088 (LAN, token from fleet.token)"
  elif [[ -n "${FLEET_TOKEN:-}" ]]; then
    echo "  API:     127.0.0.1:18088 (LAN=off in machine.local)"
  else
    echo "  API:     127.0.0.1:18088 (no fleet.token)"
  fi
}

if [[ "${ZSH_EVAL_CONTEXT:-}" == "toplevel" ]]; then
  set -uo pipefail
  _root="${0:A:h:h}"
  machine_detect "$_root"
  read_fleet_token "$_root"
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
