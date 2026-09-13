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
ARCH = os.uname().machine  # arm64 or x86_64; bin/xmrig is universal
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
    Cmd("/config", "Threads, mode, pool, worker", "config", "miner"),
    Cmd("/logs", "Last 20 lines of xmrig.log", "logs", "miner"),
    Cmd("/err", "Last 20 lines of the error log", "err", "miner"),
    Cmd("/open", "Open this folder in Finder", "open", "actions"),
    Cmd("/bench", "Offline thread sweep, ~10 min", "bench", "actions"),
    Cmd("/flex", "Pool picks the algo (or rx/0)", "flex", "actions"),
    Cmd("/help", "List commands", "help", "ui"),
    Cmd("/quit", "Quit; the miner keeps running", "quit", "ui"),
)
GROUPS = ("miner", "actions", "ui")
NOT_RECENT = {"help", "quit"}
RECENT_N = 3

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


def api_get(url: str) -> Optional[dict]:
    try:
        with urllib.request.urlopen(url, timeout=1.2) as r:
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
        if a == "quit":
            return "xmrig keeps running" if state != "STOPPED" else ""
        return ""

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
            "log": parse_log(read_tail(LOG, 400) + read_tail(ERR, 200)),
        }
        # STARTING has zeros; writing that over last-session.json made a later
        # leftover look like a 0-share session. Only persist a live RUNNING sample.
        if not self.dump and state == "RUNNING":
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
                    why = last_error_msg(live.get("log") or {})
                    sub = why or "xmrig reconnects on its own; shares in flight may be lost — /logs for the reason"
                    self.add("warn", title=f"Pool connection failed ({fails} so far this session)", sub=sub)
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
        R.append(f"{SEC}:{port or '—'} · {tls_label(conn, job)} · {fmt_ping(conn.get('ping')) if run else '—'} · {fails if fails is not None else '—'} failure{'' if str(fails) == '1' else 's'}{INK}")
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
                bullet(f"{BOLD}Dataset ready{NOBOLD}{when}")
                tree(f"{SEC}{alloc} · huge pages {fmt_hugepages(live.get('hugepages') or (al and al['hp']))}{INK}")
            elif k == "pool":
                pool = conn.get("pool") or live["job"].get("pool") or "—"
                tls_s = tls_label(conn, live["job"])
                bullet(f"{BOLD}Connected{NOBOLD} to {pool}")
                tree(f"{SEC}{tls_s} · {fmt_ping(conn.get('ping'))} · worker {live['job'].get('worker') or '—'}{INK}")
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
                kv("Priority", "--cpu-priority=4 → nice -10 (needs root)"),
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
            return [self.r(f"  {SEC}no matching commands{INK}")]
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
            pal_n = max(1, min(want or 1, 12, avail))
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

    def do_start(self) -> None:
        self.add("user", text="s")
        live = self.live()
        try:
            out = subprocess.check_output([CTL, "start"], text=True, timeout=12)
        except subprocess.CalledProcessError as e:
            out = e.output or "start failed"
        except Exception as e:
            out = str(e)
        lines = [ln for ln in out.splitlines() if ln.strip()]
        nice = next((ln.strip() for ln in lines if ln.startswith("Nice:")), "")
        cmd = self.live_cache[1]["job"].get("cmdline") if self.live_cache else ""
        if live["state"] in ("RUNNING", "STARTING") and lines and lines[0].startswith("Already running"):
            msg = f"Already mining ({fmt_uptime(live.get('up') or 0)}). t stops it."
            self.say(msg, nice) if nice else self.say(msg)
            self.nice_cache = None
            self.mode = "home"
            return
        if lines and lines[0].startswith("Started"):
            self.start_ts = time.time()
            self.ds_ready_s = None
            self.share_rows = []
            self.last_acc = self.last_rej = self.last_fail = None
            self.nice_cache = None
            self.drop("dsprog", "shares", "dataset", "pool")
            self.add("start", cmd=cmd, nice=nice)
            self.add("dsprog")
            self.prev_state = "STARTING"
        elif lines and lines[0].startswith("Already running"):
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
        try:
            self._clear_next = True
            self.draw()
            while True:
                key = self.read_key()
                resized = self._resized or self.size()
                if key is None:
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
                elif self.mode == "confirm":
                    ok = self.on_key_confirm(key)
                else:
                    ok = self.on_key_home(key)
                if not ok:
                    break
                self.draw()
        finally:
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
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".plist") as tf:
        plistlib.dump({"ProgramArguments": ["xmrig", "-u", "4" + "8" * 94]}, tf)
        tf.flush()
        check("bare -u wallet is never the worker", parse_job(tf.name)["worker"] == "-")
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
    check("palette groups + digits + status", " miner" in pal and " actions" in pal and " ui" in pal and "▎1 /usage" in pal and " 9 /quit" in pal
          and "4,178 H/s · 1,945 ✓" in pal and "off · rx/0 only" in pal and "running · will refuse" in pal and "Type / for" not in pal)
    check("palette hints row", "1 of 9" in pal and "1–9 run" in pal)
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
    check("config card", "Algorithm:" in config and "Job file:" in config and "Edit the plist" in config)
    check("config priority", "Priority:" in config and "nice -10" in config)
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

    App().loop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
