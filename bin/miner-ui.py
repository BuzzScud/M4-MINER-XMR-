#!/usr/bin/env python3
"""Ledger-style miner TUI (Codex CLI lineage). Scrollback-first and quiet.

A small job card at the top, then plain bullets with └ results, each with its time. Shares are
kept as a ledger (#, time, diff, latency, verdict); every rejected share and pool error gets its
own timed line. /usage, /config and /logs print cards. The only motion is a shimmer across
"Mining". Mining starts only on s, or on a signed command from the main Mac (bin/control.py).
"""
from __future__ import annotations

import glob
import json
import os
import plistlib
import queue
import re
import select
import shutil
import signal
import subprocess
import sys
import termios
import threading
import time
import tty
import urllib.request
from dataclasses import dataclass, field
from typing import Optional

import fleet  # bin/fleet.py: the API token, and every Mac at once
import control  # bin/control.py: commands between the Macs, and the timed event log

ROOT = os.environ.get("MINER_ROOT") or os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def is_main_mac() -> bool:
    """git config miner.role == main: the Mac that sends releases (minerctl role). Unset = follower."""
    try:
        out = subprocess.run(["git", "-C", ROOT, "config", "--get", "miner.role"], capture_output=True, text=True, timeout=2)
        return out.stdout.strip() == "main"
    except Exception:
        return False
CTL = os.environ.get("MINER_CTL") or os.path.join(ROOT, "bin", "minerctl.sh")  # MINER_CTL: a stub for tests
LOG = os.path.join(ROOT, "logs", "xmrig.log")
ERR = os.path.join(ROOT, "logs", "xmrig.err.log")
PLIST = os.path.join(ROOT, "com.minerv3.xmrig.plist")
ARCH = os.uname().machine  # arm64 or x86_64; bin/xmrig is universal
API = "http://127.0.0.1:18088/2/summary"
API_BACKENDS = "http://127.0.0.1:18088/2/backends"
PEAK_HS = 4204.0
SNAP = os.path.join(ROOT, "logs", "last-session.json")
NOTICE = os.path.join(ROOT, "logs", "update-notice.json")  # left by `minerctl update` for the next window
DATASET_S = 7.0  # RandomX fast-mode dataset init on this M4 (xmrig: "dataset ready (6620 ms)")

# ---- palette: xterm-256, Codex CLI lineage — your terminal's ink, one cyan, green for credits
def fg(n: int) -> str:
    return f"\033[38;5;{n}m"


# "Material" palette (design 9 of ~/Desktop/XMR Miner — Rail Palettes.html): Google dark ground #202124,
# Google blue for actions, a lighter blue for data, the four brand tones only for state.
INK = fg(253)      # body text
SEC = fg(246)      # secondary text, labels
FAINT = fg(242)    # card borders, empty bar cells
CYAN = fg(33)      # accent: prompt mark, commands, paths (Google blue)
DATA = fg(39)      # data: sparkline, bars
GOOD = fg(41)      # accepted (Google green)
BAD = fg(203)      # rejected (Google red)
WARN = fg(220)     # warnings (Google yellow)
BRIGHT = fg(231)   # shimmer peak
MID = fg(250)      # shimmer shoulder
RULE = fg(240)     # hairline beside the rail
BAND_BG = "\033[48;5;237m"
GROUND = "#202124"  # asked of the terminal with OSC 11 on start, reset with OSC 111 on exit
RESET = "\033[0m"
BOLD = "\033[1m"
NOBOLD = "\033[22m"
UNDER = "\033[4m"
NOUNDER = "\033[24m"
HIDE = "\033[?25l"
SHOW = "\033[?25h"
HOME = "\033[H"
SYNC_BEGIN = "\033[?2026h"
SYNC_END = "\033[?2026l"
ALT_ON = "\033[?1049h"
ALT_OFF = "\033[?1049l"
WRAP_OFF = "\033[?7l"
WRAP_ON = "\033[?7h"
CLEAR = "\033[H\033[2J"


@dataclass(frozen=True)
class Cmd:
    name: str
    desc: str
    action: str
    group: str = "miner"


COMMANDS = (
    Cmd("/usage", "Status, speed, shares, pool", "usage", "miner"),
    Cmd("/fleet", "Every Mac: LAN API + pool", "fleet", "miner"),
    Cmd("/config","Threads (+/−), mode, pool, worker", "config", "miner"),
    Cmd("/logs", "Last 20 lines of xmrig.log", "logs", "miner"),
    Cmd("/err", "Last 20 lines of the error log", "err", "miner"),
    Cmd("/open", "Open this folder in Finder", "open", "actions"),
    Cmd("/bench", "Offline thread sweep, ~10 min", "bench", "actions"),
    Cmd("/flex", "Pool picks the algo (or rx/0)", "flex", "actions"),
    Cmd("/pause", "Pause while you use the Mac: on/off", "pause", "actions"),
    Cmd("/start", "Start here, or: /start m2, all", "start", "fleet"),
    Cmd("/stop", "Stop here, or: /stop m2, all", "stop", "fleet"),
    Cmd("/events", "Pool errors, rejects, stops", "events", "fleet"),
    Cmd("/help", "List commands", "help", "ui"),
    Cmd("/quit", "Quit; the miner keeps running", "quit", "ui"),
)
GROUPS = ("miner", "actions", "fleet", "ui")
NOT_RECENT = {"help", "quit"}
RECENT_N = 3

ALIASES = {
    "/stats": "usage",
    "/status": "usage",
    "/plist": "config",
    "/perf": "config",
    "/threads": "config",
    "/performance": "config",
    "/settings": "config",
    "/set": "config",
    "/unset": "config",
    "/error": "err",
    "/refresh": "usage",
    "/macs": "fleet",
    "/machines": "fleet",
    "fleet": "fleet",
    "usage": "usage",
    "status": "usage",
    "logs": "logs",
    "err": "err",
    "events": "events",
    "/restart": "restart",
    "plist": "config",
    "help": "help",
    "?": "help",
}

TABS = ("Usage", "Fleet", "Config", "Logs")
TYPED = ("/perf", "/threads", "/set", "/unset", "/pause", "/start", "/stop", "/restart")  # take arguments: /perf 4, /stop m2
LOG_VIEWS = ("log", "err", "events")  # e on the Logs card steps through these
PERF_DELAY = 1.5  # + / − while mining: presses gather this long, then one restart applies them


_ANSI = re.compile(r"\033\[[0-9;?]*[A-Za-z]")


def vis_len(s: str) -> int:
    return len(_ANSI.sub("", s or ""))


def plain(s: str) -> str:
    return _ANSI.sub("", s or "")


def trunc(s: Optional[str], n: int) -> str:
    """Clip to n visible cells. Keeps ANSI when it fits; drops it when it has to cut."""
    s = "-" if s is None else str(s)
    if n <= 0:
        return ""
    if vis_len(s) <= n:
        return s
    if n == 1:
        return "…"
    return plain(s)[: n - 1] + "…"


def clip_row(s: Optional[str], width: int) -> str:
    """Exactly `width` visible cells, padded with spaces on the right."""
    s = trunc(s, width)
    pad = width - vis_len(s)
    return s + " " * pad if pad > 0 else s


def short_path(path: str, n: int) -> str:
    home = os.path.expanduser("~")
    if path.startswith(home):
        path = "~" + path[len(home) :]
    return trunc(path, n)


def fit_row(left: str, right: str, width: int) -> str:
    """Left text, right-aligned text, one visible row of exactly `width`."""
    lv, rv = vis_len(left), vis_len(right)
    if rv == 0:
        return clip_row(left, width)
    gap = width - lv - rv
    if gap < 1:
        keep = max(8, width - rv - 1)
        left = trunc(plain(left), keep)
        lv = vis_len(left)
        gap = max(1, width - lv - rv)
    return clip_row(left + " " * gap + right, width)


def parse_job(path: str = PLIST) -> dict:
    out = {
        "threads": "-",
        "mode": "-",
        "init": "-",
        "algo": "rx/0",
        "pool": "-",
        "pool_host": "-",
        "backup": "",
        "worker": "-",
        "user": "-",
        "http": "18088",
        "cwd": ROOT,
        "donate": "0",
        "cmdline": "",
    }
    try:
        p = plistlib.load(open(path, "rb"))
    except Exception:
        return out
    args = [str(a) for a in (p.get("ProgramArguments") or [])]
    # first value wins: a backup pool repeats -o -u -p -a after the main pool's
    d: dict[str, str] = {}
    pools: list[str] = []
    i = 0
    while i < len(args):
        a = args[i]
        if a.startswith("-") and "=" in a:
            k, v = a.split("=", 1)
            d.setdefault(k, v)
            i += 1
        elif a.startswith("-") and i + 1 < len(args) and not str(args[i + 1]).startswith("-"):
            d.setdefault(a, args[i + 1])
            if a == "-o":
                pools.append(args[i + 1])
            i += 2
        else:
            i += 1
    user = d.get("-u", "")
    worker = user.rsplit(".", 1)[-1] if "." in user else ""  # a bare -u is the wallet, never a worker name
    pool = d.get("-o", "-")
    # the command as minerctl runs it, reduced to what decides the hash: algo, threads, mode
    show = ["xmrig"]
    if "-a" in d:
        show += ["-a", d["-a"]]
    for k in ("--threads", "--randomx-mode"):
        if k in d:
            show.append(f"{k}={d[k]}")
    out.update(
        {
            "threads": d.get("--threads", "-"),
            "mode": d.get("--randomx-mode", "-"),
            "init": d.get("--randomx-init", "-"),
            "algo": d.get("-a", "rx/0"),
            "pool": pool,
            "pool_host": pool.split(":")[0] if pool else "-",
            "backup": pools[1] if len(pools) > 1 else "",
            "worker": worker or "-",
            "user": user or "-",
            "http": d.get("--http-port", "18088"),
            "http_host": d.get("--http-host", "127.0.0.1"),
            "token": bool(d.get("--http-access-token")),
            "cwd": p.get("WorkingDirectory") or ROOT,
            "donate": d.get("--donate-level", "0"),
            "tls": "--tls" in args,
            "cmdline": "caffeinate -i " + " ".join(show),
        }
    )
    return out


def tls_label(conn: Optional[dict], job: Optional[dict] = None) -> str:
    """API tls is empty while reconnecting; the job file still has --tls."""
    tls = (conn or {}).get("tls")
    if isinstance(tls, str) and tls.strip():
        return tls.strip()
    if tls:
        return "TLS"
    if (job or {}).get("tls"):
        return "TLS"
    return "plain"


def on_backup(conn: Optional[dict], job: Optional[dict]) -> bool:
    """True while xmrig mines on the job file's second pool (the main one failed 5 times)."""
    backup = (job or {}).get("backup")
    return bool(backup) and (conn or {}).get("pool") == backup


FLEET_TOKEN = fleet.read_token()


def api_get(url: str) -> Optional[dict]:
    """The local API. With fleet.token it wants the token; a pre-token xmrig is retried bare."""
    d, st = fleet.api_json(url, FLEET_TOKEN, 1.2)
    return d if st == "ok" else None


def api_summary() -> Optional[dict]:
    return api_get(API)


def api_thread_rates() -> list[float]:
    """Per-thread 10 s hashrate from /2/backends (cpu backend)."""
    try:
        data = api_get(API_BACKENDS) or []
        for b in data:
            if b.get("type") == "cpu":
                out = []
                for t in b.get("threads") or []:
                    hs = (t.get("hashrate") or [None])[0]
                    out.append(float(hs) if hs is not None else 0.0)
                return out
    except Exception:
        pass
    return []


def ctl_status() -> tuple[str, list[str]]:
    try:
        raw = subprocess.check_output([CTL, "status"], text=True, timeout=4)
    except Exception:
        return "STOPPED", []
    lines = [ln.rstrip() for ln in raw.splitlines() if ln.strip()]
    if not lines:
        return "STOPPED", []
    return lines[0].strip(), lines[1:]


def machine_env() -> dict:
    """This Mac's settings as the next start will use them (bin/machine.sh --env, machine.local applied)."""
    try:
        raw = subprocess.check_output([os.path.join(ROOT, "bin", "machine.sh"), "--env"], text=True, timeout=4)
    except Exception:
        return {}
    return dict(ln.split("=", 1) for ln in raw.splitlines() if "=" in ln)


def local_overrides() -> list[str]:
    """KEY=value lines of machine.local, comments dropped."""
    try:
        lines = open(os.path.join(ROOT, "machine.local")).read().splitlines()
    except OSError:
        return []
    out = [ln.split("#", 1)[0].replace(" ", "").replace("\t", "") for ln in lines]
    return [ln for ln in out if ln]


def typed_hint(buf: str) -> str:
    """What ↵ does with a command that takes arguments (the palette has no row for it)."""
    p = buf.split()
    if not p or p[0] not in TYPED:
        return ""
    if p[0] in ("/perf", "/threads"):
        if len(p) > 1:
            return f"↵ threads → {p[1]}"
        return "↵ opens /config · or /perf up, down, max, eco, auto, 4, 75%"
    if p[0] == "/pause":
        if len(p) > 1:
            v = pause_value(p[1])
            return f"↵ PAUSE={v} and apply it" if v else "/pause on | off | 10-3600 (seconds idle before mining again)"
        return "↵ switches it on/off · or /pause on, off, 300 (seconds idle)"
    if p[0] in ("/start", "/stop", "/restart"):
        verb = p[0][1:]
        if len(p) > 1:
            return f"↵ {verb} {' '.join(p[1:])}" + ("" if verb == "start" else " (asks first)")
        return f"↵ {verb} this Mac · or /{verb} m2, /{verb} all"
    if p[0] == "/set":
        return "↵ writes machine.local and applies it" if len(p) > 1 else "/set KEY=value … (THREADS MODE WORKER POOL BACKUP TLS YIELD PAUSE LAN)"
    return "↵ back to automatic" if len(p) > 1 else "/unset KEY … (back to automatic)"


def session_snapshot(live: dict) -> dict:
    api = live.get("api") or {}
    job = live.get("job") or {}
    hs = api.get("hashrate") or {}
    tot = hs.get("total") or []
    conn = api.get("connection") or {}
    return {
        "saved_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "state": live.get("state"),
        "hs": live.get("hs"),
        "hs10": tot[0] if len(tot) > 0 else live.get("hs"),
        "hs60": tot[1] if len(tot) > 1 else None,
        "hs15": tot[2] if len(tot) > 2 else None,
        "highest": live.get("highest"),
        "acc": live.get("acc") or 0,
        "rej": live.get("rej") or 0,
        "up": live.get("up") or 0,
        "algo": live.get("algo") or job.get("algo"),
        "pool": conn.get("pool") or job.get("pool") or job.get("pool_host"),
        "worker": job.get("worker") if job.get("worker") not in ("", "-", None) else None,
        "ping": conn.get("ping"),
        "failures": conn.get("failures"),
        "version": api.get("version"),
        "threads": job.get("threads"),
        "mode": job.get("mode"),
        "hugepages": live.get("hugepages"),
    }


def save_session_snapshot(live: dict) -> None:
    try:
        os.makedirs(os.path.dirname(SNAP), exist_ok=True)
        tmp = SNAP + ".tmp"
        snap = session_snapshot(live)
        old = load_snapshot() or {}
        if old.get("recent"):
            snap["recent"] = old["recent"]
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(snap, f, indent=2)
        os.replace(tmp, SNAP)
    except Exception:
        pass


def load_recent() -> list[str]:
    snap = load_snapshot() or {}
    r = snap.get("recent") or []
    return [a for a in r if isinstance(a, str)][:RECENT_N]


def save_recent(actions: list[str]) -> None:
    """Keep the recent list inside logs/last-session.json (the summary script ignores extra keys)."""
    try:
        snap = load_snapshot() or {}
        snap["recent"] = actions[:RECENT_N]
        os.makedirs(os.path.dirname(SNAP), exist_ok=True)
        tmp = SNAP + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(snap, f, indent=2)
        os.replace(tmp, SNAP)
    except Exception:
        pass


