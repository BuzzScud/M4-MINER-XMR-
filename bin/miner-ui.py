#!/usr/bin/env python3
"""Ledger-style miner TUI (Codex CLI lineage). Scrollback-first and quiet.

A small job card at the top, then plain bullets with └ results. Shares are kept as a
ledger (#, time, diff, latency, verdict). /usage, /config and /logs print cards.
The only motion is a shimmer across "Mining". Mining starts only on s.
"""
from __future__ import annotations

import glob
import json
import os
import plistlib
import re
import select
import shutil
import signal
import subprocess
import sys
import termios
import time
import tty
import urllib.request
from dataclasses import dataclass, field
from typing import Optional

ROOT = os.environ.get("MINER_ROOT") or os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CTL = os.path.join(ROOT, "bin", "minerctl.sh")
LOG = os.path.join(ROOT, "logs", "xmrig.log")
ERR = os.path.join(ROOT, "logs", "xmrig.err.log")
PLIST = os.path.join(ROOT, "com.minerv3.xmrig.plist")
API = "http://127.0.0.1:18088/2/summary"
API_BACKENDS = "http://127.0.0.1:18088/2/backends"
PEAK_HS = 4204.0
SNAP = os.path.join(ROOT, "logs", "last-session.json")
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


@dataclass(frozen=True)
class Cmd:
    name: str
    desc: str
    action: str


COMMANDS = (
    Cmd("/usage", "Show status, speed, shares, pool, and machine", "usage"),
    Cmd("/config", "Show threads, mode, pool, and worker from the job file", "config"),
    Cmd("/logs", "Tail the last 20 lines of xmrig.log", "logs"),
    Cmd("/err", "Tail the last 20 lines of the error log", "err"),
    Cmd("/open", "Open this folder in Finder", "open"),
    Cmd("/bench", "Offline thread sweep (~10 min). Never starts mining", "bench"),
    Cmd("/flex", "Toggle pool algo switch (off = rx/0 only)", "flex"),
    Cmd("/help", "List commands", "help"),
    Cmd("/quit", "Quit this UI (does not stop a running miner)", "quit"),
)

ALIASES = {
    "/stats": "usage",
    "/status": "usage",
    "/plist": "config",
    "/error": "err",
    "/refresh": "usage",
    "usage": "usage",
    "status": "usage",
    "logs": "logs",
    "err": "err",
    "plist": "config",
    "help": "help",
    "?": "help",
}

TABS = ("Usage", "Config", "Logs")


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
    d: dict[str, str] = {}
    i = 0
    while i < len(args):
        a = args[i]
        if a.startswith("-") and "=" in a:
            k, v = a.split("=", 1)
            d[k] = v
            i += 1
        elif a.startswith("-") and i + 1 < len(args) and not str(args[i + 1]).startswith("-"):
            d[a] = args[i + 1]
            i += 2
        else:
            i += 1
    user = d.get("-u", "")
    worker = user.rsplit(".", 1)[-1] if "." in user else user
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
            "worker": worker or "-",
            "user": user or "-",
            "http": d.get("--http-port", "18088"),
            "cwd": p.get("WorkingDirectory") or ROOT,
            "donate": d.get("--donate-level", "0"),
            "tls": "--tls" in args,
            "cmdline": "caffeinate -i " + " ".join(show),
        }
    )
    return out


def api_get(url: str) -> Optional[dict]:
    try:
        with urllib.request.urlopen(url, timeout=0.6) as r:
            return json.load(r)
    except Exception:
        return None


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
        "worker": job.get("worker") or api.get("worker_id"),
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
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(session_snapshot(live), f, indent=2)
        os.replace(tmp, SNAP)
    except Exception:
        pass


