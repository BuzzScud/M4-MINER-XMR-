# XMR Miner

This folder **is** the install. XMRig `rx/0` (RandomX), Apple Silicon or Intel, click-to-start, nothing at login.
Machine-aware: `bin/machine.sh` reads the chip, cores, cache and RAM and renders the job file for whichever Mac the
folder is on, at install and again at every start.

Home: wherever this folder lives (this M4: `/Users/christiantavarez/Desktop/PROJECTS/XMR MINER`)

## Folder layout

```
bin/                 xmrig (universal arm64 + x86_64, 6.26.0), machine.sh (chip/threads/mode/worker + the job file),
                     miner-ui.py (the Terminal UI), minerctl.sh (start/stop/status/job/fleet), fleet.py (every Mac
                     at once: LAN API + pool), write-session-summary.py,
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
machine.local        optional tuning overrides for this Mac: THREADS= / MODE= / WORKER= / LAN=off (gitignored)
fleet.token          the API access token every Mac shares (tracked, like wallet.local; see Fleet)
fleet.local          optional extra hosts for the fleet view, one host[:port] per line (gitignored)
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
- Overrides: `machine.local`, one `KEY=value` per line. Do not edit the plist by hand; start re-renders it.
  `minerctl perf` / `minerctl config` and the `/config` card write it for you (see Performance and settings).
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
./bin/machine.sh           # chip, cores, RAM, threads, mode, worker, API bind, binary slices
./bin/minerctl.sh fleet    # every Mac on this wallet (add --watch, --json, -v for addresses)
./bin/minerctl.sh fleet here   # on any Mac: can the others see this one? (bind, token, LAN, firewall)
./bin/minerctl.sh fleet scan   # look for miners on this subnet now
./bin/minerctl.sh update   # git pull from GitHub, re-run install (no prompt), restart xmrig if it was running
./bin/minerctl.sh perf     # threads now; perf up | down | max | eco | auto | 6 | 75%
./bin/minerctl.sh config   # every setting; config set KEY=value | unset KEY | edit | reset | help
./bin/minerctl.sh bench    # offline thread sweep (stop the miner first); prints the perf line to use
./bin/minerctl.sh restart  # stop + start without a Desktop summary
./bin/minerctl.sh help
```

## Performance and settings

Threads are the performance knob. More threads = more CPU; whether that is more H/s depends on the cache:
RandomX keeps a 2 MB scratchpad per thread in L3. Apple Silicon: every core pays off. Intel: past L3 ÷ 2 MB threads
(3 on a 6 MB i7-6700HQ, 6 on a 12 MB i7-8700B) each extra thread mostly adds heat. `minerctl bench` measures it.

Activity Monitor: the CPU Load graph is a share of *every* logical CPU, so 3 threads on an 8-thread i7 peak near
38%, and xmrig's row reads ~300%. That is the setting, not a fault.

```bash
./bin/minerctl.sh perf up        # one more thread   (UI: + on the home screen or the /config card)
./bin/minerctl.sh perf down      # one fewer          (UI: −)
./bin/minerctl.sh perf max       # every logical CPU
./bin/minerctl.sh perf eco       # half of auto (a quieter Mac)
./bin/minerctl.sh perf 75%       # a share of the logical CPUs
./bin/minerctl.sh perf auto      # back to this Mac's rule
./bin/minerctl.sh config set THREADS=4 MODE=fast YIELD=off
./bin/minerctl.sh config edit    # machine.local in $EDITOR (nano), commented template on first use
```

Every change is written to `machine.local`, the job file is re-rendered, and a running xmrig restarts (the dataset
rebuilds, about a minute to full speed; no Desktop summary for a restart). Stopped, it applies on the next start.

| Key       | Values                                               | Default                                 |
|-----------|------------------------------------------------------|-----------------------------------------|
| `THREADS` | `1`…logical CPUs, `auto`, `max`, `eco`, `N%`         | `auto` (Apple: every core; Intel: L3 ÷ 2 MB) |
| `MODE`    | `fast`, `light`, `auto`                              | `auto` (fast with 8 GB+ RAM)            |
| `WORKER`  | letters, digits, `. _ -`                             | `minerv3-<chip>-<ram>gb`                |
| `POOL`    | `host:port`                                          | `gulf.moneroocean.stream:20016`         |
| `TLS`     | `on`, `off`                                          | `on` (set `off` only for a plain port)  |
| `YIELD`   | `on` (other apps first, lower H/s), `off`            | `off` (`--cpu-no-yield`)                |
| `LAN`     | `on`, `off`                                          | `on` (API on the LAN with fleet.token)  |

In the UI: `+` / `−` on the home screen change threads (while mining, presses gather for 1.5 s so one restart
applies them); `/config` shows every setting and takes `+ −` threads, `a` auto, `x` max, `o` eco, `m` fast/light,
`y` yield, `e` edit machine.local. Typed: `/perf 4`, `/perf max`, `/set MODE=light`, `/unset THREADS`.

Live stats: `curl -s -H "Authorization: Bearer $(tail -1 fleet.token)" http://127.0.0.1:18088/2/summary | python3 -m json.tool`

