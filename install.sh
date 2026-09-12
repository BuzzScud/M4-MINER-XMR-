#!/bin/zsh
# =============================================================================
# XMR miner installer — this folder IS the install.
#
# Writes a launchd job for THIS machine and builds XMR Miner.app here.
# Nothing autostarts. Nothing is loaded until you start it.
# =============================================================================
set -uo pipefail

DEST="${0:A:h}"
LOGS="$DEST/logs"
POOL="gulf.moneroocean.stream:20016"
LABEL="com.minerv3.xmrig"
WALLET_FILE="$DEST/wallet.local"
if [[ -n "${XMR_WALLET:-}" ]]; then
  WALLET="$XMR_WALLET"
elif [[ -f $WALLET_FILE ]]; then
  WALLET=$(grep -v '^#' "$WALLET_FILE" | grep -v '^[[:space:]]*$' | head -1 | tr -d '[:space:]')
else
  WALLET=""
fi
if [[ -z $WALLET || $WALLET == YOUR_XMR_ADDRESS ]]; then
  echo "ERROR: public XMR receive address missing."
  echo "  Copy wallet.local.example to wallet.local and put the address in it."
  echo "  Do not put spend or view keys in this folder."
  exit 1
fi

echo
echo "== XMR miner install =="
echo "  Home:    $DEST"

if pgrep -x xmrig >/dev/null 2>&1; then
  echo "ERROR: xmrig is already running. Stop it before installing."
  exit 1
fi

CHIP=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Apple Silicon")
CORES=$(sysctl -n hw.logicalcpu)
PCORE=$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || echo "?")
ECORE=$(sysctl -n hw.perflevel1.logicalcpu 2>/dev/null || echo "?")
RAMGB=$(( $(sysctl -n hw.memsize) / 1073741824 ))
ARCH=$(uname -m)

if [[ "$ARCH" != "arm64" ]]; then
  echo "ERROR: this build is arm64 only (got $ARCH)."
  exit 1
fi

echo "  Chip:    $CHIP"
echo "  Cores:   $CORES logical (${PCORE}P + ${ECORE}E)"
echo "  Memory:  ${RAMGB} GB"
echo

if (( RAMGB < 8 )); then
  echo "  WARNING: under 8 GB. Fast mode needs ~2.1 GB resident; expect stalls."
  echo
fi

THREADS=$CORES
WORKER="minerv3-$(echo "$CHIP" | tr 'A-Z ' 'a-z-' | sed 's/apple-//')-${RAMGB}gb"

echo "  Threads: $THREADS"
echo "  Worker:  $WORKER"
echo "  Pool:    $POOL"
echo "  Wallet:  ${WALLET:0:12}...${WALLET: -6}"
echo
printf "Proceed? [y/N] "
read -r ans
[[ "$ans" == "y" || "$ans" == "Y" ]] || { echo "Cancelled."; exit 0; }

mkdir -p "$DEST/bin" "$LOGS"
chmod +x "$DEST/bin/xmrig" "$DEST/bin/minerctl.sh" "$DEST/bin/xmr_bench_sweep.sh" "$DEST/bin/miner-ui.sh" "$DEST/bin/miner-ui.py" 2>/dev/null

xattr -dr com.apple.quarantine "$DEST/bin/xmrig" 2>/dev/null
codesign --force --sign - "$DEST/bin/xmrig" >/dev/null 2>&1 && echo "  signed xmrig"

cat > "$DEST/$LABEL.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>$LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/bin/caffeinate</string>
        <string>-i</string>
        <string>$DEST/bin/xmrig</string>
        <string>-o</string><string>$POOL</string>
        <string>-u</string><string>$WALLET.$WORKER</string>
        <string>-p</string><string>x</string>
        <string>-a</string><string>rx/0</string>
        <string>-k</string>
        <string>--tls</string>
        <string>--randomx-mode=fast</string>
        <string>--randomx-init=$THREADS</string>
        <string>--cpu-priority=4</string>
        <string>--threads=$THREADS</string>
        <string>--cpu-no-yield</string>
        <string>--http-host=127.0.0.1</string>
        <string>--http-port=18088</string>
        <string>--donate-level=0</string>
        <string>--print-time=30</string>
        <string>--no-color</string>
        <string>--log-file=$LOGS/xmrig.log</string>
    </array>
    <key>WorkingDirectory</key><string>$DEST</string>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>ThrottleInterval</key><integer>30</integer>
    <key>ProcessType</key><string>Standard</string>
    <key>LowPriorityIO</key><false/>
    <key>StandardOutPath</key><string>$LOGS/xmrig.log</string>
    <key>StandardErrorPath</key><string>$LOGS/xmrig.err.log</string>
</dict>
</plist>
PLIST
plutil -lint "$DEST/$LABEL.plist" >/dev/null && echo "  plist written and valid"

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
