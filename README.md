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

Prompt is Claude-style. Type `/` and matching commands appear above the prompt as you type. Mining starts only on `s`.

```
s            start (immediate)
t            stop (writes a summary txt to Desktop)
q            quit UI
/            command palette (filter as you type)
/usage       dashboard: status, speed, shares, pool, machine
/config      threads, mode, pool, worker
/logs        log tab
/err         error log tab
/open        this folder in Finder
/bench       thread sweep (offline, never starts mining)
/flex        pool algo switch (off = rx/0). does not start mining.
/help        command list
esc          close palette / dashboard
```

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