Logs: `logs/xmrig.log` and `logs/xmrig.err.log`

## App / dock

Click **XMR Miner** in the dock (or the `.app` in this folder). It opens Terminal with the miner UI. It does **not** start mining.

The UI is "Rail" (design 2 of `~/Desktop/XMR Miner — TUI Designs v2.html`, built on the Ledger): the left pane is the
Ledger — job card, then a scrollback of `•` bullets with `└` results (start, dataset ready, pool connected, a share ledger
with #, time, diff, latency, ✓/✗, stop, Desktop summary path); `/usage`, `/config`, `/logs` print as cards (`←/→` cycles,
`esc` returns). At 100 columns or more a quiet rail on the right always shows hashrate with a sparkline of the UI's own
1-second samples and the 10s/60s/15m windows, shares with per-minute bars and a next-share estimate, per-thread load from
`/2/backends`, pool, dataset, machine, session, and the fleet (every Mac on this wallet; first to drop when
the window is short). Below 100 columns the rail folds away and the card carries a live `now:`
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
/fleet       card: every Mac on this wallet: state, H/s, shares, uptime, LAN or pool (also /macs)
/config      card: threads, mode, yield, pool, worker, flex, overrides; + − a x o m y e change them
+ / −        one thread more / fewer (restarts xmrig if it is mining)
/perf N      threads: N, up, down, max, eco, auto, 75%      /set KEY=value …   /unset KEY …
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

## Fleet (every Mac, from any one)

Any Mac running this folder can show all of them: `/fleet` in the UI, the fleet section at the foot of the rail, or
`./bin/minerctl.sh fleet`. Two sources, merged by worker name:

- **LAN**: each Mac's own xmrig HTTP API, the one the UI already reads. The job file now binds it to `0.0.0.0:18088`,
  requires the token in `fleet.token`, and sets `--api-worker-id` so it reports `minerv3-m2-8gb`, not the host name.
  It stays in xmrig's restricted mode: read-only, and `/1/config` (the only endpoint that holds the wallet) answers 403.
  Live numbers: 10s/60s/15m, shares, uptime, pool, ping. Polled every 5 s while the UI is open. Nothing extra runs.
- **Pool**: MoneroOcean's per-worker stats for the wallet. About a minute behind, but it works from anywhere, so a Mac
  on another network still shows (as `pool`). Polled every 60 s.

Macs are found without configuration: the fleet view scans this Mac's subnet for :18088 answering with the token and
remembers each one by worker name in `logs/fleet.json`, so a new DHCP address is found again. It rescans at most every
5 minutes, and only when the pool reports a worker the LAN view has not found.

**Set up each other Mac once:** `git pull` in this folder (or copy the folder again), then **t** and **s** in the UI
(or `minerctl stop` then `start`) so xmrig restarts with the LAN job file. `./bin/minerctl.sh fleet here` on that
Mac checks the bind, the token, the LAN address and the macOS firewall. If the firewall is on, click Allow when macOS
asks about xmrig, or run the `socketfilterfw --unblockapp` line `fleet here` prints.

States: `mining` (LAN), `pool` (the pool gets its shares but the LAN API is not visible: not updated yet, another
network, or a firewall; the row says which), `stopped` (the Mac answered, the miner is not running), `offline` (no
answer: asleep, off, away), `no token` (its `fleet.token` differs), `idle` (the pool has had no share for 10 minutes).

- Another network: put the Macs on Tailscale (or ZeroTier) and list their names in `fleet.local`, one `host[:port]` per
  line. Do not port-forward 18088 to the internet.
- Keep one Mac off the LAN: `LAN=off` in its `machine.local` (API back on 127.0.0.1; the pool still shows it).
- `fleet.token` is tracked in git on purpose (private repo) so every Mac shares it with a `git pull`. It only unlocks
  read-only stats. To rotate it: replace the line, commit, pull on every Mac, **t** and **s** on each. Rotate it if the
  repo ever goes public.
- Because it is xmrig's own API, XMRig dashboards and monitors that take a URL plus an access token work as well.

## Other Macs / re-install

Clone the repo or copy the folder, then once per Mac:

```bash
cd "/path/to/XMR MINER"
./install.sh
```

It signs the binary, strips quarantine, writes the job file for that Mac and builds the dock app. Starts nothing.
The job file is re-rendered at every start anyway, so a folder copied from another Mac corrects its own paths,
thread count and worker name the first time you press `s`. On this M4 confirm Threads: 10.

## Updating

On a Mac that cloned the repo, `./bin/minerctl.sh update` brings it up to the latest `main`:
it fetches, lists the incoming commits, stops xmrig if it is running (writing the usual Desktop summary),
fast-forwards, runs `./install.sh --yes` (job file, dock app, signature check), then starts xmrig again if it
was running. `bin/miner.applescript` and `XMR Miner.app` always differ from git because install bakes this folder's
path into them; update resets and rebuilds them. Any other local edit stops the update until you commit or stash it.
A folder copied from a zip is not a checkout: clone the repo instead (or `git init` it with `origin` set to the repo).

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