def load_snapshot() -> Optional[dict]:
    try:
        with open(SNAP, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def latest_summary_path() -> Optional[str]:
    try:
        files = glob.glob(os.path.expanduser("~/Desktop/XMR-miner-summary-*.txt"))
        return max(files, key=os.path.getmtime) if files else None
    except Exception:
        return None


def xmrig_up() -> bool:
    try:
        r = subprocess.run(
            ["pgrep", "-x", "xmrig"],
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


def parse_log(lines: list[str]) -> dict:
    """What the ledger wants from xmrig's own log (--log-file): shares with real latency,
    dataset timing and allocation, the pool line, the latest speed line."""
    out: dict = {"shares": [], "dataset_ms": None, "dataset_ts": None, "alloc": None, "pool": None, "speed": None}
    for ln in lines:
        m = _LOG_TS.match(ln)
        if not m:
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
    return out


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
    lw: int = 80  # width of the left pane (== cols when the rail is folded)
    hist: list = field(default_factory=list)  # 10 s hashrate, one sample per poll
    thr_cache: Optional[tuple] = None
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

    # ------------------------------------------------------------------ plumbing
    def size(self) -> None:
        try:
            s = shutil.get_terminal_size((80, 24))
            self.cols = max(60, s.columns)
            self.rows = max(18, s.lines)
        except Exception:
            self.cols, self.rows = 80, 24

    def matches(self) -> list[Cmd]:
        src = self.force_palette if self.force_palette is not None else self.buf
        return filter_cmds(src)

    def selected(self) -> Optional[Cmd]:
        ms = self.matches()
        if not ms:
            return None
        i = max(0, min(self.sel, len(ms) - 1))
        return ms[i]

    def live(self) -> dict:
        if self.fixed_live is not None:
            return self.fixed_live
        now = time.time()
        if self.live_cache and now - self.live_cache[0] < 0.9:
            return self.live_cache[1]
        job = parse_job()
        api = api_summary()
        if api:
            state = "RUNNING"
        elif xmrig_up():
            state = "STARTING"
        else:
            state = "STOPPED"
        hs = None
        highest = PEAK_HS
        acc = rej = 0
        up = 0
        algo = job.get("algo") or "rx/0"
        hugepages = None
        if api:
            tot = ((api.get("hashrate") or {}).get("total") or [None])
            hs = tot[0] if tot else None
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
        if state == "RUNNING" and hs is None:
            state = "STARTING"  # API is up but RandomX is still building the dataset
        live = {
            "state": state,
            "job": job,
            "api": api,
            "hs": hs,
            "highest": highest,
            "acc": acc,
            "rej": rej,
            "up": up,
            "algo": algo,
            "hugepages": hugepages,
            "log": parse_log(read_tail(LOG, 200)) if state != "STOPPED" else {"shares": []},
        }
        if not self.dump and state in ("RUNNING", "STARTING"):
            save_session_snapshot(live)
        self.live_cache = (now, live)
        if state == "RUNNING":
            self.hist.append(hs)
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
        if state == "RUNNING" and prev != "RUNNING":
            if self.start_ts and prev == "STARTING":
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
        if state == "RUNNING":
            acc, rej = live["acc"], live["rej"]
            since = (self.start_ts or 0) - 2
            from_log = [r for r in (live.get("log") or {}).get("shares", []) if r["ts"] >= since]
            if from_log:
                # xmrig's own log has the real per-share latency: it wins over API counting
                self.share_rows = from_log[-60:]
            else:
                if self.last_acc is not None and acc > self.last_acc:
                    for n in range(self.last_acc + 1, acc + 1):
                        self.share_rows.append({"n": n, "ts": now, "diff": res.get("diff_current"), "ms": conn.get("ping"), "ok": True, "src": "api"})
                if self.last_rej is not None and rej > self.last_rej:
                    self.share_rows.append({"n": acc, "ts": now, "diff": res.get("diff_current"), "ms": conn.get("ping"), "ok": False, "src": "api"})
                self.share_rows = self.share_rows[-60:]
            self.last_acc, self.last_rej = acc, rej
            fails = conn.get("failures")
            try:
                fails = int(fails) if fails is not None else None
            except (TypeError, ValueError):
                fails = None
            if fails is not None:
                if self.last_fail is not None and fails > self.last_fail:
                    self.add("warn", title=f"Pool connection failed ({fails} so far this session)", sub="xmrig reconnects on its own; shares in flight may be lost")
                self.last_fail = fails
        else:
            self.last_acc = self.last_rej = None
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
        R.append(lab("hashrate", f"{live['hs'] / peak * 100:.0f}% of peak" if run and live["hs"] else ""))
        R.append(f"{BOLD}{fmt_hs(live['hs'])}{NOBOLD}" if run else f"{SEC}{'warming up' if starting else 'not mining'}{INK}")
        spark = braille_rows(self.hist, W, 4, 3600, max(4250, peak)) if self.hist else [" " * W] * 3 + ["⣀" * W]
        R += [f"{DATA if run else FAINT}{s}{INK}" for s in spark]
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
        R.append(host or pool)
        fails = conn.get("failures")
        R.append(f"{SEC}:{port or '—'} · {'TLS' if (conn.get('tls') or job.get('tls')) else 'plain'} · {fmt_ping(conn.get('ping')) if run else '—'} · {fails if fails is not None else '—'} failure{'' if str(fails) == '1' else 's'}{INK}")
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
        R.append(f"{SEC}XMRig {api.get('version') or (snap or {}).get('version') or '—'} arm64 · api :{job.get('http', '18088')}{INK}")
        # session
        R.append("")
        R.append(lab("session"))
        if run or starting:
            R.append(f"{SEC}started {hms(now - (live['up'] or 0))} · {fmt_uptime(live['up'])}{INK}")
        elif snap and snap.get("up"):
            R.append(f"{SEC}last ran {fmt_uptime(snap.get('up') or 0)} · {snap.get('saved_at', '')[11:16]}{INK}")
        else:
            R.append(f"{SEC}nothing yet{INK}")
        # fit: drop whole sections from the bottom until it fits
        while len(R) > avail:
            idx = max((i for i, ln in enumerate(R) if ln == ""), default=0)
            if idx == 0:
                R = R[:avail]
                break
            R = R[:idx]
        return [clip_row(ln, W) for ln in R]

    def now_row(self, live: dict, label: str = "now:     ") -> str:
        """The live line of the job card: what is happening right now, in one glance."""
        state = live["state"]
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
            f"{SEC}job:     {INK}{spec}{SEC}     /config to view{INK}",
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
                bullet(f"{BOLD}Started xmrig{NOBOLD}" if not e.get("text") else f"{BOLD}xmrig{NOBOLD}{SEC} · {e['text']}{INK}")
                tree(f"{SEC}{e.get('cmd') or live['job'].get('cmdline') or 'caffeinate -i xmrig'}{INK}")
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
                bullet(f"{BOLD}Dataset ready{NOBOLD}{when}")
                tree(f"{SEC}{alloc} · huge pages {fmt_hugepages(live.get('hugepages') or (al and al['hp']))}{INK}")
            elif k == "pool":
                pool = conn.get("pool") or live["job"].get("pool") or "—"
                tls = conn.get("tls")
                tls_s = tls if isinstance(tls, str) and tls else ("TLS" if tls else "plain")
                bullet(f"{BOLD}Connected{NOBOLD} to {pool}")
                tree(f"{SEC}{tls_s} · {fmt_ping(conn.get('ping'))} · worker {api.get('worker_id') or live['job'].get('worker') or '—'}{INK}")
            elif k == "warn":
                bullet(f"{WARN}{e['title']}{INK}", WARN)
                tree(f"{SEC}{e.get('sub','')}{INK}")
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
            elif k == "stop":
                bullet(f"{BOLD}Stopped{NOBOLD} after {fmt_uptime(e.get('up') or 0)}" + (f"{SEC} · {e['text']}{INK}" if e.get("text") else ""))
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
        name = "/err" if (tab == "Logs" and self.log_which == "err") else "/" + tab.lower()
        tight = room < 20
        out = [self.band(f"{SEC} › {INK}{name}")] + ([] if tight else [self.r("")])
        if tab == "Logs":
            path = ERR if self.log_which == "err" else LOG
            rel = os.path.relpath(path, ROOT)
            out.append(self.r(f"{SEC}• {INK}{BOLD}Ran{NOBOLD} tail -n 20 {rel}{SEC}   (e switches to the {'main' if self.log_which == 'err' else 'error'} log){INK}"))
            tail = read_tail(path, 20)
            if not tail:
                out.append(self.r(f"{SEC}  └ (no output){INK}"))
            for i, ln in enumerate(tail):
                col = GOOD if "accepted" in ln else BAD if "rejected" in ln else INK
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
        head = fit_row(f"{CYAN}{BOLD}>_ {INK}XMR Miner{NOBOLD}{SEC} · {tab.lower()}{INK}", f"{FAINT}←/→ usage · config · logs{INK}", inner)
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
        if tab == "Config":
            flex = os.path.isfile(os.path.join(ROOT, "flex.on"))
            rows = [
                kv("Algorithm", job.get("algo") or "—"),
                kv("Mode", job.get("mode") or "—"),
                kv("Threads", str(job.get("threads") or "—")),
                kv("Init", str(job.get("init") or "—")),
                kv("Pool", job.get("pool") or "—"),
                kv("Worker", job.get("worker") or "—"),
                kv("HTTP API", f"127.0.0.1:{job.get('http', '18088')}"),
                kv("Donate", f"{job.get('donate') or '0'}%"),
                kv("Flex", "on · pool picks the algo" if flex else "off · rx/0 only"),
                kv("Job file", os.path.basename(PLIST)),
                kv("Folder", short_path(str(job.get("cwd") or ROOT), self.lw - 22)),
                "",
                f"  {SEC}Edit the plist, then t and s to apply. Mining starts on s only.{INK}",
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
            kv("Pool", f"{conn.get('pool') or job.get('pool') or '—'}{SEC} · {fmt_ping(conn.get('ping')) if run else '—'} · {fail_s}{INK}"),
            kv("Machine", f"{cpu} · {ram} · XMRig {api.get('version') or '—'} arm64"),
            kv("Uptime", fmt_clock(live["up"]) if run or state == "STARTING" else "not running"),
        ]
        return rows

    def palette_rows(self, ms: list[Cmd], n: int) -> list[str]:
        if not ms:
            return [self.r(f"  {SEC}no matching commands{INK}")]
        sel = max(0, min(self.sel, len(ms) - 1))
        start = 0
        if len(ms) > n:
            start = min(max(0, sel - n + 1), len(ms) - n)
        out = []
        for i, c in enumerate(ms[start : start + n]):
            on = start + i == sel
            mark = f"{CYAN}› " if on else "  "
            name = f"{CYAN}{BOLD}{c.name:<12}{NOBOLD}" if on else f"{SEC}{c.name:<12}"
            desc = f"{INK}{c.desc}" if on else f"{FAINT}{c.desc}"
            out.append(self.r(f"{mark}{name}{desc}{INK}"))
        return out

    def status_row(self, live: dict) -> Optional[str]:
        state = live["state"]
        if state == "STOPPED":
            return None
        f = self.frame_no
        word = "Building dataset" if state == "STARTING" else "Mining"
        dot = f"{SEC}{'•' if (f >> 1) % 2 else '◦'}{INK}"
        hi = (f % (len(word) + 8)) - 4
        letters = []
        for i, ch in enumerate(word):
            d = abs(i - hi)
            letters.append((BRIGHT + BOLD if d == 0 else MID + BOLD if d == 1 else SEC + NOBOLD) + ch)
        if state == "STARTING":
            el = int(time.time() - self.start_ts) if self.start_ts else int(live["up"] or 0)
            tail = f"({fmt_uptime(el)} • t to cancel)"
        else:
            tail = f"({fmt_clock(live['up'])} • t to stop)"
        return self.r(f"{dot} {''.join(letters)}{NOBOLD}{SEC} {tail}{INK}")

    def band_rows(self, live: dict, y0: int) -> list[str]:
        if self.mode == "confirm":
            text, ph = self.confirm_buf, "yes"
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
        return [self.band(""), self.band(line), self.band("")]

    def footer_row(self, live: dict) -> str:
        state = live["state"]
        if self.mode == "confirm":
            keys = [("↵", "run"), ("esc", "cancel")]
            right = "offline sweep · mines nothing"
        elif self.mode == "overlay":
            keys = [("←→", "cards"), ("esc", "back"), ("s", "start"), ("t", "stop")]
            right = ""
        else:
            keys = [("↵", "run"), ("s", "start"), ("t", "stop"), ("/", "commands"), ("⌃C", "quit")]
            right = ""
        if not right:
            if state == "RUNNING":
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
        rows, cols = self.rows, self.cols
        rail = self.rail_on()
        self.lw = cols - self.RAIL_W - 4 if rail else cols
        compact = rows <= 24
        head = self.header_rows(live, compact)
        top = len(head)
        band_y = rows - 4
        pal_n = 0
        ms = self.matches()
        pal = self.mode == "home" and (self.force_palette is not None or self.buf.startswith("/"))
        if pal:
            pal_n = max(1, min(len(ms) or 1, 9, rows - top - 6))
        status = self.status_row(live) if (self.mode == "home" and not pal) else None
        if self.mode == "confirm":
            status = self.r(f"{SEC}• {INK}Offline sweep{SEC} · ~10 minutes · mines nothing · refuses if the miner is running · type yes{INK}")
        body_end = band_y - pal_n - (1 if status else 0) - (1 if pal else 0)  # exclusive; one spacer above a palette
        room = max(1, body_end - top)
        if self.mode == "overlay":
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
            out += self.palette_rows(ms, pal_n)
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

    def draw(self, force: bool = True) -> None:
        self.size()
        self.frame_no += 1
        try:
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
        body = HOME + "\r\n".join(lines)
        if self.cursor:
            y, x = self.cursor
            body += f"\033[{y + 1};{x + 1}H"
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

    def do_start(self) -> None:
        self.add("user", text="s")
        try:
            out = subprocess.check_output([CTL, "start"], text=True, timeout=8)
        except subprocess.CalledProcessError as e:
            out = e.output or "start failed"
        except Exception as e:
            out = str(e)
        lines = [ln for ln in out.splitlines() if ln.strip()]
        if lines and lines[0].startswith("Started"):
            self.start_ts = time.time()
            self.ds_ready_s = None
            self.share_rows = []
            self.last_acc = self.last_rej = self.last_fail = None
            self.drop("dsprog", "shares", "dataset", "pool")
            self.add("start", cmd=(self.live_cache[1]["job"].get("cmdline") if self.live_cache else ""))
            self.add("dsprog")
            self.prev_state = "STARTING"
        else:
            self.say(*(lines or ["start: no output"]))
        self.live_cache = None
        self.mode = "home"

    def do_stop(self) -> None:
        self.add("user", text="t")
        live = self.live()
        try:
            out = subprocess.check_output([CTL, "stop"], text=True, timeout=15)
        except Exception as e:
            out = str(e)
        lines = [ln for ln in out.splitlines() if ln.strip()]
        if any(ln.startswith("Stopped") for ln in lines):
            api = live.get("api") or {}
            tot = (api.get("hashrate") or {}).get("total") or []
            avg = next((v for v in (tot[2] if len(tot) > 2 else None, tot[1] if len(tot) > 1 else None, live.get("hs")) if v), None)
            self.drop("dsprog", "shares")
            self.add("stop", up=live.get("up") or 0, acc=live.get("acc") or 0, rej=live.get("rej") or 0, avg=avg)
            for ln in lines:
                if ln.startswith("Desktop summary:"):
                    self.add("summary", path=ln.split(":", 1)[1].strip())
            self.prev_state = "STOPPED"
            self.last_acc = self.last_rej = None
        else:
            self.say(*(lines or ["stop: no output"]))
        self.live_cache = None
        self.mode = "home"

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
        r, _, _ = select.select([self.fd], [], [], 0.4)
        if not r:
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
        ms = self.matches()
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
            if action:
                return self.run_action(action)
            self.add("user", text=text)
            self.say("Unknown command. Type / for the list.")
            return True
        if key and len(key) == 1:
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
            self.log_which = "log" if self.log_which == "err" else "err"
            return True
        if key in "123":
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

    def loop(self) -> None:
        self.take_tty()
        sys.stdout.write(f"\033]11;{GROUND}\007")  # ask the terminal for the Material ground (ignored if unsupported)
        try:
            self.draw()
            while True:
                key = self.read_key()
                if key is None:
                    # live refresh in place; faster while the shimmer is on
                    state = self.live_cache[1]["state"] if self.live_cache else "STOPPED"
                    period = 0.4 if state != "STOPPED" else 1.0
                    if time.time() - self.last_draw >= period:
                        self.draw(force=False)
                    continue
                if self.mode == "overlay":
                    ok = self.on_key_overlay(key)
                elif self.mode == "confirm":
                    ok = self.on_key_confirm(key)
                else:
                    ok = self.on_key_home(key)
                if not ok:
                    break
                self.draw()
        finally:
            self.restore_tty()
            sys.stdout.write(SHOW + RESET + "\033]111\007" + "\n")  # OSC 111: restore the profile's background
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
        "worker_id": "minerv3-m4-16gb",
        "uptime": 58080,
        "hugepages": [0, 1178],
    }
    if state == "STOPPED":
        return {"state": "STOPPED", "job": job, "api": None, "hs": None, "highest": PEAK_HS, "acc": 0, "rej": 0, "up": 0, "algo": "rx/0", "hugepages": None}
    return {"state": state, "job": job, "api": api, "hs": 4178.4 if state == "RUNNING" else None, "highest": PEAK_HS, "acc": 1945, "rej": 0, "up": 58080, "algo": "rx/0", "hugepages": [0, 1178]}


def demo_app(kind: str, cols: int = 110, rows: int = 36) -> App:
    """A frame from canned data, for --dump and the self-test. Touches no miner."""
    app = App(dump=True, cols=cols, rows=rows)
    app.size = lambda: None  # type: ignore
    app.cols, app.rows = cols, rows
    stopped = kind.endswith("-stopped")
    starting = kind.endswith("-starting")
    kind = kind.replace("-stopped", "").replace("-starting", "")
    live = _demo_live("STOPPED" if stopped else "STARTING" if starting else "RUNNING")
    app.fixed_live = live
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
    if kind in ("palette", "slash", "/"):
        app.force_palette = "/" if kind != "palette" else "/usage"
    elif kind in ("usage", "config", "logs", "err"):
        app.mode = "overlay"
        app.tab = TABS.index({"usage": "Usage", "config": "Config"}.get(kind, "Logs"))
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
    check("tabs are Usage Config Logs", TABS == ("Usage", "Config", "Logs"))
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
    snap = session_snapshot(_demo_live())
    check("session snapshot", snap["acc"] == 1945 and snap["hs10"] == 4178.4 and "moneroocean" in snap["pool"])
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

    for kind in ("home", "home-stopped", "palette", "slash", "usage", "config", "logs", "confirm"):
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
    check("rail present at 110 cols", any("│  hashrate" in ln for ln in rail) and any("│  shares" in ln for ln in rail) and any("│  threads" in ln for ln in rail) and any("│  pool" in ln for ln in rail) and any("│  dataset" in ln for ln in rail) and any("│  session" in ln for ln in rail))
    check("rail sparkline + windows", any(ch in "".join(rail) for ch in "⣿⣶⣤⣀") and any("10s 4,178 · 60s 4,062 · 15m 4,166" in ln for ln in rail))
    check("rail hides the now row", not any("now:" in ln for ln in rail) and any("99% of peak" in ln for ln in rail))
    check("rail folds below 100 cols", not any("│  hashrate" in ln for ln in dump_frame("home", 99, 36, strip=True).split("\n")) and "now:" in dump_frame("home", 99, 36, strip=True))
    check("rail band spans left pane only", any(ln.startswith(" › Type / for commands") and "│" in ln for ln in rail))
    check("braille shape", braille_rows([0, 100], 1, 1, 0, 100) == [chr(0x2800 | 0x40 | 0x80 | 0x20 | 0x10 | 0x08)] and braille_rows([100, 100], 1, 2, 0, 100) == ["⣿", "⣿"] and braille_rows([50, 50], 1, 2, 0, 100) == [" ", "⣿"])
    check("starting now row (folded layout)", "now:     building the dataset · 3s" in dump_frame("home-starting", 90, 36, strip=True))
    check("starting rail", "│  warming up" in dump_frame("home-starting", 110, 36, strip=True) and "building" in dump_frame("home-starting", 110, 36, strip=True))
    check("user turn band", " › s" in home)
    check("bullets and trees", "• Started xmrig" in home and "└ caffeinate -i xmrig" in home)
    check("shares ledger", "• Shares  1,945 accepted" in home and "#1,945" in home and "diff 125,113" in home and "143 ms" in home and "✓" in home)
    check("status shimmer line", "Mining (16h 08m" in home and "t to stop)" in home)
    check("band placeholder", "› Type / for commands, t to stop" in home)
    check("footer keys", "↵ run" in home and "⌃C quit" in home and "4,178 H/s · 99% of peak" in home)
    stopped = dump_frame("home-stopped", strip=True)
    check("stopped: summary + placeholder", "• Stopped after 16h 8m" in stopped and "Wrote ~/Desktop/XMR-miner-summary" in stopped and "s to mine" in stopped and "not mining" in stopped and "Mining (" not in stopped)
    pal = dump_frame("slash", strip=True)
    check("palette rows", "› /usage" in pal and "/quit" in pal and "Type / for" not in pal)
    usage = dump_frame("usage", strip=True)
    check("usage card", "› /usage" in usage and "XMR Miner · usage" in usage and "Hashrate:" in usage and "[" in usage and "Windows:" in usage and "Cadence:" in usage and "Uptime:      16h 08m 00s" in usage)
    config = dump_frame("config", strip=True)
    check("config card", "Algorithm:" in config and "Job file:" in config and "Edit the plist" in config)
    logs = dump_frame("logs", strip=True)
    check("logs tail", "• Ran tail -n 20 logs/xmrig.log" in logs)
    confirm = dump_frame("confirm", strip=True)
    check("confirm band", "› yes" in confirm and "Offline sweep" in confirm and "esc cancel" in confirm)
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

    def _winch(_sig, _frm):
        pass

    signal.signal(signal.SIGWINCH, _winch)
    App().loop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
