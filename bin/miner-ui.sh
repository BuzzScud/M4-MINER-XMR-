#!/bin/zsh
# Claude-style miner prompt. Mining starts only on "s".
# Type / and commands appear above the prompt, like Claude Code.
set -uo pipefail
ROOT="${0:A:h:h}"
cd "$ROOT" || exit 1
export MINER_ROOT="$ROOT"
if ! command -v python3 >/dev/null 2>&1; then
  print "python3 is required for the miner UI."
  exit 1
fi
# Render the job file for this Mac before the UI reads it (quiet; "s" reports problems).
"$ROOT/bin/minerctl.sh" job >/dev/null 2>&1 || true
exec python3 "$ROOT/bin/miner-ui.py" "$@"
