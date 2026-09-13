# XMR Miner

This folder **is** the install. XMRig `rx/0` (RandomX), Apple Silicon or Intel, click-to-start, nothing at login.
Machine-aware: `bin/machine.sh` reads the chip, cores, cache and RAM and renders the job file for whichever Mac the
folder is on, at install and again at every start.

Home: wherever this folder lives (this M4: `/Users/christiantavarez/Desktop/PROJECTS/XMR MINER`)

## Folder layout

```
bin/                 xmrig (universal arm64 + x86_64, 6.26.0), machine.sh (chip/threads/mode/worker + the job file),
                     miner-ui.py (the Terminal UI), minerctl.sh (start/stop/status/job), write-session-summary.py,
                     xmr_bench_sweep.sh, the dock launcher's AppleScript and icon
XMR Miner.app/       dock launcher: opens Terminal at 147×58 with the UI; never starts mining by itself
logs/                xmrig.log / xmrig.err.log (from --log-file), last-session.json, the pid file   (local only)
docs/setup/          Setup Instructions (html)
docs/randomx/        how RandomX is made, the mining audit, the M4 tune plan, Two Proofs of Work
docs/designs/        the interactive TUI mockup pages the screen was chosen from
lab/                 the RandomX ↔ crystalline validation lab (see lab/README.md); lab/RandomX is a pinned submodule
lab/crystalline-rx-formula/   R = F(K, H) rebuilt from crystalline primitives only; reproduces vector 1a (see its README)
install.sh           signs xmrig, writes the job file for this Mac, rebuilds the dock app; once per Mac; starts nothing
wallet.local         the payout address (public receive address only; tracked in git so every Mac mines to it)
com.minerv3.xmrig.plist(.example)   the job file, rendered per Mac at install and every start (gitignored)
machine.local        optional tuning overrides for this Mac: THREADS= / MODE= / WORKER= (gitignored)
vendor/              local xmrig source checkout (gitignored); XMR-Miner-Portable.zip, bin/xmrig.* extras (gitignored)
```

## Wallet

`wallet.local` holds the **public Monero receive address** and is tracked in git, so every Mac that clones or copies
this folder mines to the same wallet. Only the receive address goes there. Never put spend keys, view keys, or seed
words in this folder. `XMR_WALLET=<address> ./install.sh` overrides it for one machine.

`com.minerv3.xmrig.plist` is gitignored: `bin/machine.sh` renders it from `wallet.local` plus this Mac's hardware.

RandomX lab page: `lab/RandomX-formula-lab.html` (the real one; the original sketch is `lab/RandomX-formula-lab.original.html`)

## Ready (per Mac)

`bin/machine.sh` decides the tuning at install and again at every start; `./bin/machine.sh` prints it.

- Apple Silicon: threads = every core (P + E). This M4: **10** (4P + 6E), the measured max; `--randomx-init=10`.
- Intel: threads = min(cores, L3 ÷ 2 MiB), one RandomX scratchpad per 2 MiB of L3 (a 12 MB i7 gets 6).
- 8 GB or more: `--randomx-mode=fast` (2 GB dataset). Under 8 GB: `light` (256 MB).
- Worker: `minerv3-<chip>-<ram>gb`: `minerv3-m4-16gb`, `minerv3-m2-8gb`, `minerv3-i7-8700b-16gb`.
- Binary: `bin/xmrig` is universal (XMRig 6.26.0, arm64 + x86_64); the kernel runs the native slice. The arm64 slice is
  the build that beat a Clang 17 `-mcpu=native` rebuild on the M4 (4,204 vs 3,874 H/s); the x86_64 slice is the
  official release (checksum verified).
- Overrides: `machine.local` with `THREADS=`, `MODE=fast|light`, `WORKER=` lines. Do not edit the plist by hand;
  start re-renders it.
- `-a rx/0` on every Mac.
- `/flex` in the Terminal UI: pool may pick algo. Default is `rx/0`. Does not start mining.
- **Not mining** until you start it

## Terminal

```bash
cd "/Users/christiantavarez/Desktop/PROJECTS/XMR MINER"
./bin/minerctl.sh status
./bin/minerctl.sh start
./bin/minerctl.sh stop
./bin/minerctl.sh nice     # try nice -10 (sudo -n, or: sudo ./bin/minerctl.sh nice)
./bin/minerctl.sh job      # render the job file for this Mac and print the profile
./bin/machine.sh           # chip, cores, RAM, threads, mode, worker, binary slices
```

