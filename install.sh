#!/bin/zsh
# =============================================================================
# XMR miner installer — this folder IS the install.
#
# Signs the binary, writes the launchd job for THIS machine (bin/machine.sh
# decides threads, mode and worker from the chip, cores, cache and RAM) and
# builds XMR Miner.app here. Run it once per Mac.
# Nothing autostarts. Nothing is loaded until you start it.
# --yes skips the prompt (bin/minerctl.sh update runs it that way after a pull).
# =============================================================================
set -uo pipefail

DEST="${0:A:h}"
YES=0
[[ "${1:-}" == --yes || "${1:-}" == -y ]] && YES=1
LOGS="$DEST/logs"
source "$DEST/bin/machine.sh"

read_wallet "$DEST" || exit 1

echo
echo "== XMR miner install =="
echo "  Home:    $DEST"

if pgrep -x xmrig >/dev/null 2>&1; then
  echo "ERROR: xmrig is already running. Stop it before installing."
  exit 1
fi

machine_detect "$DEST"

if ! xmrig_has_arch "$DEST/bin/xmrig" "$ARCH"; then
  echo "ERROR: bin/xmrig has no $ARCH slice (has: $(lipo -archs "$DEST/bin/xmrig" 2>/dev/null))."
  exit 1
fi

echo "  Chip:    $CHIP ($ARCH)"
echo "  Cores:   $CORES_LABEL"
echo "  Memory:  ${RAMGB} GB"
echo

if (( RAMGB < 8 )); then
  echo "  NOTE: under 8 GB, so light mode (256 MB) instead of the 2 GB fast dataset."
  echo
fi

echo "  Threads: $THREADS"
echo "  Mode:    $MODE"
echo "  Worker:  $WORKER"
echo "  Pool:    $POOL"
echo "  Wallet:  ${WALLET:0:12}...${WALLET: -6}"
[[ -f "$DEST/machine.local" ]] && echo "  Overrides: machine.local"
echo
if (( ! YES )); then
  printf "Proceed? [y/N] "
  read -r ans
  [[ "$ans" == "y" || "$ans" == "Y" ]] || { echo "Cancelled."; exit 0; }
fi

mkdir -p "$DEST/bin" "$LOGS"
chmod +x "$DEST/bin/xmrig" "$DEST/bin/machine.sh" "$DEST/bin/minerctl.sh" "$DEST/bin/xmr_bench_sweep.sh" "$DEST/bin/miner-ui.sh" "$DEST/bin/miner-ui.py" 2>/dev/null

xattr -dr com.apple.quarantine "$DEST/bin/xmrig" 2>/dev/null
# Re-sign only when the signature is broken: bin/xmrig is tracked, and re-signing a good one
# changes its bytes, which would block the next `git pull`.
if codesign -v "$DEST/bin/xmrig" >/dev/null 2>&1; then
  echo "  xmrig signature ok"
else
  codesign --force --sign - "$DEST/bin/xmrig" >/dev/null 2>&1 && echo "  signed xmrig"
fi

if write_job_file "$DEST" >/dev/null; then
  echo "  plist written and valid"
else
  echo "ERROR: could not write the job file."
  exit 1
fi

# Dock click opens Terminal with the miner UI. Does not start mining.
cat > "$DEST/bin/miner.applescript" <<APPLESCRIPT
on run
	set ui to "$DEST/bin/miner-ui.sh"
	tell application "Terminal"
		activate
		set t to do script "clear; exec " & quoted form of ui
		try
			set number of columns of front window to 147
			set number of rows of front window to 58
		end try
	end tell
end run
APPLESCRIPT

APP="$DEST/XMR Miner.app"
rm -rf "$APP"
if osacompile -o "$APP" "$DEST/bin/miner.applescript" >/dev/null 2>&1; then
  cp "$DEST/bin/icon.icns" "$APP/Contents/Resources/applet.icns" 2>/dev/null
  codesign --force --deep --sign - "$APP" >/dev/null 2>&1
  touch "$APP"
  echo "  built: $APP"
else
  echo "  WARNING: could not build the app; use bin/minerctl.sh instead"
fi

echo
echo "Done. Nothing is running and nothing starts at login."
echo "  Start:  \"$DEST/bin/minerctl.sh\" start"
echo "  Or:     double-click \"XMR Miner.app\" in this folder"
echo "  Tune:   \"$DEST/bin/xmr_bench_sweep.sh\""
echo
