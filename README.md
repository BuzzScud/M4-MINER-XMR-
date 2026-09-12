# XMR Miner

This folder **is** the install. XMRig `rx/0` (RandomX), Apple Silicon, click-to-start, nothing at login.

Home: `/Users/christiantavarez/Desktop/PROJECTS/XMR MINER`

## Wallet (keep private files local)

Git does **not** carry the payout address or any keys.

1. `cp wallet.local.example wallet.local`
2. Put only the **public Monero receive address** in `wallet.local`
3. Never put spend keys, view keys, or seed words in this folder

`wallet.local` and `com.minerv3.xmrig.plist` are gitignored. `./install.sh` reads `wallet.local` and writes the plist.

RandomX lab page: `RandomX-formula-lab.html`

## Ready (this M4)

- `--threads=10` `--randomx-init=10` `--randomx-mode=fast` `-a rx/0`
- Worker `minerv3-m4-16gb`
- Threads: **10** (max H/s on this M4)
- Binary: XMRig 6.26.0 arm64 AES (stock beat a Clang 17 `-mcpu=native` rebuild: 4,204 vs 3,874 H/s)
- `/flex` in the Terminal UI: pool may pick algo. Default is `rx/0`. Does not start mining.
- **Not mining** until you start it

## Terminal

```bash
cd "/Users/christiantavarez/Desktop/PROJECTS/XMR MINER"
./bin/minerctl.sh status
./bin/minerctl.sh start
./bin/minerctl.sh stop
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

The job file passes `--log-file=logs/xmrig.log`, so xmrig writes its own log: the ledger takes share latency, dataset time and
allocation from it, and `/logs` shows it. Before the first restart with that flag the ledger counts shares from the API instead
and marks the latency column `~` (it is the pool ping at that moment).

```
s            start (immediate)
t            stop (writes a summary txt to Desktop)
q / ⌃C       quit UI (does not stop a running miner)
/            command palette (filter as you type, ↑↓ pick, tab complete, ↵ run)
/usage       card: hashrate, windows, shares, cadence, threads, dataset, pool, machine, uptime
/config      card: threads, mode, pool, worker, flex, job file
/logs        tail -n 20 of xmrig.log (e switches to the error log)
/err         tail -n 20 of xmrig.err.log
/open        this folder in Finder
/bench       thread sweep (offline, never starts mining; asks for "yes")
/flex        pool algo switch (off = rx/0). does not start mining.
/help        command list
esc          close palette / card
```

Checks that touch no miner: `python3 bin/miner-ui.py --self-test` and
`python3 bin/miner-ui.py --dump home|home-stopped|home-starting|slash|usage|config|logs|confirm [cols rows] [--plain]`.

If the dock icon is a question mark, drag `XMR Miner.app` from this folder onto the dock.

## Re-install / refresh plist

```bash
cd "/Users/christiantavarez/Desktop/PROJECTS/XMR MINER"
./install.sh
```

Starts nothing. Confirm Threads: 10.

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