def load_snapshot() -> Optional[dict]:
    try:
        with open(SNAP, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def take_update_notice(path: str = NOTICE) -> Optional[dict]:
    """The note the last `minerctl update` left, once: reading it deletes it."""
    try:
        with open(path, encoding="utf-8") as f:
            n = json.load(f)
    except Exception:
        return None
    try:
        os.remove(path)
    except OSError:
        pass
    return n if isinstance(n, dict) else None


def update_event(n: dict) -> Optional[tuple]:
    """(kind, fields) for the ledger: the release that came in, or why the check failed."""
    if n.get("status") == "fail":
        return "warn", {"title": "Could not check for an update",
                        "sub": f"{n.get('reason') or 'git fetch failed'} · still on {n.get('at') or '?'}"}
    if n.get("status") != "ok":
        return None
    cnt = int(n.get("count") or 0)
    lines = [f"{BOLD}Updated{NOBOLD} to release {n.get('to') or '?'}"
             + (f"{SEC} · {cnt} new commit{'' if cnt == 1 else 's'}{INK}" if cnt else "")]
    lines += [f"{SEC}{s}{INK}" for s in (n.get("commits") or [])[:3]]
    if cnt > 3:
        lines.append(f"{SEC}+{cnt - 3} more{INK}")
    if n.get("was_up"):
        lines.append(f"{WARN}mining stopped for the update · s starts it again{INK}")
    if n.get("saved"):
        lines.append(f"{SEC}saved first: {n['saved']}{INK}")
    if n.get("install_ok") is False:
        lines.append(f"{WARN}install.sh failed: run ./install.sh in this folder{INK}")
    return "out", {"lines": lines}


def latest_summary_path() -> Optional[str]:
    try:
        files = glob.glob(os.path.expanduser("~/Desktop/XMR-miner-summary-*.txt"))
        return max(files, key=os.path.getmtime) if files else None
    except Exception:
        return None


def xmrig_up() -> bool:
    """True if an xmrig process exists, or something is still listening on the API port.

    A leftover miner can hold :18088 after pgrep misses it (renamed binary, race).
    Starting a second copy then makes the UI attach to the old process's 17h API.
    """
    try:
        r = subprocess.run(
            ["pgrep", "-x", "xmrig"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=1,
        )
        if r.returncode == 0:
            return True
    except Exception:
        pass
    try:
        r = subprocess.run(
            ["lsof", "-nP", "-iTCP:18088", "-sTCP:LISTEN"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=1,
        )
        return r.returncode == 0
    except Exception:
        return False


_CPU_INFO: Optional[tuple[str, str]] = None


def cpu_info() -> tuple[str, str]:
    global _CPU_INFO
    if _CPU_INFO is not None:
        return _CPU_INFO

    def sysctl(k: str) -> str:
        try:
            return subprocess.check_output(["sysctl", "-n", k], text=True, timeout=1).strip()
        except Exception:
            return ""

    brand = sysctl("machdep.cpu.brand_string") or "Apple Silicon"
    try:
        ram = int(sysctl("hw.memsize") or "0") // 1073741824
        ram_s = f"{ram} GB" if ram else "-"
    except Exception:
        ram_s = "-"
    _CPU_INFO = (brand, ram_s)
    return _CPU_INFO


_CORES: Optional[str] = None


def core_layout() -> str:
    """'4P + 6E' from sysctl, or '' when the machine does not say."""
    global _CORES
    if _CORES is None:
        try:
            p = subprocess.check_output(["sysctl", "-n", "hw.perflevel0.physicalcpu"], text=True, timeout=1).strip()
            e = subprocess.check_output(["sysctl", "-n", "hw.perflevel1.physicalcpu"], text=True, timeout=1).strip()
            _CORES = f"{p}P + {e}E" if p and e else ""
        except Exception:
            _CORES = ""
    return _CORES


_BRAILLE_BITS = ((0x40, 0x04, 0x02, 0x01), (0x80, 0x20, 0x10, 0x08))  # per column, bottom dot to top dot


def live_state(api: Optional[dict], up: bool) -> tuple:
    """(state, 10 s H/s, paused) from /2/summary (None when the API is down) and whether xmrig runs.
    An API with no 10 s rate is the dataset build, unless xmrig says it is paused (PAUSE)."""
    paused = bool(api and api.get("paused"))
    if not api:
        return ("STARTING" if up else "STOPPED"), None, False
    tot = (api.get("hashrate") or {}).get("total") or [None]
    hs = tot[0] if tot else None
    return ("RUNNING" if (hs is not None or paused) else "STARTING"), hs, paused


def pause_label(env: dict) -> str:
    """PAUSE from machine.sh --env in words: 120 -> "2 min", 90 -> "90 s", off or missing -> ""."""
    v = str(env.get("PAUSE") or "")
    if not v.isdigit():
        return ""
    n = int(v)
    return f"{n // 60} min" if n % 60 == 0 else f"{n} s"


def pause_value(arg: str) -> Optional[str]:
    """/pause argument -> the PAUSE value machine.sh takes: on -> 120, off, 10-3600; None when invalid."""
    a = arg.strip().lower()
    if a in ("on", "off"):
        return "120" if a == "on" else "off"
    return a if a.isdigit() and 10 <= int(a) <= 3600 else None


def braille_rows(vals: list, w: int, h: int, lo: float, hi: float) -> list[str]:
    """A w×h cell sparkline of `vals` (2 samples per column, 4 dots per row), newest at the right."""
    need = w * 2
    v = list(vals)[-need:]
    off = need - len(v)
    dots = h * 4
    span = max(1e-9, float(hi) - float(lo))
    lev = []
    for i in range(need):
        j = i - off
        if j < 0 or v[j] is None:
            lev.append(0)
        else:
            x = max(float(lo), min(float(hi), float(v[j])))
            lev.append(max(1, round((x - lo) / span * dots)))
    rows = []
    for r in range(h):
        from_bottom = (h - 1 - r) * 4
        s = ""
        for c in range(w):
            b = 0
            for k in (0, 1):
                for d in range(4):
                    if from_bottom + d < lev[c * 2 + k]:
                        b |= _BRAILLE_BITS[k][d]
            s += chr(0x2800 + b) if b else " "
        rows.append(s)
    return rows


LEVELS = "▁▂▃▄▅▆▇█"


def level_char(frac: float) -> str:
    frac = max(0.0, min(1.0, float(frac or 0.0)))
    return LEVELS[max(0, min(7, int(round(frac * 8)) - 1))]


def bar(frac: float, width: int, color: str = DATA) -> str:
    """Codex /status-style bracket bar: [████░░░░]."""
    frac = max(0.0, min(1.0, float(frac or 0.0)))
    n = int(round(frac * width))
    n = max(0, min(width, n))
    return f"{SEC}[{color}{'█' * n}{FAINT}{'░' * (width - n)}{SEC}]{INK}"


def filter_cmds(buf: str) -> list[Cmd]:
    if not buf.startswith("/"):
        return []
    q = buf[1:].lower()
    if q == "":
        return list(COMMANDS)
    pref, sub = [], []
    for c in COMMANDS:
        name = c.name[1:].lower()
        if name.startswith(q):
            pref.append(c)
        elif q in name or q in c.desc.lower():
            sub.append(c)
    return pref + sub


def resolve_action(text: str, selected: Optional[Cmd]) -> Optional[str]:
    t = (text or "").strip()
    if t in ALIASES:
        return ALIASES[t]
    for c in COMMANDS:
        if t == c.name:
            return c.action
    if selected and t.startswith("/"):
        return selected.action
    return None


def fmt_uptime(seconds: int) -> str:
    seconds = max(0, int(seconds))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h {m}m"
    if m:
        return f"{m}m {s}s"
    return f"{s}s"


def fmt_clock(seconds: int) -> str:
    seconds = max(0, int(seconds))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h}h {m:02d}m {s:02d}s"


def fmt_hs(v) -> str:
    try:
        if v is None:
            return "—"
        return f"{float(v):,.0f} H/s"
    except (TypeError, ValueError):
        return "—"


def fmt_n(v) -> str:
    try:
        return f"{int(v):,}"
    except (TypeError, ValueError):
        return "—"


def fmt_hugepages(val) -> str:
    """XMRig v1 API uses a bool; /2/summary uses [allocated, total]."""
    if val is True:
        return "yes"
    if val is False:
        return "no"
    if isinstance(val, (list, tuple)):
        if len(val) >= 2:
            try:
                a, t = int(val[0]), int(val[1])
            except (TypeError, ValueError):
                return "—"
            if t > 0:
                return f"{a}/{t} ({100.0 * a / t:.0f}%)"
            return f"{a}/{t}"
        if len(val) == 1:
            return fmt_hugepages(val[0])
        return "—"
    if val is None:
        return "—"
    return str(val)


def fmt_ping(val) -> str:
    if val is None or val == "":
        return "—"
    try:
        return f"{int(val)} ms"
    except (TypeError, ValueError):
        return str(val)


def hms(ts: float) -> str:
    return time.strftime("%H:%M:%S", time.localtime(ts))


def read_tail(path: str, n: int = 20) -> list[str]:
    if not os.path.isfile(path):
        return []
    try:
        with open(path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(0, size - 64_000), os.SEEK_SET)
            data = f.read().decode("utf-8", "replace")
        return data.splitlines()[-n:]
    except Exception as e:
        return [f"(could not read) {e}"]


_LOG_TS = re.compile(r"^\[(\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)\.(\d{3})\]\s+(\S+)\s+(.*)$")
_SHARE = re.compile(r'^(accepted|rejected) \((\d+)/(\d+)\) diff (\d+)(?: "([^"]*)")? \((\d+) ms\)')
_DSREADY = re.compile(r"dataset ready \((\d+) ms\)")
_ALLOC = re.compile(r"allocated (\d+) MB \((\d+)\+(\d+)\) huge pages (\d+)% (\d+)/(\d+)")
_ERR_QUOTED = re.compile(r'(connect error|DNS error|read error|write error):\s+"([^"]+)"', re.I)


def parse_log(lines: list[str]) -> dict:
    """What the ledger wants from xmrig's own log (--log-file): shares with real latency,
    dataset timing and allocation, the pool line, the latest speed line, connect errors."""
    out: dict = {
        "shares": [],
        "dataset_ms": None,
        "dataset_ts": None,
        "alloc": None,
        "pool": None,
        "speed": None,
        "errors": [],
    }
    for raw in lines:
        ln = _ANSI.sub("", raw or "")
        m = _LOG_TS.match(ln)
        if not m:
            if "address already in use" in ln.lower():
                out["errors"].append({"ts": 0.0, "kind": "bind", "msg": "HTTP API port already in use (another xmrig is running)"})
            continue
        y, mo, d, h, mi, s, ms3 = (int(x) for x in m.groups()[:7])
        try:
            ts = time.mktime((y, mo, d, h, mi, s, 0, 0, -1)) + ms3 / 1000.0
        except (OverflowError, ValueError):
            continue
        msg = m.group(9).strip()
        sm = _SHARE.match(msg)
        if sm:
            out["shares"].append({"n": int(sm.group(2)), "rej": int(sm.group(3)), "ts": ts, "diff": int(sm.group(4)),
                                  "ms": int(sm.group(6)), "ok": sm.group(1) == "accepted", "why": sm.group(5) or "", "src": "log"})
            continue
        dm = _DSREADY.search(msg)
        if dm:
            out["dataset_ms"], out["dataset_ts"] = int(dm.group(1)), ts
            continue
        am = _ALLOC.search(msg)
        if am:
            out["alloc"] = {"mb": int(am.group(1)), "dataset": int(am.group(2)), "cache": int(am.group(3)), "hp": [int(am.group(5)), int(am.group(6))]}
            continue
        if msg.startswith("use pool "):
            out["pool"] = msg[9:].split()[0]
        elif msg.startswith("speed "):
            out["speed"] = msg
        em = _ERR_QUOTED.search(msg)
        if em:
            kind = em.group(1).split()[0].lower()  # connect / DNS / read / write
            out["errors"].append({"ts": ts, "kind": kind, "msg": em.group(2)})
        elif "address already in use" in msg.lower():
            out["errors"].append({"ts": ts, "kind": "bind", "msg": "HTTP API port already in use (another xmrig is running)"})
    return out


def last_error_msg(logd: dict) -> str:
    """Human line for the latest xmrig log error, or '' if the log has none."""
    errs = logd.get("errors") or []
    if not errs:
        return ""
    e = errs[-1]
    kind = e.get("kind") or "error"
    msg = e.get("msg") or ""
    if kind == "connect":
        return f'xmrig: connect error "{msg}"'
    if kind == "dns":
        return f'xmrig: DNS error "{msg}"'
    if kind in ("read", "write"):
        return f'xmrig: {kind} error "{msg}"'
    return msg


@dataclass
class App:
    cols: int = 80
    rows: int = 24
    buf: str = ""
    sel: int = 0
    mode: str = "home"  # home | overlay | confirm
    tab: int = 0  # Usage
    log_which: str = "log"
    confirm_buf: str = ""
    events: list = field(default_factory=list)
    share_rows: list = field(default_factory=list)
    last_acc: Optional[int] = None
    last_rej: Optional[int] = None
    last_fail: Optional[int] = None
    prev_state: Optional[str] = None
    start_ts: Optional[float] = None
    ds_ready_s: Optional[float] = None
    frame_no: int = 0
    recent: Optional[list] = None
    lw: int = 80  # width of the left pane (== cols when the rail is folded)
    hist: list = field(default_factory=list)  # 10 s hashrate, one sample per poll
    thr_cache: Optional[tuple] = None
    nice_cache: Optional[tuple] = None
    prev_paused: Optional[bool] = None
    live_cache: Optional[tuple] = None
    cursor: Optional[tuple] = None
    started: float = field(default_factory=time.time)
    last_draw: float = 0.0
    last_frame: str = ""
    fd: int = 0
    old_tty: Optional[list] = None
    dump: bool = False
    force_palette: Optional[str] = None
    fixed_live: Optional[dict] = None
    _resized: bool = False
    last_resize: float = 0.0
    _winch_r: Optional[int] = None
    _clear_next: bool = False
    fleet_fixed: Optional[tuple] = None  # canned (rows, meta) for --dump and the self-test
    fleet_w: Optional[object] = None     # fleet.Watcher, started on the first frame
    perf_target: Optional[int] = None    # + / − pending while mining (applied after PERF_DELAY)
    perf_at: float = 0.0
    env_cache: Optional[tuple] = None    # (ts, machine_env())
    env_fixed: Optional[dict] = None     # canned machine_env() for --dump and the self-test
    main: bool = field(default_factory=is_main_mac)  # main Mac: rail adds the wallet balance + per-Mac shares
    ctl_note: str = ""                   # the last settings change, shown on the /config card
    fleet_pick: str = ""                 # the Mac picked on the Fleet card (↑↓); '' = this Mac
    ask: Optional[dict] = None           # a y / n question: {"cmd", "rows", "note", "text", "back"}
    remote_q: object = field(default_factory=queue.Queue)  # (ledger event, answer) from commands sent to other Macs
    feed_w: Optional[object] = None      # control.FeedWatcher: every Mac's timed events (this Mac's on a follower)
    feed_fixed: Optional[list] = None    # canned events for --dump and the self-test
    feed_seq: int = 0                    # the last Feed change booked in the ledger
    feed_rows: dict = field(default_factory=dict)  # event id -> ledger event: a growing outage updates in place
    inbox_at: float = 0.0
    feed_at: float = 0.0
    rej_seen: set = field(default_factory=set)     # (ts, share #) of rejects already in the ledger
    prev_backup: Optional[bool] = None
    key_fixed: Optional[bool] = None     # --dump and the self-test: pretend the signing key is here

    # ------------------------------------------------------------------ plumbing
    def tty_size(self) -> tuple[int, int]:
        """Kernel winsize for the output tty. Ignores $COLUMNS/$LINES so a drag is visible."""
        fds: list[int] = []
        if self.fd:
            fds.append(self.fd)
        try:
            if sys.stdout.isatty():
                fds.append(sys.stdout.fileno())
        except Exception:
            pass
        for fd in fds:
            try:
                s = os.get_terminal_size(fd)
                return max(1, int(s.columns)), max(1, int(s.lines))
            except Exception:
                continue
        try:
            s = shutil.get_terminal_size((self.cols or 80, self.rows or 24))
            return max(1, int(s.columns)), max(1, int(s.lines))
        except Exception:
            return max(1, self.cols or 80), max(1, self.rows or 24)

    def size(self) -> bool:
        """Update cols/rows from the tty. True when the window actually changed."""
        cols, rows = self.tty_size()
        changed = (cols, rows) != (self.cols, self.rows)
        if changed:
            self.cols, self.rows = cols, rows
            self.last_resize = time.time()
            self._clear_next = True
        return changed

    def matches(self) -> list[Cmd]:
        src = self.force_palette if self.force_palette is not None else self.buf
        return filter_cmds(src)

    def palette_query(self) -> str:
        return self.force_palette if self.force_palette is not None else self.buf

    def recent_cmds(self) -> list[Cmd]:
        if self.recent is None:
            self.recent = [] if self.dump else load_recent()
        by_action = {c.action: c for c in COMMANDS}
        return [by_action[a] for a in self.recent if a in by_action]

    def palette_lines(self, with_headers: bool = True) -> list[tuple]:
        """Rows of the palette as ('h', title) headers and ('c', Cmd, ordinal) commands, in display order.
        Bare '/' shows recent + the three groups; anything typed after it is a flat ranked filter."""
        q = self.palette_query()
        ms = filter_cmds(q)
        lines: list[tuple] = []
        n = 0
        if q == "/":
            rec = self.recent_cmds()
            if rec and with_headers:
                lines.append(("h", "recent"))
            for c in rec:
                n += 1
                lines.append(("c", c, n))
            for g in GROUPS:
                cmds = [c for c in COMMANDS if c.group == g and c not in rec]
                if not cmds:
                    continue
                if with_headers:
                    lines.append(("h", g))
                for c in cmds:
                    n += 1
                    lines.append(("c", c, n))
        else:
            for c in ms:
                n += 1
                lines.append(("c", c, n))
        return lines

    def visible_cmds(self) -> list[Cmd]:
        return [ln[1] for ln in self.palette_lines(False) if ln[0] == "c"]

    def selected(self) -> Optional[Cmd]:
        ms = self.visible_cmds()
        if not ms:
            return None
        i = max(0, min(self.sel, len(ms) - 1))
        return ms[i]

    def note_recent(self, action: str) -> None:
        if action in NOT_RECENT:
            return
        rec = [a for a in (self.recent or []) if a != action]
        self.recent = [action] + rec
        self.recent = self.recent[:RECENT_N]
        if not self.dump:
            save_recent(self.recent)

    def cmd_status(self, c: Cmd, live: dict, compact: bool = False) -> str:
        """What the command would find right now — so often you need not open it."""
        state = live["state"]
        run = state == "RUNNING"
        a = c.action
        if a == "usage":
            if run:
                return f"{fmt_hs(live['hs'])} · {fmt_n(live['acc'])} ✓" + ("" if compact else f" · {fmt_uptime(live['up'])}")
            return "building dataset" if state == "STARTING" else "not mining"
        if a == "fleet":
            fs = self.fleet_latest()
            if not fs:
                return "looking for Macs…"
            m = fs[1]
            if compact:
                return f"{m.get('mining', 0)}/{m.get('macs', 0)} · {fmt_hs(m.get('total'))}"
            return f"{m.get('mining', 0)} of {m.get('macs', 0)} mining · {fmt_hs(m.get('total'))}"
        if a == "config":
            j = live["job"]
            return f"{j.get('algo', '-')} {j.get('mode', '-')} · {j.get('threads', '-')} threads"
        if a in ("logs", "err"):
            path = LOG if a == "logs" else ERR
            tail = read_tail(path, 20)
            if not tail:
                return "empty"
            last = tail[-1]
            m = _LOG_TS.match(last)
            when = f"{m.group(4)}:{m.group(5)}:{m.group(6)} " if m else ""
            what = (m.group(9).strip() if m else last).split(" ")[0]
            return f"{len(tail)} lines · {when.strip()}" if compact else f"{len(tail)} lines · last {when}{what}"
        if a == "open":
            return short_path(ROOT, 24 if compact else 28)
        if a == "bench":
            return "running · will refuse" if state != "STOPPED" else "~10 min · mines nothing"
        if a == "flex":
            return "on · pool picks the algo" if os.path.isfile(os.path.join(ROOT, "flex.on")) else "off · rx/0 only"
        if a == "pause":
            idle = pause_label(self.env())
            return f"on · {idle} idle" if idle else "off · mines while you work"
        if a == "quit":
            return "xmrig keeps running" if state != "STOPPED" else ""
        if a in ("start", "stop"):
            fs = self.fleet_latest()
            here = "mining here" if state != "STOPPED" else "not mining here"
            if not fs or compact:
                return here
            return f"{here} · {fs[1].get('mining', 0)} of {fs[1].get('macs', 0)} mining"
        if a == "events":
            evs = self.feed_latest(1)
            if not evs:
                return "no events yet"
            e = evs[-1]
            return f"{control.when(e['ts'])} {control.describe(e)[1]}"
        return ""

    def live(self) -> dict:
        if self.fixed_live is not None:
            return self.fixed_live
        now = time.time()
        if self.live_cache and now - self.live_cache[0] < 0.9:
            return self.live_cache[1]
        job = parse_job()
        api = api_summary()
        state, hs, paused = live_state(api, bool(api) or xmrig_up())
        highest = PEAK_HS
        acc = rej = 0
        up = 0
        algo = job.get("algo") or "rx/0"
        hugepages = None
        if api:
            try:
                highest = max(float((api.get("hashrate") or {}).get("highest") or 0), PEAK_HS)
            except Exception:
                pass
            conn = api.get("connection") or {}
            res = api.get("results") or {}
            acc = int(conn.get("accepted") or res.get("shares_good") or 0)
            total_sh = int(res.get("shares_total") or acc)
            rej = max(0, total_sh - acc) if total_sh else int(conn.get("rejected") or 0)
            if "rejected" in conn:
                rej = int(conn.get("rejected") or 0)
            up = int(api.get("uptime") or conn.get("uptime") or 0)
            algo = api.get("algo") or algo
            hugepages = api.get("hugepages")
        live = {
            "state": state,
            "paused": paused,
            "job": job,
            "api": api,
            "hs": hs,
            "highest": highest,
            "acc": acc,
            "rej": rej,
            "up": up,
            "algo": algo,
            "hugepages": hugepages,
            "log": parse_log(read_tail(LOG, 400) + read_tail(ERR, 200)),
        }
        # STARTING has zeros; writing that over last-session.json made a later
        # leftover look like a 0-share session. Only persist a live RUNNING sample.
        if not self.dump and state == "RUNNING":
            save_session_snapshot(live)
        self.live_cache = (now, live)
        if state == "RUNNING":
            self.hist.append(None if paused else hs)
            self.hist = self.hist[-240:]
        self.track(live, now)
        return live

    def thread_rates(self) -> list[float]:
        """Per-thread 10 s rates, refreshed every ~3 s (one more local HTTP call)."""
        if self.dump:
            return []
        now = time.time()
        if self.thr_cache and now - self.thr_cache[0] < 3.0:
            return self.thr_cache[1]
        rates = api_thread_rates()
        self.thr_cache = (now, rates)
        return rates

    def process_nice(self) -> Optional[int]:
        """Current xmrig nice value, or None if it is not running. Cached ~3 s."""
        if self.dump:
            return None
        now = time.time()
        if self.nice_cache and now - self.nice_cache[0] < 3.0:
            return self.nice_cache[1]
        ni: Optional[int] = None
        try:
            raw = subprocess.check_output(["pgrep", "-x", "xmrig"], text=True, timeout=1)
            pid = next((p for p in raw.split() if p.isdigit()), None)
            if pid:
                s = subprocess.check_output(["ps", "-o", "ni=", "-p", pid], text=True, timeout=1).strip()
                ni = int(s)
        except Exception:
            ni = None
        self.nice_cache = (now, ni)
        return ni

    def fleet_latest(self) -> Optional[tuple]:
        """(rows, meta) for every Mac, from a background thread (LAN 5 s, pool 60 s). Never blocks."""
        if self.fleet_fixed is not None:
            return self.fleet_fixed
        if self.dump:
            return None
        if self.fleet_w is None:
            try:
                self.fleet_w = fleet.Watcher()
            except Exception:
                return None
        return self.fleet_w.latest()  # type: ignore[union-attr]

    def fleet_rows(self) -> list:
        fs = self.fleet_latest()
        return list(fs[0]) if fs else []

    def feed(self) -> Optional[object]:
        """control.FeedWatcher, started on first use: every Mac's events on the main Mac, this Mac's elsewhere."""
        if self.dump:
            return None
        if self.feed_w is None:
            try:
                # rows from the fleet poller if it runs; never start a second one from this thread
                rows = lambda: list((self.fleet_w.latest() or ([], {}))[0]) if self.fleet_w is not None else []  # noqa: E731
                self.feed_w = control.FeedWatcher(rows, remote=self.main)
            except Exception:
                return None
        return self.feed_w

    def feed_latest(self, n: int) -> list:
        if self.feed_fixed is not None:
            return [e for e in self.feed_fixed if control.describe(e)[0] != "hide"][-n:]
        fw = self.feed()
        return fw.feed.latest(n) if fw else []  # type: ignore[union-attr]

    def pending_cmd(self, worker: str) -> Optional[str]:
        """The command sent to that Mac that has not answered yet, if any."""
        for e in reversed(self.events):
            if e["k"] == "remote" and e.get("worker") == worker:
                return e["cmd"] if e.get("state") == "pending" else None
        return None

    # ------------------------------------------------------------------ ledger model
    def add(self, kind: str, **kw) -> dict:
        e = {"k": kind, "ts": kw.pop("ts", time.time())}
        e.update(kw)
        self.events.append(e)
        if len(self.events) > 400:
            del self.events[: len(self.events) - 400]
        return e

    def drop(self, *kinds: str) -> None:
        self.events = [e for e in self.events if e["k"] not in kinds]

    def has(self, kind: str) -> bool:
        return any(e["k"] == kind for e in self.events)

    def seed(self, live: dict) -> None:
        """First frame: describe what is already true when the UI opens."""
        now = time.time()
        state = live["state"]
        if state in ("RUNNING", "STARTING"):
            st = now - (live["up"] or 0)
            self.start_ts = st
            self.add("start", ts=st, text="xmrig was already running when this window opened", cmd=live["job"].get("cmdline", ""))
            if state == "RUNNING":
                self.add("dataset", ts=st, secs=None)
                self.add("pool", ts=st)
                self.add("shares", ts=st)
            else:
                self.add("dsprog", ts=st)
        else:
            snap = load_snapshot()
            if snap:
                self.add("laststop", ts=now, snap=snap)
                p = latest_summary_path()
                if p:
                    self.add("summary", ts=now, path=p)
        note = None if self.dump else take_update_notice()
        ev = update_event(note) if note else None
        if ev:
            self.add(ev[0], ts=now, **ev[1])
        self.prev_state = state

    def track(self, live: dict, now: float) -> None:
        """Turn polled state into ledger events: dataset ready, pool up, shares, failures, exits."""
        if self.prev_state is None:
            self.seed(live)
        state = live["state"]
        api = live.get("api") or {}
        conn = api.get("connection") or {}
        res = api.get("results") or {}
        prev = self.prev_state
        if state == "STOPPED" and prev == "STARTING" and self.start_ts and (now - self.start_ts) < 60:
            # just launched: API is down for a bit. Do not treat that as "exited
            # outside this window" or the last-session snapshot (5m, 8 shares)
            # gets pasted onto a start that is still coming up.
            return
        if state == "RUNNING" and prev != "RUNNING":
            up = int(live.get("up") or 0)
            leftover = self.start_ts is not None and (now - self.start_ts) < 60 and up > 120
            if leftover:
                self.add(
                    "warn",
                    title="This is a leftover xmrig, not a new session",
                    sub=f"already up {fmt_uptime(up)} · {fmt_n(live.get('acc'))} shares · press t to stop it",
                )
                self.start_ts = now - up
            elif self.start_ts and prev == "STARTING":
                self.ds_ready_s = max(0.0, now - self.start_ts)
            self.drop("dsprog")
            if not self.has("dataset"):
                self.add("dataset", secs=self.ds_ready_s)
            if not self.has("pool"):
                self.add("pool")
            if not self.has("shares"):
                self.add("shares")
        if state == "STOPPED" and prev in ("RUNNING", "STARTING"):
            # stopped outside this UI (or the process died): say so from the last snapshot
            self.drop("dsprog", "shares")
            if not (self.events and self.events[-1]["k"] in ("stop", "summary")):
                snap = load_snapshot() or {}
                self.add("stop", up=snap.get("up") or live.get("up") or 0, acc=snap.get("acc") or 0,
                         rej=snap.get("rej") or 0, avg=snap.get("hs15") or snap.get("hs60") or snap.get("hs10"),
                         text="xmrig exited outside this window")
        paused = bool(live.get("paused")) if state == "RUNNING" else None
        if paused is not None and self.prev_paused is not None and paused != self.prev_paused:
            # one line for the latest switch: an idle Mac flips this every time you walk away
            self.drop("pause", "resume")
            self.add("pause" if paused else "resume")
        self.prev_paused = paused
        if state == "RUNNING":
            acc, rej = live["acc"], live["rej"]
            since = (self.start_ts or 0) - 2
            from_log = [r for r in (live.get("log") or {}).get("shares", []) if r["ts"] >= since]
            if from_log:
                # xmrig's own log has the real per-share latency: it wins over API counting
                self.share_rows = from_log[-60:]
                for r in from_log:
                    # every rejected share gets its own timed line; the 4-row share ledger scrolls on
                    if not r["ok"] and (r["ts"], r["n"]) not in self.rej_seen:
                        self.rej_seen.add((r["ts"], r["n"]))
                        self.add("reject", ts=r["ts"], n=r["n"] + r.get("rej", 0), diff=r["diff"], ms=r["ms"], why=r.get("why") or "")
            else:
                if self.last_acc is not None and acc > self.last_acc:
                    for n in range(self.last_acc + 1, acc + 1):
                        self.share_rows.append({"n": n, "ts": now, "diff": res.get("diff_current"), "ms": conn.get("ping"), "ok": True, "src": "api"})
                if self.last_rej is not None and rej > self.last_rej:
                    self.share_rows.append({"n": acc, "ts": now, "diff": res.get("diff_current"), "ms": conn.get("ping"), "ok": False, "src": "api"})
                    self.add("reject", ts=now, n=acc + rej, diff=res.get("diff_current"), ms=conn.get("ping"), why="", approx=True)
                self.share_rows = self.share_rows[-60:]
            backup = on_backup(conn, live["job"])
            if self.prev_backup is not None and backup != self.prev_backup:
                self.add("backup" if backup else "mainpool", pool=conn.get("pool") or "")
            self.prev_backup = backup
            self.last_acc, self.last_rej = acc, rej
            fails = conn.get("failures")
            try:
                fails = int(fails) if fails is not None else None
            except (TypeError, ValueError):
                fails = None
            if fails is not None:
                if self.last_fail is not None and fails > self.last_fail:
                    why = last_error_msg(live.get("log") or {})
                    errs = (live.get("log") or {}).get("errors") or []
                    ets = errs[-1]["ts"] if errs and 0 <= now - errs[-1]["ts"] < 120 else now  # the log's own time
                    sub = why or "xmrig reconnects on its own; shares in flight may be lost — /logs for the reason"
                    self.add("warn", ts=ets, title=f"Pool connection failed ({fails} so far this session)", sub=sub)
                self.last_fail = fails
        else:
            self.last_acc = self.last_rej = None
            self.prev_backup = None
        self.prev_state = state

    # ------------------------------------------------------------------ rows
    def band(self, text: str) -> str:
        """A pane-wide row on the input-band background. Only fg codes inside, one reset at the end."""
        return BAND_BG + clip_row(text, self.lw) + RESET

    def r(self, text: str = "") -> str:
        return clip_row(text, self.lw)

    RAIL_W = 34

    def rail_on(self) -> bool:
        return self.cols >= 100

    def rail_rows(self, live: dict, avail: int) -> list[str]:
        """The telemetry rail: a column of small ledgers. Row 0 is blank; sections drop from the bottom when short."""
        W = self.RAIL_W
        state = live["state"]
        run = state == "RUNNING"
        starting = state == "STARTING"
        api = live.get("api") or {}
        conn = api.get("connection") or {}
        res = api.get("results") or {}
        tot = (api.get("hashrate") or {}).get("total") or []
        job = live["job"]
        peak = live["highest"] or PEAK_HS
        snap = None if (run or starting or self.dump) else load_snapshot()
        now = time.time()

        def lab(text: str, right: str = "") -> str:
            return fit_row(f"{SEC}{text}{INK}", f"{FAINT}{right}{INK}" if right else "", W)

        R: list[str] = [""]
        # hashrate
        paused = run and live.get("paused")
        R.append(lab("hashrate", f"{live['hs'] / peak * 100:.0f}% of peak" if run and live["hs"] and not paused else ""))
        if paused:
            R.append(f"{WARN}{BOLD}paused{NOBOLD}{SEC} · you're active{INK}")
        else:
            R.append(f"{BOLD}{fmt_hs(live['hs'])}{NOBOLD}" if run else f"{SEC}{'warming up' if starting else 'not mining'}{INK}")
        spark = braille_rows(self.hist, W, 4, 3600, max(4250, peak)) if self.hist else [" " * W] * 3 + ["⣀" * W]
        R += [f"{DATA if run else FAINT}{s}{INK}" for s in spark]
        if paused:
            R.append(f"{SEC}mines after {pause_label(self.env()) or 'the idle time'} without input{INK}")
        else:
            R.append(f"{SEC}10s {fmt_n(tot[0] if tot else None)} · 60s {fmt_n(tot[1] if len(tot) > 1 else None)} · 15m {fmt_n(tot[2] if len(tot) > 2 else None)}{INK}" if run
                     else f"{SEC}peak {fmt_n(peak)} H/s (sweep){INK}")
        # shares
        R.append("")
        R.append(lab("shares", f"diff {res.get('diff_current', 0) // 1000}k" if run and res.get("diff_current") else ""))
        if run:
            R.append(f"{GOOD}{BOLD}{fmt_n(live['acc'])} ✓{NOBOLD}{INK}  {BAD if live['rej'] else SEC}{live['rej']} ✗{INK}")
        elif snap and snap.get("up"):
            R.append(f"{GOOD}{fmt_n(snap.get('acc'))} ✓{INK}  {SEC}{snap.get('rej') or 0} ✗{INK}")
        else:
            R.append(f"{SEC}—{INK}")
        bins = [0] * 30
        for s in self.share_rows:
            m = int((now - s["ts"]) // 60)
            if 0 <= m < 30:
                bins[29 - m] += 1
        R.append("".join((f"{GOOD}{'▁▃▅▆█'[min(4, n)]}" if n else f"{FAINT}▁") for n in bins) + f"{FAINT}{'▁' * (W - 30)}{INK}" if run else f"{FAINT}{'▁' * 30}{INK}")
        nxt = ""
        if run and res.get("avg_time") and self.share_rows:
            nxt = f" · next ~{max(0, int(res['avg_time']) - int(now - self.share_rows[-1]['ts']))} s"
        R.append(f"{FAINT}{'per minute' + nxt if run else ('last session' if snap else 'no shares yet')}{INK}")
        # threads
        rates = self.thread_rates() if run else []
        R.append("")
        R.append(lab("threads", core_layout() or f"{job.get('threads', '-')} threads"))
        if rates:
            per = max(rates) if max(rates) > 0 else 1.0
            R.append(fit_row(f"{DATA}{''.join(level_char(v / per) for v in rates)}{INK}", f"{fmt_hs(sum(rates))}", W))
            R.append(f"{FAINT}{len(rates)} threads in xmrig order · tallest {max(rates):.0f} H/s{INK}")
        else:
            R.append(f"{FAINT}{'▁' * int(job.get('threads') or 10) if str(job.get('threads', '')).isdigit() else '▁' * 10}{INK}")
            R.append(f"{FAINT}{'per-thread rates while mining' if not run else 'no per-thread data yet'}{INK}")
        # pool
        R.append("")
        R.append(lab("pool"))
        pool = conn.get("pool") or job.get("pool") or "—"
        host, _, port = str(pool).rpartition(":")
        R.append((host or pool) + (f"{WARN} · backup{INK}" if on_backup(conn, job) else ""))
        fails = conn.get("failures")
        R.append(f"{SEC}:{port or '—'} · {tls_label(conn, job)} · {fmt_ping(conn.get('ping')) if run else '—'} · {fails if fails is not None else '—'} failure{'' if str(fails) == '1' else 's'}{INK}")
        fs = self.fleet_latest()
        # balance: what the pool owes this wallet (main Mac only)
        if self.main:
            R.append("")
            bal = (fs[1].get("balance") if fs else None) or None
            R.append(lab("balance", f"{fleet.age(fs[1].get('pool_at'))} ago" if bal else ""))
            if bal:
                R.append(f"{BOLD}{bal['due']:.6f} XMR{NOBOLD}{SEC} due{INK}")
                thr = bal.get("threshold")
                if thr:
                    R.append(f"{bar(bal['due'] / thr, 16)} {SEC}{bal['due'] / thr * 100:.1f}% to payout{INK}")
                paid = f"{bal['paid']:.4f} paid" if bal["paid"] else "0 paid"
                R.append(f"{SEC}{f'payout at {thr:g} XMR · ' if thr else ''}{paid}{INK}")
            else:
                R.append(f"{SEC}—{INK}")
                R.append(f"{FAINT}asking the pool…{INK}")
        # dataset
        R.append("")
        ds_state = "released" if state == "STOPPED" else ("2.0 GB" if run else "building")
        R.append(lab("dataset", ds_state))
        el = (now - self.start_ts) if (starting and self.start_ts) else 0.0
        frac = 1.0 if run else (min(0.95, el / DATASET_S) if starting else 0.0)
        R.append(f"{bar(frac, 20, DATA if run else WARN)} {SEC}{'ready' if run else 'building' if starting else '—'}{INK}")
        R.append(f"{FAINT}huge pages {fmt_hugepages(live.get('hugepages'))} · 2336 MB{INK}")
        # machine
        brand, ram = cpu_info()
        R.append("")
        R.append(lab("machine"))
        R.append(f"{(api.get('cpu') or {}).get('brand') or brand} · {ram}")
        R.append(f"{SEC}XMRig {api.get('version') or (snap or {}).get('version') or '—'} {ARCH} · api :{job.get('http', '18088')}{INK}")
        if run or starting:
            ni = self.process_nice()
            if ni is not None:
                extra = "" if ni <= -10 else " · wanted -10"
                R.append(f"{SEC}nice {ni}{FAINT}{extra}{INK}")
        # session
        R.append("")
        R.append(lab("session"))
        if run or starting:
            R.append(f"{SEC}started {hms(now - (live['up'] or 0))} · {fmt_uptime(live['up'])}{INK}")
        elif snap and snap.get("up"):
            R.append(f"{SEC}last ran {fmt_uptime(snap.get('up') or 0)} · {snap.get('saved_at', '')[11:16]}{INK}")
        else:
            R.append(f"{SEC}nothing yet{INK}")
        # fleet: every Mac on this wallet (bin/fleet.py). Last, so it is the first to go when short.
        R.append("")
        if fs:
            frows, fmeta = fs
            R.append(lab("fleet", f"{fmeta.get('mining', 0)} of {fmeta.get('macs', 0)} · {fmt_hs(fmeta.get('total'))}"))
            for fr in frows[:4]:
                R.append(fit_row(*self.fleet_cells(fr), W))
                if self.main:
                    R.append(self.fleet_shares(fr))
            if len(frows) > 4:
                R.append(f"{FAINT}+{len(frows) - 4} more · /fleet{INK}")
        else:
            R.append(lab("fleet", "looking…"))
            R.append(f"{FAINT}LAN every 5 s · pool every 60 s{INK}")
        # fit: drop whole sections from the bottom until it fits
        while len(R) > avail:
            idx = max((i for i, ln in enumerate(R) if ln == ""), default=0)
            if idx == 0:
                R = R[:avail]
                break
            R = R[:idx]
        return [clip_row(ln, W) for ln in R]

    FLEET_TINT = {"mining": GOOD, "starting": WARN, "paused": WARN, "pool": WARN, "no token": BAD, "error": BAD}

    def fleet_cells(self, r: dict) -> tuple[str, str]:
        """One rail row for one Mac: mark + short name on the left, H/s (or its state) + where from on the right."""
        st = r["state"]
        col = self.FLEET_TINT.get(st, FAINT)
        name = r["worker"][8:] if r["worker"].startswith("minerv3-") else r["worker"]
        right = fmt_hs(fleet.eff_hs(r)) if st in fleet.ACTIVE else st
        pend = self.pending_cmd(r["worker"])
        if pend:
            right, col = control.DOING[pend].lower() + "…", WARN
        via = "here" if r["here"] else ("LAN" if r["via"] == "lan" else "pool")
        return (f"{col}{fleet.MARK.get(st, '?')}{INK} {trunc(name, 15)}",
                f"{INK if st in fleet.ACTIVE else SEC}{right}{FAINT} {via:>4}{INK}")

    def fleet_shares(self, r: dict) -> str:
        """Under a Mac's rail row: shares the pool has from that worker, and this run's count when the LAN has it."""
        pool = f"{GOOD}{fmt_n(r['pool_acc'])} ✓{SEC} pool" if r.get("pool_acc") is not None else f"{SEC}— pool"
        run = f"{FAINT} · {SEC}{fmt_n(r['acc'])} this run" if r.get("acc") is not None else ""
        rej = f"{FAINT} · {BAD}{r['rej']} ✗" if r.get("rej") else ""
        return f"{FAINT} └ {pool}{run}{rej}{INK}"

    def event_line(self, e: dict, name_w: int = 14) -> str:
        """One timed event: when · which Mac · mark + what · detail."""
        tone, title, det = control.describe(e)
        col = {"good": GOOD, "bad": BAD, "warn": WARN}.get(tone, INK)
        return (f"{SEC}{control.when(e['ts']):>14}{INK}  {trunc(control.short(e.get('worker') or ''), name_w):<{name_w}} "
                f"{col}{control.MARKS.get(e['k'], '•')} {title}{INK}" + (f"{SEC} · {det}{INK}" if det else ""))

    def now_row(self, live: dict, label: str = "now:     ") -> str:
        """The live line of the job card: what is happening right now, in one glance."""
        state = live["state"]
        if state == "RUNNING" and live.get("paused"):
            idle = pause_label(self.env()) or "the idle time"
            return (f"{SEC}{label}{INK}{WARN}paused{INK}{SEC} · you're active · back after {idle} idle · {INK}"
                    f"{GOOD}{fmt_n(live['acc'])} ✓{INK}")
        if state == "RUNNING":
            hs = live["hs"] or 0.0
            peak = live["highest"] or PEAK_HS
            frac = hs / peak if peak else 0.0
            rej = live["rej"]
            rej_s = f" {BAD}{rej} ✗{INK}" if rej else ""
            return (f"{SEC}{label}{INK}{fmt_hs(hs):<10} {bar(frac, 12)} {SEC}{frac * 100:.0f}%{INK} · "
                    f"{GOOD}{fmt_n(live['acc'])} ✓{INK}{rej_s} · {fmt_uptime(live['up'])}")
        if state == "STARTING":
            el = int(time.time() - self.start_ts) if self.start_ts else int(live["up"] or 0)
            return f"{SEC}{label}{INK}building the dataset{SEC} · {fmt_uptime(el)} · full speed in about a minute{INK}"
        snap = load_snapshot() if not self.dump else None
        if snap and snap.get("up"):
            avg = snap.get("hs15") or snap.get("hs60") or snap.get("hs10")
            return f"{SEC}{label}{INK}not mining{SEC} · last session {fmt_uptime(snap.get('up') or 0)} · {fmt_n(snap.get('acc'))} ✓ · avg {fmt_hs(avg)}{INK}"
        return f"{SEC}{label}{INK}not mining{SEC} · press s to start{INK}"

    def header_rows(self, live: dict, compact: bool) -> list[str]:
        job = live["job"]
        api = live.get("api") or {}
        w = min(66, self.lw)
        inner = w - 4
        ver = api.get("version") or (load_snapshot() or {}).get("version") or "6.26.0"
        spec = f"{job.get('algo','rx/0')} {job.get('mode','-')} · {job.get('threads','-')} threads"
        if compact:
            # three rows, as wide as the window allows: the title lives in the top border
            w = min(self.lw - 2, 78)
            inner = w - 4
            text = trunc(f"{CYAN}{BOLD}>_ {INK}XMR Miner{NOBOLD}{SEC} · {spec} · {job.get('pool_host','-')}{INK}", w - 6)
            fill = max(1, w - 5 - vis_len(text))
            return [
                self.r(f"{FAINT}╭─ {text} {FAINT}{'─' * fill}╮{INK}"),
                self.r(f"{FAINT}│{INK} {clip_row(self.now_row(live, 'now:  '), inner)} {FAINT}│{INK}"),
                self.r(f"{FAINT}╰{'─' * (w - 2)}╯{INK}"),
            ]
        title = f"{CYAN}{BOLD}>_ {INK}XMR Miner{NOBOLD}{SEC} (XMRig {ver}){INK}"
        rows_in = [
            title,
            "",
            f"{SEC}job:     {INK}{spec}{SEC}     + / − threads · /config{INK}",
            f"{SEC}pool:    {INK}{job.get('pool','-')}",
            f"{SEC}folder:  {INK}{short_path(str(job.get('cwd') or ROOT), inner - 9)}",
        ]
        if not self.rail_on():
            rows_in.append(self.now_row(live))  # with the rail up, the rail carries the live numbers
        out = [self.r("")]
        out.append(self.r(f"{FAINT}╭{'─' * (w - 2)}╮{INK}"))
        for ln in rows_in:
            out.append(self.r(f"{FAINT}│{INK} {clip_row(ln, inner)} {FAINT}│{INK}"))
        out.append(self.r(f"{FAINT}╰{'─' * (w - 2)}╯{INK}"))
        out.append(self.r(""))
        return out

    def ledger_rows(self, live: dict) -> list[str]:
        """The transcript: bullets with └ results. Newest last."""
        L: list[str] = []
        W = self.lw

        def bullet(text: str, c: str = SEC) -> None:
            L.append(self.r(f"{c}• {INK}{text}"))

        def tree(text: str, first: bool = True) -> None:
            L.append(self.r(f"{SEC}{'  └ ' if first else '    '}{INK}{text}"))

        def gap() -> None:
            L.append(self.r(""))

        def at(e: dict) -> str:
            return f"{SEC} · {control.when(e['ts'])}{INK}"

        def frm(e: dict) -> str:
            return f"{SEC} · from {control.short(e['by'])}{INK}" if e.get("by") else ""

        state = live["state"]
        api = live.get("api") or {}
        conn = api.get("connection") or {}
        res = api.get("results") or {}
        for e in self.events:
            k = e["k"]
            if k == "user":
                L.append(self.band(f"{SEC} › {INK}{e['text']}"))
                gap()
                continue
            if k == "start":
                word = "Restarted xmrig" if e.get("restart") else "Started xmrig"
                bullet((f"{BOLD}{word}{NOBOLD}" if not e.get("text") else f"{BOLD}xmrig{NOBOLD}{SEC} · {e['text']}{INK}") + frm(e) + at(e))
                tree(f"{SEC}{e.get('cmd') or live['job'].get('cmdline') or 'caffeinate -i xmrig'}{INK}")
                if e.get("nice"):
                    tree(f"{SEC}{e['nice']}{INK}", first=False)
            elif k == "dsprog":
                if state != "STARTING":
                    continue
                el = max(0.0, time.time() - (self.start_ts or time.time()))
                frac = min(0.95, el / DATASET_S)
                bullet(f"{BOLD}Building the RandomX dataset{NOBOLD}")
                tree(f"{el:>3.0f} s of about {DATASET_S:.0f}  {bar(frac, 30)}  {SEC}2.0 GB · full speed in about a minute{INK}")
            elif k == "dataset":
                logd = live.get("log") or {}
                secs = e.get("secs")
                if logd.get("dataset_ms") and (logd.get("dataset_ts") or 0) >= (self.start_ts or 0) - 2:
                    secs = logd["dataset_ms"] / 1000.0  # xmrig's own figure beats our stopwatch
                when = f" in {secs:.1f} s" if secs else (" before this window opened" if e["ts"] < self.started - 1 else "")
                al = logd.get("alloc")
                alloc = f"{al['mb']} MB ({al['dataset']} + {al['cache']})" if al else "2336 MB (2080 + 256)"
                bullet(f"{BOLD}Dataset ready{NOBOLD}{when}" + ("" if "before" in when else at(e)))
                tree(f"{SEC}{alloc} · huge pages {fmt_hugepages(live.get('hugepages') or (al and al['hp']))}{INK}")
            elif k == "pool":
                pool = conn.get("pool") or live["job"].get("pool") or "—"
                tls_s = tls_label(conn, live["job"])
                bullet(f"{BOLD}Connected{NOBOLD} to {pool}" + (f"{WARN} · backup pool{INK}" if on_backup(conn, live["job"]) else "") + at(e))
                tree(f"{SEC}{tls_s} · {fmt_ping(conn.get('ping'))} · worker {live['job'].get('worker') or '—'}{INK}")
            elif k == "warn":
                bullet(f"{WARN}{e['title']}{INK}" + at(e), WARN)
                tree(f"{SEC}{e.get('sub','')}{INK}")
            elif k == "reject":
                bullet(f"{BAD}Share rejected{INK} #{fmt_n(e.get('n'))}" + (f"{SEC} · ≈ time (no log line yet){INK}" if e.get("approx") else "") + at(e), BAD)
                bits = [f'{BAD}"{e["why"]}"{SEC}' if e.get("why") else "", f"diff {fmt_n(e.get('diff'))}", fmt_ping(e.get("ms"))]
                tree(f"{SEC}{' · '.join(b for b in bits if b)}{INK}")
            elif k == "backup":
                bullet(f"{WARN}Switched to the backup pool{INK}" + at(e), WARN)
                tree(f"{SEC}{e.get('pool') or '—'} · the main pool failed 5 times; xmrig goes back once it answers{INK}")
            elif k == "mainpool":
                bullet(f"{BOLD}Back on the main pool{NOBOLD}" + at(e))
                tree(f"{SEC}{e.get('pool') or '—'}{INK}")
            elif k == "remote":
                w, cmd, st = control.short(e["worker"]), e["cmd"], e.get("state")
                res = e.get("res") or {}
                if st == "pending":
                    bullet(f"{BOLD}{control.DOING[cmd]} {w}{NOBOLD}{SEC} from this Mac…{INK}" + at(e))
                    tree(f"{SEC}{'about a minute to full speed there' if cmd != 'stop' else 'waiting for its answer'}{INK}")
                elif st == "ok":
                    bullet(f"{BOLD}{control.DONE[cmd]} {w}{NOBOLD}{SEC} from this Mac · {control.when(e.get('done') or e['ts'])}{INK}")
                    if cmd == "stop" and res.get("acc") is not None:
                        tree(f"{GOOD}{fmt_n(res.get('acc'))}{SEC} accepted · {res.get('rej') or 0} rejected · ran {fmt_uptime(res.get('up') or 0)}"
                             f" · via its {res.get('via') or 'helper'}{INK}")
                    else:
                        tree(f"{SEC}via its {res.get('via') or 'helper'}{' · full speed in about a minute' if cmd != 'stop' else ''}{INK}")
                else:
                    bullet(f"{WARN}Could not {cmd} {w}{INK}{SEC} · {control.when(e.get('done') or e['ts'])}{INK}", WARN)
                    tree(f"{SEC}{res.get('error') or ' · '.join(res.get('lines') or []) or 'no answer'}{INK}")
            elif k == "fevent":
                ev = e["ev"]
                tone, title, det = control.describe(ev)
                col = {"good": GOOD, "bad": BAD, "warn": WARN}.get(tone, SEC)
                bullet(f"{INK}{control.short(ev.get('worker') or '')}{SEC} · {col}{title}{INK}" + at(ev), col)
                if det:
                    tree(f"{SEC}{det}{INK}")
            elif k == "shares":
                if state != "RUNNING":
                    continue
                acc, rej = live["acc"], live["rej"]
                bullet(f"{BOLD}Shares{NOBOLD}  {GOOD}{BOLD}{fmt_n(acc)}{NOBOLD}{INK} accepted · {BAD if rej else INK}{rej}{INK} rejected")
                rows = self.share_rows[-4:][::-1]
                if rows:
                    for i, s in enumerate(rows):
                        mark = f"{GOOD}✓{INK}" if s["ok"] else f"{BAD}✗{INK}"
                        # "~" = latency not in the log yet, so this is the pool ping at the time
                        ms_s = fmt_ping(s["ms"]) if s.get("src") == "log" else "~" + fmt_ping(s["ms"])
                        why = f"  {BAD}{s['why']}{INK}" if s.get("why") else ""
                        tree(f"{SEC}{('#' + fmt_n(s['n'])):<8}  {hms(s['ts'])}   {INK}diff {fmt_n(s['diff']):>7}   {SEC}{ms_s:>8}   {mark}{why}", i == 0)
                elif acc:
                    avg = res.get("avg_time")
                    tree(f"{SEC}{fmt_n(acc)} before this window · diff {fmt_n(res.get('diff_current'))} · one every ~{avg}s{INK}")
                else:
                    tree(f"{SEC}waiting for the first share…{INK}")
            elif k == "pause":
                idle = pause_label(self.env()) or "the idle time"
                bullet(f"{WARN}Paused{INK}{SEC} · keyboard or mouse in use · {control.when(e['ts'])}{INK}", WARN)
                tree(f"{SEC}mines again after {idle} without input · PAUSE in /config{INK}")
            elif k == "resume":
                idle = pause_label(self.env()) or "the idle time"
                bullet(f"{BOLD}Mining again{NOBOLD}{SEC} · no input for {idle} · {control.when(e['ts'])}{INK}")
            elif k == "stop":
                bullet(f"{BOLD}Stopped{NOBOLD} after {fmt_uptime(e.get('up') or 0)}" + (f"{SEC} · {e['text']}{INK}" if e.get("text") else "") + frm(e) + at(e))
                tree(f"{GOOD}{fmt_n(e.get('acc'))}{SEC} accepted · {e.get('rej') or 0} rejected · avg {fmt_hs(e.get('avg'))}{INK}")
            elif k == "laststop":
                s = e["snap"]
                bullet(f"{BOLD}Last session{NOBOLD}{SEC} · {s.get('saved_at','?')}{INK}")
                tree(f"{SEC}{fmt_uptime(s.get('up') or 0)} · {GOOD}{fmt_n(s.get('acc'))}{SEC} accepted · {s.get('rej') or 0} rejected · avg {fmt_hs(s.get('hs15') or s.get('hs60') or s.get('hs10'))}{INK}")
            elif k == "summary":
                bullet(f"Wrote {CYAN}{UNDER}{short_path(e['path'], W - 12)}{NOUNDER}{INK}")
            elif k == "note":
                bullet(e["text"])
            elif k == "out":
                lines = e.get("lines") or []
                if lines:
                    bullet(lines[0])
                    for i, ln in enumerate(lines[1:]):
                        tree(ln, i == 0)
            gap()
        while L and not plain(L[-1]).strip():
            L.pop()
        return L

    def card_rows(self, live: dict, room: int = 99) -> list[str]:
        """/usage, /config, /logs as a transcript entry: › band, then a card or a └ tail.
        When `room` is short the card sheds its blank rows first, then its tip line."""
        tab = TABS[self.tab]
        name = {"err": "/err", "events": "/events"}.get(self.log_which, "/logs") if tab == "Logs" else "/" + tab.lower()
        tight = room < 20
        out = [self.band(f"{SEC} › {INK}{name}")] + ([] if tight else [self.r("")])
        if tab == "Logs" and self.log_which == "events":
            who = "every Mac" if self.main else "this Mac"
            out.append(self.r(f"{SEC}• {INK}{BOLD}Events{NOBOLD}{SEC} · {who} · newest last   (e switches to xmrig.log){INK}"))
            evs = self.feed_latest(max(3, room - len(out) - 1))
            if not evs:
                out.append(self.r(f"{SEC}  └ no events yet: pool errors, rejected shares, starts and stops land here{INK}"))
            for i, e in enumerate(evs):
                out.append(self.r(f"{SEC}{'  └ ' if i == 0 else '    '}{INK}{self.event_line(e)}"))
            return out
        if tab == "Logs":
            path = ERR if self.log_which == "err" else LOG
            rel = os.path.relpath(path, ROOT)
            nxt = "the error log" if self.log_which == "log" else "events"
            out.append(self.r(f"{SEC}• {INK}{BOLD}Ran{NOBOLD} tail -n 20 {rel}{SEC}   (e switches to {nxt}){INK}"))
            tail = read_tail(path, 20)
            if not tail:
                out.append(self.r(f"{SEC}  └ (no output){INK}"))
            for i, ln in enumerate(tail):
                low = ln.lower()
                col = GOOD if "accepted" in low else BAD if ("rejected" in low or "error" in low or "fail" in low) else INK
                out.append(self.r(f"{SEC}{'  └ ' if i == 0 else '    '}{col}{ln}{INK}"))
            return out
        body = self.card_body(tab, live)
        if tight:
            body = [ln for ln in body if plain(ln).strip()]
            if room < len(out) + len(body) + 3 and len(body) > 2:
                body = body[:-1]  # the tip line goes before any data
        w = min(86, self.lw)
        inner = w - 4
        out.append(self.r(f"{FAINT}╭{'─' * (w - 2)}╮{INK}"))
        head = fit_row(f"{CYAN}{BOLD}>_ {INK}XMR Miner{NOBOLD}{SEC} · {tab.lower()}{INK}", f"{FAINT}←/→ usage · fleet · config · logs{INK}", inner)
        out.append(self.r(f"{FAINT}│{INK} {head} {FAINT}│{INK}"))
        if not tight:
            out.append(self.r(f"{FAINT}│{INK} {' ' * inner} {FAINT}│{INK}"))
        for ln in body:
            out.append(self.r(f"{FAINT}│{INK} {clip_row(ln, inner)} {FAINT}│{INK}"))
        if not tight:
            out.append(self.r(f"{FAINT}│{INK} {' ' * inner} {FAINT}│{INK}"))
        out.append(self.r(f"{FAINT}╰{'─' * (w - 2)}╯{INK}"))
        return out

    def card_body(self, tab: str, live: dict) -> list[str]:
        def kv(k: str, v: str) -> str:
            return f"  {SEC}{(k + ':'):<13}{INK}{v}"

        job = live["job"]
        state = live["state"]
        run = state == "RUNNING"
        if tab == "Fleet":
            fs = self.fleet_latest()
            if not fs:
                return [kv("Fleet", "looking for Macs…"), "", f"  {SEC}LAN every 5 s · pool every 60 s · minerctl fleet for the same table{INK}"]
            frows, m = fs
            pool_s = (f"pool {fmt_hs(m.get('pool_total'))} · {fleet.age(m.get('pool_at'))} ago" if m.get("pool") == "ok"
                      else f"pool: {m.get('pool')}")
            rows = [kv("Total", f"{BOLD}{fmt_hs(m.get('total'))}{NOBOLD}{SEC} · {m.get('mining', 0)} of {m.get('macs', 0)} mining · {pool_s}{INK}"), ""]
            on_card = self.mode in ("overlay", "ask") and TABS[self.tab] == "Fleet"
            pick = self.picked_row(frows)
            for fr in frows:
                st = fr["state"]
                col = self.FLEET_TINT.get(st, FAINT)
                pend = self.pending_cmd(fr["worker"])
                st_s = control.DOING[pend].lower() + "…" if pend else st
                if pend:
                    col = WARN
                hs_s = fmt_hs(fleet.eff_hs(fr)) if st in fleet.ACTIVE else "—"
                sh = f"{fmt_n(fr['acc'])} ✓" if fr.get("acc") is not None else "—"
                via = "this Mac" if fr["here"] else ("LAN" if fr["via"] == "lan" else "pool")
                up = fmt_uptime(fr["up"]) if fr.get("up") else "—"
                mark = f"{CYAN}▸{INK}" if (on_card and pick is fr) else " "
                rows.append(f" {mark}{col}{fleet.MARK.get(st, '?')}{INK} {trunc(fr['worker'], 22):<22} {col}{st_s:<9}{INK}{hs_s:>11} "
                            f"{sh:>9} {SEC}{up:>7}  {via}{INK}")
                note = fr.get("note") or ""
                if not fr["here"]:
                    note = control.ctl_label(fr, self.main) if fr.get("ctl") else (note or control.ctl_why(fr))
                if note:
                    rows.append(f"      {SEC}└ {note}{INK}")
            key = lambda k: f"{CYAN}{k}{SEC}"  # noqa: E731
            if self.main:
                rows += ["", f"  {key('↑↓')} pick a Mac · {key('s')} start · {key('t')} stop · {key('r')} restart{SEC} · stop and restart ask first{INK}"]
            else:
                rows += ["", f"  {key('↑↓')} pick · {key('s')} {key('t')} {key('r')} act on this Mac · the main Mac starts and stops the others{INK}"]
            evs = self.feed_latest(4)
            if evs:
                rows += ["", f"  {SEC}Recent events{FAINT} · /events for all{INK}"]
                rows += ["  " + self.event_line(e, 12) for e in evs]
            rows += ["", f"  {SEC}LAN every 5 s · pool every 60 s · last LAN scan {fleet.age(m.get('scan_at'))} ago{INK}",
                     f"  {SEC}On another Mac, ./bin/minerctl.sh fleet here says what this Mac can see of it{INK}"]
            if not m.get("token"):
                rows.append(f"  {WARN}fleet.token is missing: this Mac and the pool only{INK}")
            return rows
        if tab == "Config":
            flex = os.path.isfile(os.path.join(ROOT, "flex.on"))
            env = self.env()
            thr = env.get("THREADS") or str(job.get("threads") or "—")
            cores = env.get("CORES") or "—"
            spec = env.get("THREADS_SPEC") or "auto"
            try:
                frac = int(thr) / int(cores)
            except ValueError:
                frac = 0.0
            if self.perf_target is not None:
                thr_v = f"{thr} → {BOLD}{self.perf_target}{NOBOLD} of {cores}{SEC} · applying in a moment{INK}"
            else:
                src = "auto" if spec == "auto" else f"THREADS={spec} · auto is {env.get('AUTO_THREADS') or '—'}"
                thr_v = f"{thr} of {cores}  {bar(frac, 12)}  {SEC}{src}{INK}"
            if run and not self.dump and str(job.get("threads")) not in ("-", thr):
                thr_v += f"{WARN} · xmrig still runs {job.get('threads')}{INK}"
            mode_spec = env.get("MODE_SPEC") or "auto"
            yld = env.get("YIELD") or "off"
            pool = env.get("POOL") or job.get("pool") or "—"
            tls = (env.get("TLS") == "on") if env else bool(job.get("tls"))
            backup = env.get("BACKUP") or job.get("backup") or "off"
            ovr = [] if self.dump else local_overrides()
            key = lambda k: f"{CYAN}{k}{SEC}"  # noqa: E731
            rows = [
                kv("Threads", thr_v),
                kv("CPU", f"about {frac * 100:.0f}% of this Mac while mining" if frac else "—"),
                kv("Mode", (env.get("MODE") or job.get("mode") or "—") + f"{SEC} · {'auto' if mode_spec == 'auto' else 'MODE=' + mode_spec}{INK}"),
                kv("Yield", "on · other apps first, lower H/s" if yld == "on" else "off · xmrig keeps its cores"),
                kv("Pause", f"{pause_label(env)} idle{SEC} · paused while you're at the keyboard{INK}" if pause_label(env)
                   else f"off{SEC} · mines while you use this Mac{INK}"),
                kv("Algorithm", job.get("algo") or "—"),
                kv("Pool", f"{pool}{SEC} · {'TLS' if tls else 'no TLS'}{INK}"),
                kv("Backup", f"{backup}{SEC} · if the pool fails 5 times{INK}" if backup != "off"
                   else f"off{SEC} · one pool only{INK}"),
                kv("Worker", env.get("WORKER") or job.get("worker") or "—"),
                kv("HTTP API", f"{job.get('http_host', '127.0.0.1')}:{job.get('http', '18088')}"
                   + (f"{SEC} · LAN, token from fleet.token{INK}" if job.get("http_host") == "0.0.0.0"
                      else f"{SEC} · this Mac only{INK}")),
                kv("Donate", f"{job.get('donate') or '0'}%"),
                kv("Priority", "nice 0 · same as your apps (-10 needs root)"),
                kv("Flex", "on · pool picks the algo" if flex else "off · rx/0 only"),
                kv("Overrides", ("machine.local: " + ", ".join(ovr)) if ovr else f"none{SEC} · every setting automatic{INK}"),
                kv("Folder", short_path(str(job.get("cwd") or ROOT), self.lw - 22)),
            ]
            if self.ctl_note:
                rows.append(f"  {SEC}└ {INK}{self.ctl_note}")
            rows += [
                "",
                f"  {key('+ −')} threads · {key('a')} auto · {key('x')} max · {key('o')} eco · {key('m')} mode{INK}",
                f"  {key('y')} yield · {key('p')} pause · {key('e')} edit machine.local{INK}",
                f"  {SEC}Applies now; a running xmrig restarts (~1 min to full speed).{INK}",
            ]
            return rows
        api = live.get("api") or {}
        conn = api.get("connection") or {}
        res = api.get("results") or {}
        tot = (api.get("hashrate") or {}).get("total") or []
        hs = live["hs"]
        peak = live["highest"] or PEAK_HS
        pct = (float(hs) / peak) if (run and hs and peak) else 0.0
        acc, rej = live["acc"], live["rej"]
        totsh = acc + rej
        brand, ram = cpu_info()
        cpu = (api.get("cpu") or {}).get("brand") or brand
        thr = api_thread_rates() if run and not self.dump else []
        thr_s = " ".join(f"{t:.0f}" for t in thr) if thr else f"{job.get('threads','-')} threads"
        if thr:
            thr_s += f"{SEC}  · {sum(thr):,.0f} H/s across {len(thr)}{INK}"
        hp = fmt_hugepages(live.get("hugepages"))
        ds = "released" if state == "STOPPED" else ("2.0 GB ready" if run else "building")
        cadence = f"one every ~{res.get('avg_time')} s at diff {fmt_n(res.get('diff_current'))}" if (run and res.get("avg_time")) else "—"
        fails = conn.get("failures")
        fail_s = "—" if fails is None else f"{fails} failure{'' if str(fails) == '1' else 's'}"
        bw = 28 if self.lw >= 86 else 18  # the card is narrower beside the rail
        rows = [
            kv("Hashrate", f"{(fmt_hs(hs) if run else ('warming up' if state == 'STARTING' else '0 H/s')):<11}{bar(pct, bw)}  {SEC}{pct * 100:.0f}% of peak{INK}"),
            kv("Windows", f"10s {fmt_n(tot[0] if len(tot) > 0 else None)} · 60s {fmt_n(tot[1] if len(tot) > 1 else None)} · 15m {fmt_n(tot[2] if len(tot) > 2 else None)} · max {fmt_n((api.get('hashrate') or {}).get('highest'))}" if run else f"peak {fmt_n(peak)} H/s from the offline sweep"),
            kv("Shares", f"{GOOD}{(fmt_n(acc) + ' ✓'):<11}{INK}{bar(acc / totsh if totsh else 0.0, bw, GOOD)}  {SEC}{rej} rejected{INK}"),
            kv("Cadence", cadence),
            "",
            kv("Threads", thr_s),
            kv("Dataset", f"{ds}{SEC} · huge pages {hp}{INK}"),
            kv("Pool", f"{conn.get('pool') or job.get('pool') or '—'}{SEC} · {tls_label(conn, job)} · {fmt_ping(conn.get('ping')) if run else '—'} · {fail_s}{INK}"),
            kv("Machine", f"{cpu} · {ram} · XMRig {api.get('version') or '—'} {ARCH}"),
            kv("Uptime", fmt_clock(live["up"]) if run or state == "STARTING" else "not running"),
        ]
        return rows

    def palette_rows(self, live: dict, n: int) -> list[str]:
        """Grouped list with a selected band, digit shortcuts, typed-prefix highlight and a live status column.
        Headers go first when there is no room; then the command list scrolls."""
        lines = self.palette_lines(True)
        if len(lines) > n:
            lines = self.palette_lines(False)
        cmds = [ln for ln in lines if ln[0] == "c"]
        if not cmds:
            return [self.r(f"  {SEC}{typed_hint(self.palette_query()) or 'no matching commands'}{INK}")]
        sel = max(0, min(self.sel, len(cmds) - 1))
        if len(lines) > n:  # scroll a window of the flat list around the selection
            start = min(max(0, sel - n + 1), len(lines) - n)
            lines = lines[start : start + n]
        q = self.palette_query()[1:].lower()
        W = self.lw
        status_w = 30 if W >= 100 else (24 if W >= 72 else 0)
        out = []
        for ln in lines:
            if ln[0] == "h":
                out.append(self.r(f" {FAINT}{ln[1]}{INK}"))
                continue
            c, ordinal = ln[1], ln[2]
            on = c is cmds[sel][1]
            digit = f"{ordinal}" if ordinal <= 9 else " "
            # name with the typed prefix in bold
            nm = c.name
            if q and nm[1:].lower().startswith(q):
                name = f"{BOLD}{nm[:1 + len(q)]}{NOBOLD}{nm[1 + len(q):]}"
            else:
                name = nm
            name_pad = " " * max(0, 10 - len(nm))
            status = self.cmd_status(c, live, compact=W < 100) if status_w else ""
            desc_w = max(8, W - 16 - (status_w + 2 if status else 0))
            left = f"{'▎' if on else ' '}{digit} {CYAN if on else SEC}{name}{name_pad}{INK if on else FAINT}  {trunc(c.desc, desc_w)}"
            row = fit_row(left, f"{SEC}{trunc(status, status_w)}{INK}" if status else "", W)
            if on:
                row = BAND_BG + CYAN + row + RESET  # the ▎ picks up the accent; the row gets the band ground
            out.append(row)
        return out

    def status_row(self, live: dict) -> Optional[str]:
        state = live["state"]
        if state == "STOPPED":
            return None
        f = self.frame_no
        word = "Building dataset" if state == "STARTING" else ("Paused" if live.get("paused") else "Mining")
        dot = f"{SEC}{'•' if (f >> 1) % 2 else '◦'}{INK}"
        hi = (f % (len(word) + 8)) - 4
        letters = []
        for i, ch in enumerate(word):
            d = abs(i - hi)
            letters.append((BRIGHT + BOLD if d == 0 else MID + BOLD if d == 1 else SEC + NOBOLD) + ch)
        if state == "STARTING":
            el = int(time.time() - self.start_ts) if self.start_ts else int(live["up"] or 0)
            tail = f"({fmt_uptime(el)} • t to cancel)"
        elif live.get("paused"):
            tail = f"(you're active • mines after {pause_label(self.env()) or 'the idle time'} idle • t to stop)"
        else:
            tail = f"({fmt_clock(live['up'])} • t to stop)"
        return self.r(f"{dot} {''.join(letters)}{NOBOLD}{SEC} {tail}{INK}")

    def band_rows(self, live: dict, y0: int) -> list[str]:
        if self.mode == "confirm":
            text, ph = self.confirm_buf, "yes"
        elif self.mode == "ask":
            text, ph = "", "y to go ahead · n or esc to cancel"
        else:
            text = self.buf if self.force_palette is None else self.force_palette
            if self.mode == "overlay":
                ph = "esc to return · ←/→ next card"
            elif live["state"] == "STOPPED":
                ph = "Type / for commands, s to mine"
            else:
                ph = "Type / for commands, t to stop"
        room = max(1, self.lw - 4)
        vis = text if len(text) <= room else text[-room:]
        line = f" {SEC}› {INK}{vis}" if vis else f" {SEC}› {SEC}{trunc(ph, room)}{INK}"
        self.cursor = (y0 + 1, 3 + len(vis))
        top = ""
        if self.mode == "home" and self.force_palette is None and self.buf.startswith("/"):
            cmds = self.visible_cmds()
            sel = max(0, min(self.sel, len(cmds) - 1)) + 1 if cmds else 0
            top = fit_row(f" {FAINT}{sel} of {len(cmds)}{INK}" if cmds else f" {FAINT}0 of 0{INK}",
                          f"{FAINT}↑↓ move · 1–9 run · tab complete · ↵ run · esc close{INK} ", self.lw)
        elif self.force_palette is not None:
            cmds = self.visible_cmds()
            top = fit_row(f" {FAINT}1 of {len(cmds)}{INK}", f"{FAINT}↑↓ move · 1–9 run · tab complete · ↵ run · esc close{INK} ", self.lw)
        return [self.band(top), self.band(line), self.band("")]

    def footer_row(self, live: dict) -> str:
        state = live["state"]
        if self.mode == "confirm":
            keys = [("↵", "run"), ("esc", "cancel")]
            right = "offline sweep · mines nothing"
        elif self.mode == "ask":
            keys = [("y", "go ahead"), ("n", "cancel")]
            right = ""
        elif self.mode == "overlay":
            keys = [("←→", "cards"), ("esc", "back"), ("s", "start"), ("t", "stop")]
            if TABS[self.tab] == "Config":
                keys = [("←→", "cards"), ("+−", "threads"), ("e", "edit"), ("esc", "back")]
            elif TABS[self.tab] == "Fleet":
                keys = [("↑↓", "pick"), ("s", "start"), ("t", "stop"), ("r", "restart"), ("esc", "back")]
            elif TABS[self.tab] == "Logs":
                keys = [("←→", "cards"), ("e", "next log"), ("esc", "back")]
            right = ""
        else:
            keys = [("↵", "run"), ("s", "start"), ("t", "stop"), ("+−", "threads"), ("/", "commands"), ("⌃C", "quit")]
            right = ""
        if self.perf_target is not None:
            wait = max(0.0, PERF_DELAY - (time.time() - self.perf_at))
            right = f"threads {self.env().get('THREADS', '?')} → {self.perf_target} · restarting in {wait:.0f}s"
        if not right:
            if state == "RUNNING" and live.get("paused"):
                right = "paused · you're active"
            elif state == "RUNNING":
                hs = live["hs"] or 0
                peak = live["highest"] or PEAK_HS
                right = f"{fmt_hs(hs)} · {hs / peak * 100:.0f}% of peak"
            elif state == "STARTING":
                right = "building dataset"
            else:
                right = "not mining"
        left = " " + "   ".join(f"{INK}{k} {SEC}{v}" for k, v in keys) + INK
        return fit_row(left, f"{SEC}{right}{INK}", self.cols)

    # ------------------------------------------------------------------ frame
    def compose(self, live: dict) -> list[str]:
        rows, cols = max(1, self.rows), max(1, self.cols)
        rail = self.rail_on() and cols >= self.RAIL_W + 20
        self.lw = cols - self.RAIL_W - 4 if rail else cols
        compact = rows <= 24
        head = self.header_rows(live, compact)
        top = len(head)
        band_y = rows - 4
        pal_n = 0
        ms = self.matches()
        pal = self.mode == "home" and (self.force_palette is not None or self.buf.startswith("/"))
        if pal:
            avail = max(1, rows - top - 6)
            want = len(self.palette_lines(True))
            if want > avail:
                want = len(self.palette_lines(False))
            pal_n = max(1, min(want or 1, 19, avail))  # 14 commands + 5 group headers
        status = self.status_row(live) if (self.mode == "home" and not pal) else None
        if self.mode == "confirm":
            status = self.r(f"{SEC}• {INK}Offline sweep{SEC} · ~10 minutes · mines nothing · refuses if the miner is running · type yes{INK}")
        elif self.mode == "ask":
            status = self.r(f"{WARN}• {INK}{BOLD}{(self.ask or {}).get('text', '')}{NOBOLD}  {SEC}y / n{INK}")
        body_end = band_y - pal_n - (1 if status else 0) - (1 if pal else 0)  # exclusive; one spacer above a palette
        room = max(1, body_end - top)
        if self.mode == "overlay" or (self.mode == "ask" and (self.ask or {}).get("back") == "overlay"):
            body = self.card_rows(live, room)[:room]
        else:
            body = self.ledger_rows(live)[-room:]
        while len(body) < room:
            body.append(self.r(""))
        if pal:
            body.append(self.r(""))
        out = head + body
        if status:
            out.append(status)
        if pal:
            out += self.palette_rows(live, pal_n)
        out += self.band_rows(live, len(out))
        if len(out) > rows - 1:
            out = out[-(rows - 1):]
        while len(out) < rows - 1:
            out.insert(top, self.r(""))
        if rail:
            # left pane · hairline · rail; the footer below stays full width
            rr = self.rail_rows(live, rows - 2)
            merged = []
            for i, left in enumerate(out):
                r_ = rr[i] if i < len(rr) else " " * self.RAIL_W
                rule = f" {RULE}│{INK}  " if i >= 1 else "    "
                merged.append(left + rule + clip_row(r_, self.RAIL_W))
            out = merged
        self.lw = cols
        out.append(self.footer_row(live))
        self.lw = cols - self.RAIL_W - 4 if rail else cols
        return out

    def draw(self, force: bool = True, live: Optional[dict] = None) -> None:
        if self.size() or self._resized:
            force = True
            self._resized = False
        self.frame_no += 1
        try:
            if live is None:
                live = self.live()
            lines = self.compose(live)
        except Exception as e:
            if self.dump:
                raise
            lines = [self.r(f"{BAD}UI error{INK}  {e}"), self.r(f"{SEC}redraws every second · the miner is not affected.{INK}")]
            while len(lines) < self.rows:
                lines.append(self.r(""))
            self.cursor = None
        self._flush(lines, force)

    def _flush(self, lines: list[str], force: bool) -> None:
        self.last_draw = time.time()
        if self.dump:
            sys.stdout.write("\n".join(lines) + "\n")
            sys.stdout.flush()
            return
        wipe = CLEAR if self._clear_next else HOME
        self._clear_next = False
        body = wipe + "\r\n".join(lines)
        if self.cursor:
            y, x = self.cursor
            body += f"\033[{y + 1};{x + 1}H"
        else:
            body += "\033[J"  # leftover rows from a taller previous size
        if not force and body == self.last_frame:
            return
        self.last_frame = body
        sys.stdout.write(HIDE + SYNC_BEGIN + body + SYNC_END + (SHOW if self.cursor else ""))
        sys.stdout.flush()

    # ------------------------------------------------------------------ actions
    def open_overlay(self, tab_name: str, log_which: str = "log") -> None:
        if tab_name in TABS:
            self.tab = TABS.index(tab_name)
        self.log_which = log_which
        self.mode = "overlay"
        self.buf = ""
        self.sel = 0

    def say(self, *lines: str) -> None:
        self.add("out", lines=[ln for ln in lines if ln])

    def ctl_env(self, why: str, by: str = "") -> dict:
        """minerctl notes each start and stop in logs/events.jsonl with why and who asked."""
        return dict(os.environ, MINER_WHY=why, MINER_BY=by)

    def do_start(self, by: str = "") -> tuple:
        """s here, or a start the main Mac sent (by = its worker). (ok, minerctl's lines)."""
        self.add("user", text=f"s · from {control.short(by)}" if by else "s")
        live = self.live()
        try:
            out = subprocess.check_output([CTL, "start"], text=True, timeout=12, env=self.ctl_env("remote" if by else "window", by))
        except subprocess.CalledProcessError as e:
            out = e.output or "start failed"
        except Exception as e:
            out = str(e)
        lines = [ln for ln in out.splitlines() if ln.strip()]
        nice = next((ln.strip() for ln in lines if ln.startswith("Nice:")), "")
        cmd = self.live_cache[1]["job"].get("cmdline") if self.live_cache else ""
        # the verdict line wherever it is: a stray line before it once made a good start look failed
        head = next((ln for ln in lines if ln.startswith(("Started", "Already running"))), lines[0] if lines else "")
        if live["state"] in ("RUNNING", "STARTING") and head.startswith("Already running"):
            msg = f"Already mining ({fmt_uptime(live.get('up') or 0)}). t stops it."
            self.say(msg, nice) if nice else self.say(msg)
            self.nice_cache = None
            self.mode = "home"
            return True, [msg]
        ok = True
        if head.startswith("Started"):
            self.note_started(nice, by=by)
        elif head.startswith("Already running"):
            # leftover from a previous window (q does not stop xmrig). Attach; do not spawn.
            api = api_summary()
            up = int((api or {}).get("uptime") or 0)
            self.start_ts = time.time() - up
            self.ds_ready_s = None
            self.last_acc = self.last_rej = self.last_fail = None
            self.nice_cache = None
            self.drop("dsprog")
            self.add("start", text=f"already running ({fmt_uptime(up)})", cmd=cmd, nice=nice)
            if api:
                self.prev_state = "STOPPED"  # next poll: STOPPED → RUNNING fills dataset/pool
            else:
                self.add("dsprog")
                self.prev_state = "STARTING"
        else:
            ok = False
            self.say(*(lines or ["start: no output"]))
        self.live_cache = None
        self.mode = "home"
        return ok, lines

    def note_started(self, nice: str = "", by: str = "", restart: bool = False) -> None:
        """Book a fresh xmrig in the ledger: s, a command from the main Mac, or a restart."""
        self.start_ts = time.time()
        self.ds_ready_s = None
        self.share_rows = []
        self.last_acc = self.last_rej = self.last_fail = None
        self.nice_cache = None
        self.prev_backup = None
        self.drop("dsprog", "shares", "dataset", "pool")
        self.add("start", cmd=parse_job().get("cmdline") or "", nice=nice, by=by, restart=restart)
        self.add("dsprog")
        self.prev_state = "STARTING"

    # ------------------------------------------------------------------ settings (machine.local)
    def env(self) -> dict:
        if self.env_fixed is not None:
            return self.env_fixed
        now = time.time()
        if self.env_cache is None or now - self.env_cache[0] > 5:
            self.env_cache = (now, machine_env())
        return self.env_cache[1]

    def ctl(self, *args: str) -> None:
        """Run minerctl (perf / config), print its answer; a restart in it is booked like s."""
        try:
            p = subprocess.run([CTL, *args], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            out = p.stdout or ""
        except Exception as e:
            out = str(e)
        lines = [ln.rstrip() for ln in out.splitlines() if ln.strip()]
        nice = next((ln.strip() for ln in lines if ln.startswith("Nice:")), "")
        shown = [ln for ln in lines if not ln.startswith(("Started", "Nice:", "Job file written"))]
        self.say(*(shown or ["(no output)"]))
        self.ctl_note = " · ".join(ln.strip() for ln in shown[:2])
        self.env_cache = None
        self.live_cache = None
        if any(ln.startswith("Started") for ln in lines):
            self.note_started(nice)

    def perf_step(self, d: int) -> None:
        """+ / −: one thread more or fewer. Stopped, it applies at once; mining, presses gather
        for PERF_DELAY seconds so a single restart applies them all."""
        env = self.env()
        try:
            cores, cur = int(env["CORES"]), int(env["THREADS"])
        except (KeyError, ValueError):
            self.say("Could not read this Mac's threads (./bin/machine.sh --env).")
            return
        base = self.perf_target if self.perf_target is not None else cur
        new = max(1, min(cores, base + d))
        if new == base:
            if self.perf_target is None:
                self.ctl_note = f"Already {'at the top' if d > 0 else 'at the bottom'}: {cur} of {cores} threads."
                self.say(self.ctl_note)
            return
        self.perf_target = None if new == cur else new
        self.perf_at = time.time()
        if self.perf_target is not None and self.live()["state"] == "STOPPED":
            self.apply_perf()

    def apply_perf(self) -> None:
        n, self.perf_target = self.perf_target, None
        if n is None:
            return
        self.add("user", text=f"/perf {n}")
        if not self.dump:
            self.draw()  # the restart blocks for a few seconds; show the turn first
        self.ctl("perf", str(n))

    def run_typed(self, parts: list[str]) -> None:
        """/perf N, /threads N, /set KEY=value …, /unset KEY …, /pause on|off|N from the prompt or a /config key."""
        self.add("user", text=" ".join(parts))
        if not self.dump:
            self.draw()
        self.perf_target = None
        head, rest = parts[0], parts[1:]
        if head in ("/start", "/stop", "/restart"):
            self.fleet_cmd(head[1:], " ".join(rest))
        elif head in ("/perf", "/threads"):
            self.ctl("perf", *rest[:1])
        elif head == "/set":
            self.ctl("config", "set", *rest)
        elif head == "/pause":
            v = pause_value(rest[0]) if rest else None
            if v:
                self.ctl("config", "set", f"PAUSE={v}")
            else:
                self.say("/pause on | off | 10-3600 (seconds without keyboard or mouse before mining again)")
        else:
            self.ctl("config", "unset", *rest)

    def do_edit(self) -> None:
        """e on /config: machine.local in $EDITOR (nano), then apply it here so a restart is followed."""
        self.restore_tty()
        sys.stdout.write(SHOW + WRAP_ON + RESET + ALT_OFF)
        sys.stdout.flush()
        try:
            rc = subprocess.call([CTL, "config", "edit", "--no-apply"])
        except Exception as e:
            rc = 1
            print(f"  edit failed: {e}")
        self.take_tty()
        sys.stdout.write(ALT_ON + HIDE + WRAP_OFF)
        sys.stdout.flush()
        self._clear_next = True
        self.last_frame = ""
        self.add("user", text="/config edit")
        if rc != 0:
            self.ctl_note = "Editor exited with an error; nothing applied."
            self.say(self.ctl_note)
            return
        self.ctl("config", "apply")

    def do_stop(self, by: str = "") -> tuple:
        """t here, or a stop the main Mac sent (by = its worker). (ok, minerctl's lines)."""
        self.add("user", text=f"t · from {control.short(by)}" if by else "t")
        live = self.live()
        try:
            out = subprocess.check_output([CTL, "stop"], text=True, timeout=15, env=self.ctl_env("remote" if by else "window", by))
        except Exception as e:
            out = str(e)
        lines = [ln for ln in out.splitlines() if ln.strip()]
        ok = any(ln.startswith(("Stopped", "Not running")) for ln in lines)
        if any(ln.startswith("Stopped") for ln in lines):
            api = live.get("api") or {}
            tot = (api.get("hashrate") or {}).get("total") or []
            avg = next((v for v in (tot[2] if len(tot) > 2 else None, tot[1] if len(tot) > 1 else None, live.get("hs")) if v), None)
            self.drop("dsprog", "shares")
            self.add("stop", up=live.get("up") or 0, acc=live.get("acc") or 0, rej=live.get("rej") or 0, avg=avg, by=by)
            for ln in lines:
                if ln.startswith("Desktop summary:"):
                    self.add("summary", path=ln.split(":", 1)[1].strip())
            self.prev_state = "STOPPED"
            self.last_acc = self.last_rej = None
        else:
            self.say(*(lines or ["stop: no output"]))
        self.live_cache = None
        self.mode = "home"
        return ok, lines

    def do_restart(self, by: str = "") -> tuple:
        """r on the Fleet card for this Mac, /restart, or a restart the main Mac sent."""
        self.add("user", text=f"restart · from {control.short(by)}" if by else "/restart")
        if not self.dump:
            self.draw()  # the restart blocks for a few seconds; show the turn first
        try:
            p = subprocess.run([CTL, "restart"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=60, env=self.ctl_env("restart", by))
            out = p.stdout or ""
        except Exception as e:
            out = str(e)
        lines = [ln.rstrip() for ln in out.splitlines() if ln.strip()]
        self.live_cache = None
        if any(ln.startswith("Started") for ln in lines):
            self.note_started(next((ln.strip() for ln in lines if ln.startswith("Nice:")), ""), by=by, restart=True)
            return True, lines
        self.say(*(lines or ["restart: no output"]))
        return False, lines

    # ------------------------------------------------------------------ the fleet: commands between Macs
    def picked_row(self, rows: list) -> Optional[dict]:
        """The Fleet card's picked Mac (by name, so a re-sort by hashrate does not move the pick)."""
        if not rows:
            return None
        return next((r for r in rows if r["worker"] == self.fleet_pick), None) or next((r for r in rows if r["here"]), rows[0])

    def pick_step(self, d: int) -> None:
        rows = self.fleet_rows()
        cur = self.picked_row(rows)
        if cur is not None:
            self.fleet_pick = rows[(rows.index(cur) + d) % len(rows)]["worker"]

    def fleet_cmd(self, cmd: str, arg: str = "", workers: Optional[list] = None) -> None:
        """/start m2, /stop all, or s / t / r on a Fleet card row. This Mac runs it here; another Mac
        gets a signed command (main Mac only). Stop, restart, and more than one Mac ask y / n first."""
        rows = self.fleet_rows()
        live = self.live()
        me = next((r["worker"] for r in rows if r["here"]), None) or live["job"].get("worker") or "this Mac"
        # this Mac's own state comes from this window, not the 5 s poll
        rows = [dict(r, state="mining" if live["state"] != "STOPPED" else "stopped") if r["here"] else r for r in rows]
        if not any(r["here"] for r in rows):
            rows.insert(0, {"worker": me, "here": True, "state": "mining" if live["state"] != "STOPPED" else "stopped"})
        if workers is None:
            workers, why = control.match_targets(arg or "here", [r["worker"] for r in rows], me)
            if why:
                self.say(why)
                return
        busy = [w for w in workers if self.pending_cmd(w)]
        have_key = self.key_fixed if self.key_fixed is not None else os.path.isfile(control.KEY)
        go, skip = control.plan(cmd, [w for w in workers if w not in busy], rows, self.main, have_key)
        skip = [(w, f"{self.pending_cmd(w)} already on its way") for w in busy] + skip
        notes = [f"{control.short(w)}: {why}" for w, why in skip]
        if not go:
            self.say(*(notes or ["Nothing to do."]))
            return
        names = ", ".join(control.short(r["worker"]) + (" (this Mac)" if r["here"] else "") for r in go)
        if cmd == "start" and len(go) == 1:
            self.fleet_go(cmd, go, notes)
            return
        what = names if len(go) == 1 else f"{len(go)} Macs ({names})"
        back = self.mode if self.mode in ("home", "overlay") else "home"
        self.ask = {"cmd": cmd, "rows": go, "note": notes, "back": back, "text": f"{cmd.capitalize()} {what}?"}
        self.mode = "ask"

    def fleet_go(self, cmd: str, rows: list, notes: list) -> None:
        back = self.mode
        if notes:
            self.say(*[f"skipped {n}" for n in notes])
        for r in rows:
            if r["here"]:
                {"start": self.do_start, "stop": self.do_stop, "restart": self.do_restart}[cmd]()
                self.mode = back
            else:
                self.remote_send(cmd, r)

    def remote_send(self, cmd: str, r: dict) -> None:
        """One signed command, sent from a thread; the answer comes back through remote_q."""
        ev = self.add("remote", cmd=cmd, worker=r["worker"], state="pending")
        if self.dump:
            return
        host, w = r.get("host") or "", r["worker"]

        def run() -> None:
            self.remote_q.put((ev, control.send(host, w, cmd, FLEET_TOKEN)))  # type: ignore[attr-defined]

        threading.Thread(target=run, name="send", daemon=True).start()

    def drain_remote(self) -> bool:
        changed = False
        while True:
            try:
                ev, res = self.remote_q.get_nowait()  # type: ignore[attr-defined]
            except queue.Empty:
                break
            ev.update(state="ok" if res.get("ok") else "fail", res=res, done=time.time())
            changed = True
        if changed:
            for w in (self.fleet_w, self.feed_w):
                if w is not None:
                    w.poke()  # type: ignore[attr-defined]
        return changed

    def on_key_ask(self, key: str) -> bool:
        a = self.ask or {}
        if key == "quit":
            return False
        if key not in ("y", "Y", "n", "N", "esc", "enter"):
            return True  # still asking
        self.ask = None
        self.mode = a.get("back") or "home"
        if key in ("y", "Y", "enter"):
            self.fleet_go(a["cmd"], a["rows"], a.get("note") or [])
        else:
            self.say("Cancelled.")
        return True

    def poll_inbox(self) -> None:
        """Commands from the main Mac (passed on by this Mac's helper): run them like s / t / r here."""
        if self.dump or time.time() - self.inbox_at < 0.25:
            return
        self.inbox_at = time.time()
        for req in control.take_requests():
            before = self.live()
            mode, tab = self.mode, self.tab
            if mode == "ask":
                self.ask, mode = None, "home"
            cmd, by = req["cmd"], req.get("from") or "the main Mac"
            ok, lines = {"start": self.do_start, "stop": self.do_stop, "restart": self.do_restart}[cmd](by=by)
            self.mode, self.tab = mode, tab
            self.live_cache = None
            after = self.live()
            control.finish_request(req["nonce"], {"ok": ok, "lines": [plain(ln) for ln in lines][-4:], "state": after["state"].lower(),
                                                  "up": before.get("up"), "acc": before.get("acc"), "rej": before.get("rej")})
            self.draw()

    LEDGER_KINDS = ("error", "reject", "backup", "pool", "start", "stop", "exit", "denied")

    def book_feed(self) -> None:
        """Main Mac: the other Macs' new events get ledger lines (an outage that grows updates its line)."""
        if not self.main or self.dump or self.feed_w is None or time.time() - self.feed_at < 1.0:
            return
        self.feed_at = time.time()
        me = self.live()["job"].get("worker")
        for e in self.feed_w.feed.changed_after(self.feed_seq):  # type: ignore[attr-defined]
            self.feed_seq = max(self.feed_seq, e.pop("_seq"))
            if e.get("worker") in (me, None) or e["k"] not in self.LEDGER_KINDS:
                continue
            if max(e["ts"], e.get("last") or 0) < self.started - 5 or control.describe(e)[0] == "hide":
                continue
            if e["k"] in ("start", "stop") and e.get("by") == me and any(
                    x["k"] == "remote" and x.get("worker") == e.get("worker") and abs(x["ts"] - e["ts"]) < 90 for x in self.events):
                continue  # this window sent it and already has a line for it (one from Terminal still shows)
            old = self.feed_rows.get(e["id"])
            if old is not None:
                old["ev"] = e
            else:
                self.feed_rows[e["id"]] = self.add("fevent", ts=e["ts"], ev=e)

    def do_open(self) -> None:
        subprocess.Popen(["open", ROOT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.add("user", text="/open")
        self.say(f"Opened {short_path(ROOT, self.cols - 12)} in Finder.")
        self.mode = "home"

    def do_flex(self) -> None:
        self.add("user", text="/flex")
        path = os.path.join(ROOT, "flex.on")
        if os.path.isfile(path):
            os.remove(path)
            self.say("flex off. The next s is rx/0 only.")
        else:
            open(path, "w").close()
            self.say("flex on. The next s lets the pool pick the algo (not always XMR).", "If it is running: t first, then s.")
        self.mode = "home"

    def do_pause(self) -> None:
        """/pause alone: pause-while-you-work on (120 s) or off, the same as p on /config."""
        self.run_typed(["/pause", "off" if pause_label(self.env()) else "on"])

    def do_bench(self) -> None:
        if self.live()["state"] != "STOPPED":
            self.say("The miner is running. Press t, then /bench again.")
            self.mode = "home"
            self.confirm_buf = ""
            return
        script = os.path.join(ROOT, "bin", "xmr_bench_sweep.sh")
        self.restore_tty()
        sys.stdout.write(SHOW + RESET + "\n")
        sys.stdout.flush()
        try:
            subprocess.call([script])
        except Exception as e:
            print(f"  bench failed: {e}")
        print()
        print("  enter to return")
        try:
            input()
        except Exception:
            pass
        self.take_tty()
        self.mode = "home"
        self.confirm_buf = ""
        self.say("Bench finished. /usage for the numbers.")
        self.last_frame = ""

    def run_action(self, action: str) -> bool:
        """Return False to quit."""
        if action == "quit":
            return False
        if action == "usage":
            self.open_overlay("Usage")
        elif action == "fleet":
            self.open_overlay("Fleet")
        elif action == "config":
            self.open_overlay("Config")
        elif action == "logs":
            self.open_overlay("Logs", "log")
        elif action == "err":
            self.open_overlay("Logs", "err")
        elif action == "open":
            self.do_open()
        elif action == "flex":
            self.do_flex()
        elif action == "pause":
            self.do_pause()
        elif action == "start":
            self.do_start()
        elif action == "stop":
            self.do_stop()
        elif action == "restart":
            if self.live()["state"] == "STOPPED":
                self.say("Not mining here. s starts it; /restart m2 restarts another Mac.")
            else:
                self.do_restart()
        elif action == "events":
            self.open_overlay("Logs", "events")
        elif action == "bench":
            self.add("user", text="/bench")
            self.mode = "confirm"
            self.confirm_buf = ""
        elif action == "help":
            self.buf = "/"
            self.sel = 0
            self.mode = "home"
        else:
            self.say("Unknown command. Type / for the list.")
        return True

    # ------------------------------------------------------------------ input
    def take_tty(self) -> None:
        self.fd = sys.stdin.fileno()
        self.old_tty = termios.tcgetattr(self.fd)
        tty.setraw(self.fd)

    def restore_tty(self) -> None:
        if self.old_tty is not None:
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_tty)

    def read_key(self) -> Optional[str]:
        fds = [self.fd]
        if self._winch_r is not None:
            fds.append(self._winch_r)
        # ~30 Hz so a drag rearranges live; SIGWINCH also wakes via the pipe.
        timeout = 0.03 if (time.time() - self.last_resize) < 0.5 else 0.08
        try:
            r, _, _ = select.select(fds, [], [], timeout)
        except InterruptedError:
            self._resized = True
            return None
        if self._winch_r is not None and self._winch_r in r:
            try:
                os.read(self._winch_r, 1024)
            except (BlockingIOError, OSError):
                pass
            self._resized = True
        if self.fd not in r:
            return None
        b = os.read(self.fd, 1)
        if not b:
            return "quit"
        if b in (b"\x03", b"\x04"):
            return "quit"
        if b in (b"\r", b"\n"):
            return "enter"
        if b in (b"\x7f", b"\x08"):
            return "back"
        if b == b"\t":
            return "tab"
        if b == b"\x1b":
            seq = b""
            if select.select([self.fd], [], [], 0.04)[0]:
                seq += os.read(self.fd, 1)
            if seq == b"[" and select.select([self.fd], [], [], 0.04)[0]:
                seq += os.read(self.fd, 1)
            if seq == b"[A":
                return "up"
            if seq == b"[B":
                return "down"
            if seq == b"[C":
                return "right"
            if seq == b"[D":
                return "left"
            if seq == b"[Z":
                return "shift-tab"
            return "esc"
        try:
            ch = b.decode("utf-8")
        except Exception:
            return None
        return ch if ch.isprintable() else None

    def on_key_home(self, key: str) -> bool:
        ms = self.visible_cmds() if self.buf.startswith("/") else []
        if key == "quit":
            return False
        if key == "esc":
            self.buf = ""
            self.sel = 0
            return True
        if not self.buf:
            if key in ("s", "S"):
                self.do_start()
                return True
            if key in ("t", "T"):
                self.do_stop()
                return True
            if key in ("q", "Q"):
                return False
            if key in ("+", "="):
                self.perf_step(1)
                return True
            if key in ("-", "_"):
                self.perf_step(-1)
                return True
        if key == "up":
            if ms:
                self.sel = (self.sel - 1) % len(ms)
            return True
        if key == "down":
            if ms:
                self.sel = (self.sel + 1) % len(ms)
            return True
        if key == "tab":
            sel = self.selected()
            if sel:
                self.buf = sel.name
                self.sel = 0
            return True
        if key == "back":
            self.buf = self.buf[:-1]
            self.sel = 0
            return True
        if key == "enter":
            text = self.buf.strip()
            sel = self.selected()
            action = resolve_action(text, sel)
            self.buf = ""
            self.sel = 0
            if not text:
                return True
            parts = text.split()
            if parts[0] in TYPED and len(parts) > 1:
                self.run_typed(parts)
                return True
            if action:
                self.note_recent(action)
                return self.run_action(action)
            self.add("user", text=text)
            self.say("Unknown command. Type / for the list.")
            return True
        if key and len(key) == 1:
            if self.buf.startswith("/") and key.isdigit() and key != "0":
                i = int(key) - 1
                if i < len(ms):
                    self.buf = ""
                    self.sel = 0
                    self.note_recent(ms[i].action)
                    return self.run_action(ms[i].action)
                return True
            self.buf += key
            self.sel = 0
        return True

    def on_key_overlay(self, key: str) -> bool:
        if key == "quit":
            return False
        if key == "esc":
            self.mode = "home"
            self.buf = ""
            return True
        if TABS[self.tab] == "Fleet":
            if key in ("up", "down"):
                self.pick_step(-1 if key == "up" else 1)
                return True
            if key in ("s", "S", "t", "T", "r", "R"):
                cmd = {"s": "start", "t": "stop", "r": "restart"}[key.lower()]
                row = self.picked_row(self.fleet_rows())
                if row is None or row["here"]:
                    if cmd == "restart":
                        self.run_action("restart")
                    else:
                        (self.do_start if cmd == "start" else self.do_stop)()
                    if self.mode == "home":
                        self.mode = "overlay"
                    return True
                self.add("user", text=f"{key.lower()} · {control.short(row['worker'])}")
                self.fleet_cmd(cmd, workers=[row["worker"]])
                return True
        if key in ("s", "S"):
            self.do_start()
            self.mode = "overlay"
            return True
        if key in ("t", "T"):
            self.do_stop()
            self.mode = "overlay"
            return True
        if key in ("q", "Q"):
            return False
        if key in ("left", "shift-tab"):
            self.tab = (self.tab - 1) % len(TABS)
            return True
        if key in ("right", "tab"):
            self.tab = (self.tab + 1) % len(TABS)
            return True
        if key == "e" and TABS[self.tab] == "Logs":
            i = LOG_VIEWS.index(self.log_which) if self.log_which in LOG_VIEWS else 0
            self.log_which = LOG_VIEWS[(i + 1) % len(LOG_VIEWS)]
            return True
        if TABS[self.tab] == "Config":
            if key in ("+", "="):
                self.perf_step(1)
                return True
            if key in ("-", "_"):
                self.perf_step(-1)
                return True
            if key == "e":
                self.do_edit()
                return True
            env = self.env()
            typed = {
                "a": ["/perf", "auto"],
                "x": ["/perf", "max"],
                "o": ["/perf", "eco"],
                "m": ["/set", "MODE=" + ("light" if env.get("MODE") == "fast" else "fast")],
                "y": ["/set", "YIELD=" + ("off" if env.get("YIELD") == "on" else "on")],
                "p": ["/set", "PAUSE=" + ("off" if pause_label(env) else "120")],
            }
            if key in typed:
                self.run_typed(typed[key])
                return True
        if key in "1234":
            i = int(key) - 1
            if i < len(TABS):
                self.tab = i
            return True
        return True

    def on_key_confirm(self, key: str) -> bool:
        if key in ("quit", "esc"):
            self.mode = "home"
            self.confirm_buf = ""
            self.say("Cancelled.")
            return key != "quit"
        if key == "back":
            self.confirm_buf = self.confirm_buf[:-1]
            return True
        if key == "enter":
            if self.confirm_buf.strip() == "yes":
                self.do_bench()
            else:
                self.mode = "home"
                self.confirm_buf = ""
                self.say("Cancelled.")
            return True
        if key and len(key) == 1:
            self.confirm_buf += key
        return True

    def claim_window(self) -> None:
        """logs/ui.pid tells this Mac's helper a window is open (it hands commands to it), then make
        sure the helper runs (not on the main Mac) and start the event feed."""
        try:
            control.write_json(control.UI_PID, {"pid": os.getpid(), "started": time.time()})
        except OSError:
            pass
        threading.Thread(target=control.ensure, name="helper", daemon=True).start()
        self.feed()

    def release_window(self) -> None:
        try:
            if (control.read_json(control.UI_PID) or {}).get("pid") == os.getpid():
                os.remove(control.UI_PID)
        except OSError:
            pass

    def loop(self) -> None:
        self.take_tty()
        wr = None
        old_wakeup = None
        old_winch = None
        try:
            rr, wr = os.pipe()
            os.set_blocking(rr, False)
            os.set_blocking(wr, False)
            self._winch_r = rr
            try:
                old_wakeup = signal.set_wakeup_fd(wr)
            except Exception:
                old_wakeup = None

            def _winch(_sig, _frm):
                self._resized = True

            old_winch = signal.signal(signal.SIGWINCH, _winch)
        except Exception:
            self._winch_r = None
        # alt screen: Terminal.app will not reflow the previous frame while the
        # window is dragged. wrap off: a one-cell mismatch cannot wrap a row.
        sys.stdout.write(ALT_ON + HIDE + WRAP_OFF + f"\033]11;{GROUND}\007")
        sys.stdout.flush()
        self.claim_window()
        try:
            self._clear_next = True
            self.draw()
            while True:
                key = self.read_key()
                resized = self._resized or self.size()
                if key is None:
                    self.poll_inbox()
                    if self.drain_remote():
                        self.draw()
                        continue
                    self.book_feed()
                    if self.perf_target is not None and time.time() - self.perf_at >= PERF_DELAY:
                        self.apply_perf()
                        self.draw()
                        continue
                    if resized:
                        cached = self.live_cache[1] if self.live_cache else None
                        self.draw(force=True, live=cached)
                    else:
                        state = self.live_cache[1]["state"] if self.live_cache else "STOPPED"
                        period = 0.4 if state != "STOPPED" else 1.0
                        if time.time() - self.last_draw >= period:
                            self.draw(force=False)
                    continue
                if self.mode == "overlay":
                    ok = self.on_key_overlay(key)
                elif self.mode == "ask":
                    ok = self.on_key_ask(key)
                elif self.mode == "confirm":
                    ok = self.on_key_confirm(key)
                else:
                    ok = self.on_key_home(key)
                if not ok:
                    break
                self.draw()
        finally:
            self.release_window()
            if old_winch is not None:
                signal.signal(signal.SIGWINCH, old_winch)
            try:
                signal.set_wakeup_fd(old_wakeup if isinstance(old_wakeup, int) and old_wakeup >= 0 else -1)
            except Exception:
                pass
            if self._winch_r is not None:
                try:
                    os.close(self._winch_r)
                except Exception:
                    pass
                self._winch_r = None
            if wr is not None:
                try:
                    os.close(wr)
                except Exception:
                    pass
            self.restore_tty()
            sys.stdout.write(SHOW + WRAP_ON + RESET + "\033]111\007" + ALT_OFF)
            sys.stdout.flush()


# ---------------------------------------------------------------------- tests / dumps
def _demo_live(state: str = "RUNNING") -> dict:
    job = parse_job()
    api = {
        "hashrate": {"total": [4178.4, 4062.0, 4166.1], "highest": 4201.3},
        "connection": {"pool": "gulf.moneroocean.stream:20016", "ping": 143, "failures": 1, "tls": "TLSv1.3", "accepted": 1945, "rejected": 0, "uptime": 58080},
        "results": {"diff_current": 125113, "shares_good": 1945, "shares_total": 1945, "avg_time": 30},
        "cpu": {"brand": "Apple M4"},
        "version": "6.26.0",
        "worker_id": "demo-mac.local",  # xmrig reports the host name here unless --api-worker-id is set
        "uptime": 58080,
        "hugepages": [0, 1178],
    }
    empty_log = {"shares": [], "errors": []}
    if state == "STOPPED":
        return {"state": "STOPPED", "job": job, "api": None, "hs": None, "highest": PEAK_HS, "acc": 0, "rej": 0, "up": 0, "algo": "rx/0", "hugepages": None, "log": empty_log}
    return {"state": state, "job": job, "api": api, "hs": 4178.4 if state == "RUNNING" else None, "highest": PEAK_HS, "acc": 1945, "rej": 0, "up": 58080, "algo": "rx/0", "hugepages": [0, 1178], "log": empty_log}


def _demo_fleet() -> tuple:
    now = time.time()
    base = {"hs15": None, "acc": None, "rej": None, "up": None, "ping": None, "note": "", "host": "", "pool_acc": None,
            "ctl": None, "ctl_st": None}
    hello = {"v": 1, "worker": "minerv3-m2-8gb", "window": True, "xmrig": True, "can": ["start", "stop", "restart"],
             "key": control.fingerprint(control.pub_line())}
    rows = [
        dict(base, worker="minerv3-m4-16gb", here=True, host="127.0.0.1", via="lan", state="mining", hs=4178.4,
             hs15=4166.1, acc=1945, rej=0, up=58080, pool_hs=4012.0, lts=now - 12, pool_acc=4081),
        dict(base, worker="minerv3-m2-8gb", here=False, host="192.0.2.23", via="lan", state="mining", hs=3237.1,
             hs15=3190.0, acc=812, rej=0, up=18120, pool_hs=3237.1, lts=now - 4, pool_acc=19695, ctl=hello, ctl_st="ok"),
        dict(base, worker="minerv3-i7-6700hq-16gb", here=False, via="pool", state="pool", hs=None, pool_hs=687.2,
             lts=now - 16, pool_acc=395, note="not found on this LAN yet: update it (reopen XMR Miner there, then s)"),
    ]
    meta = {"total": 4178.4 + 3237.1 + 687.2, "mining": 3, "macs": 3, "pool": "ok", "pool_at": now - 40,
            "pool_total": 7936.3, "scan_at": now - 180, "token": True, "at": now,
            "balance": {"due": 0.001715517028, "paid": 0.0, "txns": 0, "threshold": 0.3}}
    return rows, meta


def _demo_events() -> list:
    now = time.time()
    return [
        {"k": "start", "ts": now - 58080, "worker": "minerv3-m4-16gb", "why": "window"},
        {"k": "reject", "ts": now - 30000, "worker": "minerv3-m4-16gb", "n": 722, "diff": 131388, "ms": 67148,
         "why": "Throttled down share submission (please increase difficulty)"},
        {"k": "error", "ts": now - 9000, "last": now - 8975, "n": 6, "worker": "minerv3-m2-8gb",
         "msg": 'connect error: "connection timed out"', "pool": "gulf.moneroocean.stream:20016", "idle": False},
        {"k": "backup", "ts": now - 8974, "worker": "minerv3-m2-8gb", "pool": "de.moneroocean.stream:20016"},
        {"k": "pool", "ts": now - 7000, "worker": "minerv3-m2-8gb", "pool": "gulf.moneroocean.stream:20016", "main": True},
        {"k": "stop", "ts": now - 600, "worker": "minerv3-m2-8gb", "why": "remote", "by": "minerv3-m4-16gb", "up": 18120, "acc": 812, "rej": 0},
        {"k": "start", "ts": now - 540, "worker": "minerv3-m2-8gb", "why": "remote", "by": "minerv3-m4-16gb"},
    ]


def demo_app(kind: str, cols: int = 110, rows: int = 36) -> App:
    """A frame from canned data, for --dump and the self-test. Touches no miner."""
    app = App(dump=True, cols=cols, rows=rows)
    app.size = lambda: None  # type: ignore
    app.cols, app.rows = cols, rows
    stopped = kind.endswith("-stopped")
    starting = kind.endswith("-starting")
    paused = kind.endswith("-paused")
    kind = kind.replace("-stopped", "").replace("-starting", "").replace("-paused", "")
    live = _demo_live("STOPPED" if stopped else "STARTING" if starting else "RUNNING")
    if paused:
        # what xmrig reports under --pause-on-active: no 10 s or 60 s rate, "paused": true
        api = dict(live["api"], paused=True, hashrate={"total": [None, None, 3312.6], "highest": 4201.3})
        live = dict(live, api=api, hs=None, paused=True)
    app.fixed_live = live
    app.fleet_fixed = _demo_fleet()
    app.feed_fixed = _demo_events()
    app.key_fixed = True
    app.main = True
    app.env_fixed = {"ARCH": "arm64", "CORES": "10", "AUTO_THREADS": "10", "THREADS": "10", "THREADS_SPEC": "auto",
                     "MODE": "fast", "MODE_SPEC": "auto", "WORKER": "minerv3-m4-16gb",
                     "POOL": "gulf.moneroocean.stream:20016", "BACKUP": "de.moneroocean.stream:20016",
                     "TLS": "on", "YIELD": "off", "PAUSE": "120", "LAN": "on"}
    now = time.time()
    if starting:
        app.start_ts = now - 3.2
        app.events = [
            {"k": "user", "ts": now - 3.2, "text": "s"},
            {"k": "start", "ts": now - 3.2, "cmd": live["job"].get("cmdline") or "caffeinate -i xmrig -a rx/0 --threads=10 --randomx-mode=fast"},
            {"k": "dsprog", "ts": now - 3.1},
        ]
    elif stopped:
        app.events = [
            {"k": "user", "ts": now - 4200, "text": "t"},
            {"k": "stop", "ts": now - 4000, "up": 58080, "acc": 1945, "rej": 0, "avg": 4166.1},
            {"k": "summary", "ts": now - 3900, "path": os.path.expanduser("~/Desktop/XMR-miner-summary-2026-09-12_124410.txt")},
        ]
    else:
        st = now - 58080
        app.start_ts = st
        app.events = [
            {"k": "user", "ts": st, "text": "s"},
            {"k": "start", "ts": st, "cmd": live["job"].get("cmdline") or "caffeinate -i xmrig -a rx/0 --threads=10 --randomx-mode=fast"},
            {"k": "dataset", "ts": st + 7, "secs": 6.6},
            {"k": "pool", "ts": st + 7},
            {"k": "warn", "ts": now - 42000, "title": "Hashrate dipped to 3,651 H/s for 8 s", "sub": "macOS memory compressor was active, recovered on its own"},
            {"k": "shares", "ts": st + 8},
        ]
        n = 1945
        t = now - 12
        for i in range(4):
            app.share_rows.insert(0, {"n": n, "ts": t, "diff": 125113 - (1945 - n) * 37, "ms": 143 - i * 4, "ok": True, "src": "log" if i < 3 else "api"})
            n -= 1
            t -= 29
    app.prev_state = live["state"]
    if not stopped:
        import math
        app.hist = [4150 + 42 * math.sin(i / 9) + (18 if i % 7 == 0 else -9) for i in range(160)]
        for i in range(118, 126):
            app.hist[i] = 3651 + abs(i - 121.5) * 70
        if paused:
            app.hist[-24:] = [None] * 24
            app.events.append({"k": "pause", "ts": now - 95})
    if kind in ("palette", "slash", "/"):
        app.force_palette = "/" if kind != "palette" else "/usage"
    elif kind in ("usage", "fleet", "config", "logs", "err"):
        app.mode = "overlay"
        app.tab = TABS.index({"usage": "Usage", "fleet": "Fleet", "config": "Config"}.get(kind, "Logs"))
        app.log_which = "err" if kind == "err" else "log"
    elif kind == "confirm":
        app.mode = "confirm"
        app.events.append({"k": "user", "ts": now, "text": "/bench"})
    return app


def dump_frame(kind: str, cols: int = 110, rows: int = 36, strip: bool = False) -> str:
    app = demo_app(kind, cols, rows)
    lines = app.compose(app.live())
    out = "\n".join(lines) + "\n"
    return plain(out) if strip else out


def self_test() -> int:
    fails = 0

    def check(name: str, cond: bool) -> None:
        nonlocal fails
        print(("ok  " if cond else "FAIL") + " " + name)
        if not cond:
            fails += 1

    ms = filter_cmds("/")
    check("slash lists all", len(ms) == len(COMMANDS))
    ms = filter_cmds("/us")
    check("/us matches usage", any(c.name == "/usage" for c in ms) and ms[0].name == "/usage")
    check("/usage exact first", filter_cmds("/usage")[0].name == "/usage")
    check("unknown empty", filter_cmds("/xyznope") == [])
    check("alias /stats", resolve_action("/stats", None) == "usage")
    check("alias /plist", resolve_action("/plist", None) == "config")
    check("tabs are Usage Fleet Config Logs", TABS == ("Usage", "Fleet", "Config", "Logs"))
    check("alias /macs", resolve_action("/macs", None) == "fleet" and resolve_action("/fleet", None) == "fleet")
    check("hugepages list", fmt_hugepages([2080, 2080]) == "2080/2080 (100%)")
    check("hugepages bool", fmt_hugepages(True) == "yes" and fmt_hugepages(False) == "no")
    check("hugepages none", fmt_hugepages(None) == "—")
    check("uptime", fmt_uptime(125) == "2m 5s" and fmt_clock(58092) == "16h 08m 12s")
    check("bar has fill", "█" in bar(0.33, 10) and "░" in bar(0.33, 10) and plain(bar(0.5, 4)) == "[██░░]")
    check("clip_row pads", vis_len(clip_row("ab", 5)) == 5 and clip_row("ab", 5).endswith("   "))
    check("fit_row exact", vis_len(fit_row(" a", "b", 40)) == 40 and plain(fit_row(" a", "b", 40)).endswith("b"))
    job = parse_job()
    check("plist threads", job.get("threads") not in ("", None))
    check("plist cmdline hides wallet", "caffeinate -i xmrig" in job.get("cmdline", "") and "-u" not in job.get("cmdline", ""))
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".plist") as tf:
        plistlib.dump({"ProgramArguments": ["xmrig", "-u", "4" + "8" * 94]}, tf)
        tf.flush()
        check("bare -u wallet is never the worker", parse_job(tf.name)["worker"] == "-")
    with tempfile.NamedTemporaryFile(suffix=".plist") as tf:
        u = "4" + "8" * 94 + ".minerv3-m4-16gb"
        plistlib.dump({"ProgramArguments": ["xmrig", "-o", "gulf.moneroocean.stream:20016", "-u", u, "-a", "rx/0", "-k", "--tls",
                                            "-o", "de.moneroocean.stream:20016", "-u", u, "-a", "rx/0", "-k", "--tls",
                                            "--threads=10"]}, tf)
        tf.flush()
        two = parse_job(tf.name)
        check("two pools: the first is the pool", two["pool"] == "gulf.moneroocean.stream:20016" and two["threads"] == "10")
        check("two pools: the second is the backup", two["backup"] == "de.moneroocean.stream:20016")
        check("on_backup", on_backup({"pool": "de.moneroocean.stream:20016"}, two)
              and not on_backup({"pool": "gulf.moneroocean.stream:20016"}, two)
              and not on_backup({"pool": ""}, {"backup": ""}))
    snap = session_snapshot(_demo_live())
    check("session snapshot", snap["acc"] == 1945 and snap["hs10"] == 4178.4 and "moneroocean" in snap["pool"])
    check("snapshot worker is not the host", snap.get("worker") != "demo-mac.local")
    check("tls_label version", tls_label({"tls": "TLSv1.3"}, {"tls": True}) == "TLSv1.3")
    check("tls_label empty uses plist", tls_label({}, {"tls": True}) == "TLS" and tls_label({"tls": ""}, {"tls": True}) == "TLS")
    check("tls_label plain", tls_label({}, {"tls": False}) == "plain")
    lg = parse_log([
        "[2026-09-12 12:43:20.101]  net      use pool gulf.moneroocean.stream:20016  TLSv1.3",
        "[2026-09-12 12:43:20.140]  randomx  allocated 2336 MB (2080+256) huge pages 0% 0/1178 +JIT (4 ms)",
        "[2026-09-12 12:43:26.760]  randomx  dataset ready (6620 ms)",
        "[2026-09-12 12:43:58.123]  cpu      accepted (1945/0) diff 125113 (143 ms)",
        '[2026-09-12 12:44:11.900]  cpu      rejected (1945/1) diff 125113 "Low difficulty share" (140 ms)',
        "[2026-09-12 12:44:20.000]  miner    speed 10s/60s/15m 4178.4 4062.0 4166.1 H/s max 4201.3 H/s",
        "not a log line",
    ])
    check("parse_log shares", len(lg["shares"]) == 2 and lg["shares"][0]["ms"] == 143 and lg["shares"][0]["ok"] and not lg["shares"][1]["ok"] and lg["shares"][1]["why"] == "Low difficulty share")
    check("parse_log dataset/alloc/pool", lg["dataset_ms"] == 6620 and lg["alloc"]["mb"] == 2336 and lg["alloc"]["hp"] == [0, 1178] and lg["pool"] == "gulf.moneroocean.stream:20016" and lg["speed"].startswith("speed"))
    check("parse_log timestamp", time.strftime("%H:%M:%S", time.localtime(lg["shares"][0]["ts"])) == "12:43:58")
    lg_err = parse_log([
        '[2026-09-13 12:20:01.001]  net      [gulf.moneroocean.stream:20016] 1.2.3.4 connect error: "connection timed out"',
        '[2026-09-13 12:20:08.010]  net      DNS error: "temporary failure in name resolution"',
        '[2026-09-13 12:20:09.000]  http     HTTP API 127.0.0.1:18088 bind failed "address already in use"',
    ])
    check("parse_log connect error", lg_err["errors"][0]["kind"] == "connect" and lg_err["errors"][0]["msg"] == "connection timed out")
    check("parse_log dns then bind", lg_err["errors"][1]["kind"] == "dns" and lg_err["errors"][2]["kind"] == "bind")
    check("last_error_msg connect", last_error_msg({"errors": [{"kind": "connect", "msg": "connection timed out"}]}) == 'xmrig: connect error "connection timed out"')
    app = App(dump=True)
    app.prev_state = "STARTING"
    app.start_ts = time.time() - 2
    app.track({"state": "STOPPED", "api": {}, "acc": 0, "rej": 0, "up": 0, "log": {"shares": [], "errors": []}}, time.time())
    check("no false stop during start", not any(e["k"] == "stop" for e in app.events) and app.prev_state == "STARTING")
    app = App(dump=True)
    app.prev_state = "STARTING"
    app.start_ts = time.time() - 3
    app.last_fail = 0
    live_left = _demo_live("RUNNING")
    live_left["log"] = {"shares": [], "errors": [{"ts": time.time(), "kind": "connect", "msg": "connection timed out"}]}
    app.track(live_left, time.time())
    check("leftover attach warn", any(e["k"] == "warn" and "leftover" in e.get("title", "") for e in app.events))
    app.last_fail = 1
    live_left["api"]["connection"]["failures"] = 2
    app.track(live_left, time.time())
    fail_ev = [e for e in app.events if e["k"] == "warn" and "Pool connection failed" in e.get("title", "")]
    check("pool fail quotes xmrig log", bool(fail_ev) and 'connect error "connection timed out"' in fail_ev[-1].get("sub", ""))

    for kind in ("home", "home-stopped", "palette", "slash", "usage", "fleet", "config", "logs", "confirm"):
        for cols, rows in ((110, 36), (80, 24), (60, 18)):
            frame = dump_frame(kind, cols, rows)
            lines = frame.rstrip("\n").split("\n")
            widths = {vis_len(ln) for ln in lines}
            check(f"{kind} {cols}x{rows}: {rows} rows, every row {cols} cells", len(lines) == rows and widths == {cols})
    home = dump_frame("home", strip=True)
    check("header card", ">_ XMR Miner" in home and "job:" in home and "pool:" in home and "folder:" in home and "╭" in home and "╯" in home)
    folded = dump_frame("home", 90, 36, strip=True)
    check("now row (folded layout)", "now:     4,178 H/s  [" in folded and "99% · 1,945 ✓ · 16h 8m " in folded and "…" not in [ln for ln in folded.split("\n") if "now:" in ln][0])
    check("api-estimated latency marked", "~131 ms" in home and " 143 ms" in home)
    small = dump_frame("home", 80, 24, strip=True)
    check("compact card is 3 rows", small.startswith("╭─ >_ XMR Miner · rx/0 fast · 10 threads · gulf.moneroocean.stream ─") and "─╮" in small.split("\n")[0] and small.split("\n")[1].startswith("│ now:  4,178 H/s") and "…" not in small.split("\n")[1] and small.split("\n")[2].startswith("╰"))
    check("stopped now row (folded layout)", "now:     not mining" in dump_frame("home-stopped", 90, 36, strip=True))
    rail = dump_frame("home", 110, 36, strip=True).split("\n")
    check("rail present at 110 cols", any("│  hashrate" in ln for ln in rail) and any("│  shares" in ln for ln in rail) and any("│  threads" in ln for ln in rail) and any("│  pool" in ln for ln in rail) and any("│  balance" in ln for ln in rail) and any("│  dataset" in ln for ln in rail))
    app_f110 = demo_app("home", 110, 36); app_f110.main = False
    check("follower rail at 110 cols keeps session", any("│  session" in ln for ln in (plain(x) for x in app_f110.compose(app_f110.live()))))
    check("rail sparkline + windows", any(ch in "".join(rail) for ch in "⣿⣶⣤⣀") and any("10s 4,178 · 60s 4,062 · 15m 4,166" in ln for ln in rail))
    check("rail hides the now row", not any("now:" in ln for ln in rail) and any("99% of peak" in ln for ln in rail))
    check("rail folds below 100 cols", not any("│  hashrate" in ln for ln in dump_frame("home", 99, 36, strip=True).split("\n")) and "now:" in dump_frame("home", 99, 36, strip=True))
    check("rail band spans left pane only", any(ln.startswith(" › Type / for commands") and "│" in ln for ln in rail))
    check("braille shape", braille_rows([0, 100], 1, 1, 0, 100) == [chr(0x2800 | 0x40 | 0x80 | 0x20 | 0x10 | 0x08)] and braille_rows([100, 100], 1, 2, 0, 100) == ["⣿", "⣿"] and braille_rows([50, 50], 1, 2, 0, 100) == [" ", "⣿"])
    check("starting now row (folded layout)", "now:     building the dataset · 3s" in dump_frame("home-starting", 90, 36, strip=True))
    check("starting rail", "│  warming up" in dump_frame("home-starting", 110, 36, strip=True) and "building" in dump_frame("home-starting", 110, 36, strip=True))
    check("user turn band", " › s" in home)
    check("bullets and trees", "• Started xmrig" in home and "└ caffeinate -i xmrig" in home)
    check("pool line names the pool worker, not the host", "· worker " in home and "demo-mac.local" not in home)
    check("shares ledger", "• Shares  1,945 accepted" in home and "#1,945" in home and "diff 125,113" in home and "143 ms" in home and "✓" in home)
    check("status shimmer line", "Mining (16h 08m" in home and "t to stop)" in home)
    check("band placeholder", "› Type / for commands, t to stop" in home)
    check("footer keys", "↵ run" in home and "⌃C quit" in home and "4,178 H/s · 99% of peak" in home)
    stopped = dump_frame("home-stopped", strip=True)
    check("stopped: summary + placeholder", "• Stopped after 16h 8m" in stopped and "Wrote ~/Desktop/XMR-miner-summary" in stopped and "s to mine" in stopped and "not mining" in stopped and "Mining (" not in stopped)
    app_r = demo_app("home", 152, 49)
    f152 = app_r.compose(app_r.live())
    p152 = [plain(ln) for ln in f152]
    check("compose 152x49", len(f152) == 49 and {vis_len(ln) for ln in f152} == {152} and any("│  hashrate" in ln for ln in p152))
    app_r.cols, app_r.rows = 90, 24
    f90 = app_r.compose(app_r.live())
    p90 = "\n".join(plain(ln) for ln in f90)
    check("resize 152→90 folds rail live", len(f90) == 24 and {vis_len(ln) for ln in f90} == {90} and "│  hashrate" not in p90 and "now:" in p90)
    app_r.cols, app_r.rows = 110, 36
    f110 = app_r.compose(app_r.live())
    p110 = [plain(ln) for ln in f110]
    check("resize 90→110 restores rail", len(f110) == 36 and {vis_len(ln) for ln in f110} == {110} and any("│  hashrate" in ln for ln in p110))
    pal = dump_frame("slash", strip=True)
    check("palette groups + digits + status", " miner" in pal and " actions" in pal and " fleet" in pal and " ui" in pal and "▎1 /usage" in pal and " 9 /pause" in pal and "   /help" in pal
          and "   /start" in pal and "   /stop" in pal and "   /events" in pal
          and "   /quit" in pal and "4,178 H/s · 1,945 ✓" in pal and "off · rx/0 only" in pal and "running · will refuse" in pal and "Type / for" not in pal)
    check("palette fleet status (compact beside the rail)", " 2 /fleet" in pal and "3/3 · 8,103 H/s" in pal)
    wide = dump_frame("slash", 147, 58, strip=True)  # the launcher size: a 109-col pane beside the rail
    check("palette fleet status (wide)", "3 of 3 mining · 8,103 H/s" in wide)
    check("palette hints row", "1 of 14" in pal and "1–9 run" in pal)
    pal2 = dump_frame("palette", strip=True)
    pal_rows = [ln for ln in pal2.split("\n") if "/usage" in ln or ln.strip() in ("miner", "actions", "ui", "recent")]
    check("palette filter is flat + selected", not any(ln.strip() in ("miner", "actions", "ui") for ln in pal2.split("\n")) and "▎1 /usage" in pal2 and "1 of 1" in pal2)
    small = dump_frame("slash", 60, 18, strip=True)
    check("palette drops headers when short", not any(ln.strip() == "actions" for ln in small.split("\n")) and "/usage" in small)
    appf = demo_app("home"); appf.force_palette = None; appf.buf = "/lo"
    flt = plain("\n".join(appf.compose(appf.live())))
    check("typed prefix filters to /logs first", "▎1 /logs" in flt and "/usage" not in flt.split("▎1 /logs")[1].split("›")[0])
    app = demo_app("home"); app.buf = "/"; app.recent = ["logs", "flex"]
    check("recent group first", [ln[1].action for ln in app.palette_lines(True) if ln[0] == "c"][:2] == ["logs", "flex"] and app.palette_lines(True)[0] == ("h", "recent"))
    app.on_key_home("3"); check("digit runs the 3rd visible command (/usage)", app.mode == "overlay" and app.tab == TABS.index("Usage") and app.buf == "")
    usage = dump_frame("usage", strip=True)
    check("usage card", "› /usage" in usage and "XMR Miner · usage" in usage and "Hashrate:" in usage and "[" in usage and "Windows:" in usage and "Cadence:" in usage and "Uptime:      16h 08m 00s" in usage)
    config = dump_frame("config", strip=True)
    check("config card", "Algorithm:" in config and "Overrides:" in config and "+ − threads" in config and "e edit" in config)
    check("config threads row", "Threads:     10 of 10" in config and "]  auto" in config and "about 100% of this Mac" in config)
    check("config footer keys", "+− threads" in config and "e edit" in config)
    check("typed hints", typed_hint("/perf 4") == "↵ threads → 4" and "KEY=value" in typed_hint("/set") and typed_hint("/usage") == "")
    check("alias /perf opens config", resolve_action("/perf", None) == "config" and resolve_action("/threads", None) == "config")

    def ctl_app(state: str, threads: str = "3", cores: str = "8") -> tuple:
        a = demo_app("home" if state == "RUNNING" else "home-stopped")
        a.env_fixed = dict(a.env_fixed, THREADS=threads, CORES=cores)
        calls: list = []
        a.ctl = lambda *args: calls.append(args)  # type: ignore
        return a, calls

    a, calls = ctl_app("RUNNING")
    a.on_key_home("+"); a.on_key_home("+")
    check("+ while mining waits (one restart)", a.perf_target == 5 and calls == [])
    check("footer shows the pending change", "threads 3 → 5 · restarting in" in plain(a.footer_row(a.live())))
    a.on_key_home("-"); a.on_key_home("-")
    check("+ + − − cancels", a.perf_target is None and calls == [])
    a.on_key_home("-"); a.perf_at -= PERF_DELAY; a.apply_perf()
    check("pending applies as /perf N", calls == [("perf", "2")] and a.perf_target is None)
    a, calls = ctl_app("STOPPED")
    a.on_key_home("+")
    check("+ while stopped applies at once", calls == [("perf", "4")])
    a, calls = ctl_app("RUNNING", threads="8")
    a.on_key_home("+")
    check("+ at the top says so", calls == [] and a.perf_target is None and "Already at the top" in a.ctl_note)
    a, calls = ctl_app("RUNNING")
    a.buf = "/perf max"; a.on_key_home("enter")
    a.buf = "/set MODE=light YIELD=on"; a.on_key_home("enter")
    check("typed /perf and /set", calls == [("perf", "max"), ("config", "set", "MODE=light", "YIELD=on")])
    a, calls = ctl_app("RUNNING")
    a.mode = "overlay"; a.tab = TABS.index("Config")
    for k in ("m", "y", "a", "x", "o"):
        a.on_key_overlay(k)
    check("config card keys", calls == [("config", "set", "MODE=light"), ("config", "set", "YIELD=on"), ("perf", "auto"), ("perf", "max"), ("perf", "eco")] and a.mode == "overlay")
    check("config priority", "Priority:" in config and "nice 0" in config)
    check("live_state: paused is not the dataset build",
          live_state({"paused": True, "hashrate": {"total": [None, None, 3900.0]}}, True) == ("RUNNING", None, True))
    check("live_state: no rate = starting, no API = starting/stopped",
          live_state({"hashrate": {"total": [None, None, None]}}, True)[0] == "STARTING"
          and live_state(None, True)[0] == "STARTING" and live_state(None, False) == ("STOPPED", None, False))
    check("live_state: mining", live_state({"hashrate": {"total": [4100.0, 4000.0, None]}}, True) == ("RUNNING", 4100.0, False))
    check("pause label", pause_label({"PAUSE": "120"}) == "2 min" and pause_label({"PAUSE": "90"}) == "90 s"
          and pause_label({"PAUSE": "off"}) == "" and pause_label({}) == "")
    hp = dump_frame("home-paused", 147, 58, strip=True)
    check("paused frame", "Paused" in hp and "paused · you're active" in hp and "mines after 2 min" in hp
          and "Building" not in hp and "warming up" not in hp)
    check("config pause row", "Pause:" in config and "2 min idle" in config and "p pause" in config)
    a, calls = ctl_app("RUNNING")
    a.mode = "overlay"; a.tab = TABS.index("Config")
    a.on_key_overlay("p")
    check("config p turns PAUSE off", calls == [("config", "set", "PAUSE=off")])
    a, calls = ctl_app("RUNNING")
    a.env_fixed = dict(a.env_fixed, PAUSE="off")
    a.mode = "overlay"; a.tab = TABS.index("Config")
    a.on_key_overlay("p")
    check("config p turns PAUSE on at 120", calls == [("config", "set", "PAUSE=120")])
    a, calls = ctl_app("RUNNING")
    for typed in ("/pause off", "/pause on", "/pause 300", "/pause 5", "/pause soon"):
        a.buf = typed; a.on_key_home("enter")
    check("typed /pause on|off|N, bad values write nothing",
          calls == [("config", "set", "PAUSE=off"), ("config", "set", "PAUSE=120"), ("config", "set", "PAUSE=300")])
    a, calls = ctl_app("RUNNING")
    a.buf = "/pause"; a.on_key_home("enter")
    check("/pause alone switches it off", calls == [("config", "set", "PAUSE=off")])
    a, calls = ctl_app("RUNNING")
    a.env_fixed = dict(a.env_fixed, PAUSE="off")
    a.buf = "/pause"; a.on_key_home("enter")
    check("/pause alone switches it on at 120", calls == [("config", "set", "PAUSE=120")])
    pc = next(c for c in COMMANDS if c.action == "pause")
    check("/pause status", a.cmd_status(pc, a.live()) == "off · mines while you work"
          and ctl_app("RUNNING")[0].cmd_status(pc, a.live()) == "on · 2 min idle")
    check("/pause hints", typed_hint("/pause 300") == "↵ PAUSE=300 and apply it" and "on | off" in typed_hint("/pause 5")
          and "on/off" in typed_hint("/pause"))
    check("pause_value", pause_value("ON") == "120" and pause_value("off") == "off" and pause_value("3600") == "3600"
          and pause_value("9") is None and pause_value("2m") is None)
    a = demo_app("home")
    base = a.live()
    a.prev_paused = False
    for flag in (True, False, True):
        a.track(dict(base, paused=flag), time.time())
    kinds = [e["k"] for e in a.events if e["k"] in ("pause", "resume")]
    check("pause/resume keep one ledger line", kinds == ["pause"])
    a.track(dict(base, paused=False), time.time())
    check("resume replaces the pause line", [e["k"] for e in a.events if e["k"] in ("pause", "resume")] == ["resume"])
    check("config API row", "HTTP API:" in config and (":18088 · LAN, token from fleet.token" in config or ":18088 · this Mac only" in config))
    fl = dump_frame("fleet", strip=True)
    check("fleet card", "› /fleet" in fl and "XMR Miner · fleet" in fl and "Total:" in fl and "8,103 H/s · 3 of 3 mining" in fl
          and "● minerv3-m4-16gb" in fl and "this Mac" in fl and "◍ minerv3-i7-6700hq-16gb" in fl and "└ not found on this LAN yet" in fl)
    check("fleet card never shows a peer address", "192.0.2.23" not in fl)
    tall = dump_frame("home", 147, 58, strip=True).split("\n")
    check("rail fleet section at 147x58", any("│  fleet" in ln and "3 of 3 · 8,103 H/s" in ln for ln in tall)
          and any("m2-8gb" in ln and "3,237 H/s  LAN" in ln for ln in tall) and any("i7-6700hq-16gb" in ln and "pool" in ln for ln in tall))
    check("rail balance under pool (main Mac)", any("│  balance" in ln for ln in tall) and any("0.001716 XMR due" in ln for ln in tall)
          and any("] 0.6% to payout" in ln for ln in tall) and any("payout at 0.3 XMR · 0 paid" in ln for ln in tall)
          and next(i for i, ln in enumerate(tall) if "│  pool" in ln) < next(i for i, ln in enumerate(tall) if "│  balance" in ln))
    check("rail fleet shares (main Mac)", any("└ 19,695 ✓ pool · 812 this run" in ln for ln in tall)
          and any("└ 395 ✓ pool" in ln for ln in tall) and any("└ 4,081 ✓ pool · 1,945 this run" in ln for ln in tall))
    app_fw = demo_app("home", 147, 58); app_fw.main = False
    fw = [plain(x) for x in app_fw.compose(app_fw.live())]
    check("follower rail: no balance, no share lines", not any("balance" in ln or "✓ pool" in ln for ln in fw)
          and any("│  fleet" in ln for ln in fw))
    app_nf = demo_app("home", 147, 58); app_nf.fleet_fixed = None
    check("rail fleet before the first poll", any("│  fleet" in ln and "looking…" in ln for ln in (plain(x) for x in app_nf.compose(app_nf.live()))))
    logs = dump_frame("logs", strip=True)
    check("logs tail", "• Ran tail -n 20 logs/xmrig.log" in logs)
    confirm = dump_frame("confirm", strip=True)
    check("confirm band", "› yes" in confirm and "Offline sweep" in confirm and "esc cancel" in confirm)
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "update-notice.json")
        with open(p, "w", encoding="utf-8") as f:
            json.dump({"status": "ok", "to": "abc1234", "count": 5, "commits": ["One", "Two", "Three"],
                       "was_up": True, "saved": "", "install_ok": True}, f)
        n = take_update_notice(p)
        check("update notice is read once", n is not None and n["to"] == "abc1234" and not os.path.exists(p)
              and take_update_notice(p) is None)
    kind, kw = update_event(n or {})
    au = demo_app("home-stopped", 147, 58)
    au.add(kind, **kw)
    upd = "\n".join(plain(x) for x in au.compose(au.live()))
    check("update notice in the ledger", "Updated to release abc1234 · 5 new commits" in upd and "+2 more" in upd
          and "mining stopped for the update" in upd)
    fk, fkw = update_event({"status": "fail", "reason": "GitHub did not answer in 15 s", "at": "d6712f2"})
    check("update check failure is a warn", fk == "warn" and "still on d6712f2" in fkw["sub"])
    # --- the fleet: commands between Macs, timed lines
    fl2 = [plain(x) for x in (lambda a: (setattr(a, "fleet_pick", "minerv3-m2-8gb"), a.compose(a.live()))[1])(demo_app("fleet", 147, 58))]
    fl2s = "\n".join(fl2)
    check("fleet card: the pick marker follows the picked Mac", any("▸● minerv3-m2-8gb" in ln for ln in fl2) and not any("▸● minerv3-m4" in ln for ln in fl2))
    check("fleet card: what the main Mac can do to each Mac", "└ XMR Miner open there · s start · t stop · r restart" in fl2s
          and "↑↓ pick a Mac · s start · t stop · r restart" in fl2s)
    check("fleet card: recent events with times", "Recent events" in fl2s and "m2-8gb" in fl2s and "Stopped from m4-16gb" in fl2s)
    check("fleet footer keys", "↑↓ pick   s start   t stop   r restart   esc back" in fl2s)
    ev_frame = [plain(x) for x in (lambda a: (setattr(a, "log_which", "events"), a.compose(a.live()))[1])(demo_app("logs", 147, 58))]
    evs = "\n".join(ev_frame)
    check("/events view", "› /events" in evs and "Events · every Mac · newest last" in evs
          and 'Pool connect error: "connection timed out" ×6' in evs and "⇄ Switched to the backup pool" in evs
          and "Share rejected #722" in evs and "(e switches to xmrig.log)" in evs)
    for cols, rows in ((147, 58), (110, 36), (80, 24), (60, 18)):
        for kind, which in (("fleet", "log"), ("logs", "events")):
            a = demo_app(kind, cols, rows)
            a.log_which = which
            fr = a.compose(a.live())
            check(f"{kind}/{which} {cols}x{rows}: every row {cols} cells", len(fr) == rows and {vis_len(x) for x in fr} == {cols})
    a = demo_app("home")
    a.buf = "/stop m2"
    a.on_key_home("enter")
    check("/stop m2 asks first", a.mode == "ask" and a.ask["text"] == "Stop m2-8gb?" and [r["worker"] for r in a.ask["rows"]] == ["minerv3-m2-8gb"])
    ask_frame = "\n".join(plain(x) for x in a.compose(a.live()))
    check("the question row + keys", "• Stop m2-8gb?  y / n" in ask_frame and "y go ahead   n cancel" in ask_frame
          and all(vis_len(x) == 110 for x in a.compose(a.live())))
    a.on_key_ask("x")
    check("other keys keep asking", a.mode == "ask")
    a.on_key_ask("y")
    rem = [e for e in a.events if e["k"] == "remote"]
    check("y sends it (pending line)", a.mode == "home" and rem and rem[-1]["state"] == "pending" and rem[-1]["worker"] == "minerv3-m2-8gb")
    check("pending shows on the rail and card", a.pending_cmd("minerv3-m2-8gb") == "stop"
          and "Stopping m2-8gb from this Mac…" in "\n".join(plain(x) for x in a.compose(a.live())))
    a.remote_q.put((rem[-1], {"ok": True, "via": "window", "acc": 23094, "rej": 14, "up": 3600}))
    a.drain_remote()
    done = "\n".join(plain(x) for x in a.compose(a.live()))
    check("the answer: stopped + its numbers", "• Stopped m2-8gb from this Mac · " in done and "└ 23,094 accepted · 14 rejected · ran 1h 0m · via its window" in done)
    a.remote_q.put(({"k": "remote"} if False else a.add("remote", cmd="start", worker="minerv3-m2-8gb", state="pending"),
                    {"ok": False, "error": "XMR Miner is not open on m2-8gb: open it there to start mining"}))
    a.drain_remote()
    check("a refusal says why", "Could not start m2-8gb" in "\n".join(plain(x) for x in a.compose(a.live())))
    a = demo_app("home")
    a.buf = "/stop all"
    a.on_key_home("enter")
    check("/stop all: every Mac that mines and can take it", a.mode == "ask" and a.ask["text"].startswith("Stop 2 Macs (m4-16gb (this Mac), m2-8gb)")
          and any("i7-6700hq-16gb" in n for n in a.ask["note"]))
    a.on_key_ask("n")
    check("n cancels", a.mode == "home" and a.ask is None and "Cancelled." in "\n".join(plain(x) for x in a.compose(a.live())))
    a = demo_app("home")
    a.buf = "/start m2"
    a.on_key_home("enter")
    check("/start on a Mac that mines says so", a.mode == "home" and "m2-8gb: already mining" in "\n".join(plain(x) for x in a.compose(a.live())))
    a = demo_app("home")
    a.buf = "/stop m"
    a.on_key_home("enter")
    check("an unclear name asks which", "could be m4-16gb, m2-8gb" in "\n".join(plain(x) for x in a.compose(a.live())))
    a = demo_app("home")
    a.main = False
    a.buf = "/stop m2"
    a.on_key_home("enter")
    check("a follower cannot stop another Mac", a.mode == "home" and "only the main Mac" in "\n".join(plain(x) for x in a.compose(a.live())))
    a = demo_app("fleet")
    calls: list = []
    a.do_stop = lambda by="": calls.append(("stop", by)) or (True, [])  # type: ignore
    a.on_key_overlay("t")
    check("t on the Fleet card with this Mac picked stops this Mac (no question)", calls == [("stop", "")] and a.mode == "overlay")
    a.on_key_overlay("down")
    check("↓ picks the next Mac", a.fleet_pick == "minerv3-m2-8gb")
    a.on_key_overlay("r")
    check("r on another Mac asks first", a.mode == "ask" and a.ask["text"] == "Restart m2-8gb?" and a.ask["back"] == "overlay")
    a.on_key_ask("esc")
    check("esc goes back to the card", a.mode == "overlay" and TABS[a.tab] == "Fleet")
    # timed lines in the ledger
    a = demo_app("home", 147, 58)
    a.add("reject", ts=time.time() - 5, n=722, diff=131388, ms=67148, why="Throttled down share submission (please increase difficulty)")
    a.add("backup", pool="de.moneroocean.stream:20016")
    a.add("fevent", ts=time.time() - 3, ev=_demo_events()[2])
    home2 = "\n".join(plain(x) for x in a.compose(a.live()))
    hhmm = re.compile(r" · \d\d:\d\d:\d\d")
    check("start line has its time", any("• Started xmrig" in ln and hhmm.search(ln) for ln in home2.split("\n")))
    check("reject line: time + reason", "• Share rejected #722 · " in home2 and '└ "Throttled down share submission' in home2)
    check("backup line", "• Switched to the backup pool · " in home2)
    check("another Mac's event in the ledger", '• m2-8gb · Pool connect error: "connection timed out" ×6 · ' in home2)
    b = App(dump=True)
    b.prev_state, b.start_ts = "RUNNING", time.time() - 100
    lv = _demo_live("RUNNING")
    t_rej = time.time() - 20
    lv["log"] = {"shares": [{"n": 721, "rej": 1, "ts": t_rej, "diff": 131388, "ms": 67148, "ok": False, "why": "Low difficulty share", "src": "log"}], "errors": []}
    b.track(lv, time.time())
    b.track(lv, time.time())
    rj = [e for e in b.events if e["k"] == "reject"]
    check("a rejected share in the log → one reject line (#722)", len(rj) == 1 and rj[0]["n"] == 722 and rj[0]["ts"] == t_rej)
    lv2 = dict(lv, api=dict(lv["api"], connection=dict(lv["api"]["connection"], pool="de.moneroocean.stream:20016")))
    lv2["job"] = dict(lv["job"], backup="de.moneroocean.stream:20016")
    b.prev_backup = False
    b.track(lv2, time.time())
    check("mining moves to the backup pool → a line", any(e["k"] == "backup" for e in b.events))
    # a command from the main Mac arrives through the inbox
    import tempfile as _tf
    td = _tf.mkdtemp()
    old_root = control.ROOT
    try:
        control.set_root(td)
        c = demo_app("home-stopped")
        c.dump = False
        c.draw = lambda *a, **k: None  # type: ignore
        got: list = []
        c.do_start = lambda by="": got.append(by) or (True, ["Started."])  # type: ignore
        control.write_json(os.path.join(control.INBOX, "a" * 24 + ".json"), {"cmd": "start", "from": "minerv3-m4-16gb", "ts": time.time()})
        c.inbox_at = 0
        c.poll_inbox()
        ans = control.read_json(os.path.join(control.INBOX, "a" * 24 + ".done")) or {}
        check("inbox: a start from the main Mac runs like s, with who asked", got == ["minerv3-m4-16gb"] and ans.get("ok") is True
              and ans.get("lines") == ["Started."])
    finally:
        control.set_root(old_root)
        import shutil as _sh
        _sh.rmtree(td, ignore_errors=True)
    d = demo_app("home-stopped")
    d.fixed_live = _demo_live("STOPPED")
    import types
    real_co = subprocess.check_output
    try:
        subprocess.check_output = lambda *a, **k: "a=$'--log-file=/x/logs/xmrig.log\\n'\nStarted. It takes about a minute to reach full speed.\nNice: 0\n"  # type: ignore
        okd, _ = d.do_start()
    finally:
        subprocess.check_output = real_co  # type: ignore
    check("a stray line before Started. is still a start", okd and any(e["k"] == "start" for e in d.events) and not any(e["k"] == "out" for e in d.events))
    check("typed hints for the fleet", typed_hint("/stop m2") == "↵ stop m2 (asks first)" and typed_hint("/start") == "↵ start this Mac · or /start m2, /start all")
    print("self-test", "passed" if fails == 0 else f"{fails} failed")
    return 0 if fails == 0 else 1


def main() -> int:
    os.chdir(ROOT)
    args = sys.argv[1:]
    if args and args[0] in ("--self-test", "self-test"):
        return self_test()
    if args and args[0] in ("--dump", "dump"):
        kind = args[1] if len(args) > 1 else "home"
        cols = int(args[2]) if len(args) > 2 else 110
        rows = int(args[3]) if len(args) > 3 else 36
        sys.stdout.write(dump_frame(kind, cols, rows, strip="--plain" in args))
        return 0
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("miner-ui needs a Terminal window. Click the dock icon, or run it in Terminal.")
        return 1

    App().loop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