Live stats: `curl -s http://127.0.0.1:18088/2/summary | python3 -m json.tool`

Logs: `logs/xmrig.log` and `logs/xmrig.err.log`

## App / dock

Click **XMR Miner** in the dock (or the `.app` in this folder). It opens Terminal with the miner UI. It does **not** start mining.

The UI is "Rail" (design 2 of `~/Desktop/XMR Miner — TUI Designs v2.html`, built on the Ledger): the left pane is the
Ledger — job card, then a scrollback of `•` bullets with `└` results (start, dataset ready, pool connected, a share ledger
with #, time, diff, latency, ✓/✗, stop, Desktop summary path); `/usage`, `/config`, `/logs` print as cards (`←/→` cycles,
`esc` returns). At 100 columns or more a quiet rail on the right always shows hashrate with a sparkline of the UI's own
1-second samples and the 10s/60s/15m windows, shares with per-minute bars and a next-share estimate, per-thread load from
`/2/backends`, pool, dataset, machine and session. Below 100 columns the rail folds away and the card carries a live `now:`
row instead; at 24 rows or fewer the card collapses to 3 rows. The only motion is the shimmer on "Mining". Mining starts
only on `s`. Colors are the "Material" palette (9 of `~/Desktop/XMR Miner — Rail Palettes.html`): Google blue for
actions, a lighter blue for data, Google green/red/yellow only for state, on a #202124 ground — the UI asks Terminal for
that ground with OSC 11 on start and restores your profile on exit. The dock launcher opens 147×58.

The job file passes `--log-file=logs/xmrig.log`, so xmrig is the only writer of that file (stdout/stderr go to
`logs/xmrig.err.log`). The ledger takes share latency, dataset time, allocation and pool connect errors from it, and `/logs`
shows it. Pool-fail bullets quote the latest `connect error` / `DNS error` line when the log has one. `s` will not start a
second copy if a leftover xmrig is still on :18088 — it attaches instead; `t` kills every xmrig, not just the pidfile.
`--cpu-priority=4` is a no-op on macOS without root (xmrig wants nice -10). After start, `minerctl` tries
`renice -10` then `sudo -n renice -10`; the rail shows the live nice. Passwordless sudo for `/usr/bin/renice` is optional.

```
s            start (immediate)
t            stop (writes a summary txt to Desktop)
q / ⌃C       quit UI (does not stop a running miner)
/            command palette: grouped (recent · miner · actions · ui), ↑↓ pick, 1–9 run,
             tab complete, ↵ run, esc close; each row shows what it would find right now
/usage       card: hashrate, windows, shares, cadence, threads, dataset, pool, machine, uptime
/config      card: threads, mode, pool, worker, flex, job file
/logs        tail -n 20 of xmrig.log (e switches to the error log)
/err         tail -n 20 of xmrig.err.log
/open        this folder in Finder
/bench       thread sweep (offline, never starts mining; asks for "yes"; refuses while mining)
/flex        pool algo switch (off = rx/0). does not start mining.
             the last 3 commands used are remembered in logs/last-session.json (not /help, /quit)
/help        command list
esc          close palette / card
```

Checks that touch no miner: `python3 bin/miner-ui.py --self-test` and
`python3 bin/miner-ui.py --dump home|home-stopped|home-starting|slash|usage|config|logs|confirm [cols rows] [--plain]`.

If the dock icon is a question mark, drag `XMR Miner.app` from this folder onto the dock.

## Other Macs / re-install

Clone the repo or copy the folder, then once per Mac:

```bash
cd "/path/to/XMR MINER"
./install.sh
```

It signs the binary, strips quarantine, writes the job file for that Mac and builds the dock app. Starts nothing.
The job file is re-rendered at every start anyway, so a folder copied from another Mac corrects its own paths,
thread count and worker name the first time you press `s`. On this M4 confirm Threads: 10.

## Rules

- One xmrig only.
- Same 13 RandomX steps — do not rewrite them.
- macOS has no 1 GB hugepages. Keep ~3 GiB free so the 2 GB dataset is not compressed.
- If H/s drops, check compressor / a second miner, not the hash.

## Lab and design pages

- `lab/` — the RandomX ↔ crystalline validation lab (see `lab/README.md`): the crystalline abacus as the arithmetic engine under
  test against tevador's reference (`lab/RandomX`, a pinned submodule; `git submodule update --init`), the ALU + self-test,
  the hook script that routes every RandomX op through it, the traces for all six official vectors, and the formula-lab page.
- `docs/designs/` — the interactive TUI mockup pages the screen was chosen from.
