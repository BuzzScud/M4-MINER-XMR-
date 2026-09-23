#!/usr/bin/env python3
"""Fleet control: the main Mac starts and stops the others; every Mac keeps a timed event log.

The helper (`control.py serve`, port 18089) runs on every Mac except the main one:
  - while XMR Miner's window is open there, start / stop / restart go through the window,
    exactly like pressing s or t in it, so its ledger says what happened and who asked;
  - after the window closes, for as long as xmrig runs, it can still stop the miner
    (starting needs the window); then it exits.
Only the main Mac can send commands. It signs each one with a key that never leaves it
(~/Library/Application Support/XMR Miner/control.key, `ssh-keygen -Y sign`); the others check
the signature against control.pub from the repo and refuse a command that is over a minute old,
one they have seen before, or one meant for another Mac. Reads want the fleet token, like
xmrig's own API.

Events: exact times from this Mac's xmrig log (pool errors, backup pool, rejected shares,
pause / resume, starts) plus logs/events.jsonl, which minerctl writes at every start and stop
with who asked. The main Mac keeps every Mac's events in logs/fleet-events.jsonl.
"""
from __future__ import annotations

import bisect
import hashlib
import hmac
import json
import os
import re
import secrets
import signal
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Callable, Optional

import fleet  # bin/fleet.py: token, worker name, HTTP helpers, the fleet rows

PORT = 18089
NS = "xmr-miner-control"   # ssh-keygen signature namespace: a signature for anything else never verifies
SIGNER = "main"
CMDS = ("start", "stop", "restart")
MAX_AGE = 60.0             # s: an older (or that far in the future) command is refused
WINDOW_PICKUP = 20.0       # s for an open window to take a command before the helper gives up on it
WINDOW_DONE = 60.0         # s for the window to finish it (start ≤ 12 s, stop ≤ 15 s)
LOG_TAIL = 3_000_000       # bytes of xmrig.log read for events: about 5 days of mining
ERR_GAP = 90.0             # s: pool errors closer than this are one outage (xmrig retries every 5 s)


def set_root(root: str) -> None:
    """Every path below, for this folder (the self-test points it at a scratch copy)."""
    global ROOT, LOGS, PUB, CTL, XMRIG_LOG, EVENTS, FLEET_EVENTS, PIDFILE, UI_PID, INBOX, SEEN, ALLOWED, HELPER_LOG
    ROOT = root
    LOGS = os.path.join(root, "logs")
    PUB = os.path.join(root, "control.pub")
    CTL = os.environ.get("MINER_CTL") or os.path.join(root, "bin", "minerctl.sh")
    XMRIG_LOG = os.path.join(LOGS, "xmrig.log")
    EVENTS = os.path.join(LOGS, "events.jsonl")          # this Mac: starts and stops, with who asked
    FLEET_EVENTS = os.path.join(LOGS, "fleet-events.jsonl")  # main Mac: every Mac's events
    PIDFILE = os.path.join(LOGS, "control.pid")
    UI_PID = os.path.join(LOGS, "ui.pid")
    INBOX = os.path.join(LOGS, "control-inbox")
    SEEN = os.path.join(LOGS, "control-seen.json")
    ALLOWED = os.path.join(LOGS, "control.allowed_signers")
    HELPER_LOG = os.path.join(LOGS, "control.log")


set_root(fleet.ROOT)
KEY = os.environ.get("MINER_CONTROL_KEY") or os.path.expanduser("~/Library/Application Support/XMR Miner/control.key")


# ---------------------------------------------------------------------- small things
def when(ts: Optional[float], now: Optional[float] = None) -> str:
    """18:52:04 today, 09-22 08:35:08 on an earlier day."""
    if not ts:
        return "—"
    lt = time.localtime(ts)
    if time.strftime("%Y%m%d", lt) == time.strftime("%Y%m%d", time.localtime(now or time.time())):
        return time.strftime("%H:%M:%S", lt)
    return time.strftime("%m-%d %H:%M:%S", lt)


def short(worker: str) -> str:
    return worker[8:] if (worker or "").startswith("minerv3-") else (worker or "")


def fmt_up(s) -> str:
    try:
        s = int(s or 0)
    except (TypeError, ValueError):
        return "—"
    h, m = divmod(s // 60, 60)
    return f"{h}h {m}m" if h else f"{m}m {s % 60}s" if m else f"{s}s"


def pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def pid_command(pid: int) -> str:
    try:
        return subprocess.run(["ps", "-p", str(pid), "-o", "command="], capture_output=True, text=True, timeout=2).stdout
    except Exception:
        return ""


def read_json(path: str) -> Optional[dict]:
    try:
        with open(path, encoding="utf-8") as f:
            d = json.load(f)
        return d if isinstance(d, dict) else None
    except Exception:
        return None


def write_json(path: str, obj) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = f"{path}.{os.getpid()}.tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f)
    os.replace(tmp, path)


def append_jsonl(path: str, rows: list) -> None:
    if not rows:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        for r in rows:
            f.write(json.dumps(r, separators=(",", ":")) + "\n")


def read_tail_lines(path: str, nbytes: int) -> list[str]:
    try:
        with open(path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            start = max(0, size - nbytes)
            f.seek(start)
            data = f.read().decode("utf-8", "replace")
    except OSError:
        return []
    lines = data.splitlines()
    return lines[1:] if start > 0 and lines else lines  # the first one was cut in half


def read_jsonl(path: str, nbytes: int = 4_000_000) -> list[dict]:
    out = []
    for ln in read_tail_lines(path, nbytes):
        try:
            d = json.loads(ln)
        except ValueError:
            continue
        if isinstance(d, dict) and isinstance(d.get("ts"), (int, float)) and d.get("k"):
            out.append(d)
    return out


def is_main() -> bool:
    """git config miner.role == main: the Mac that releases, and the only one that sends commands."""
    try:
        out = subprocess.run(["git", "-C", ROOT, "config", "--get", "miner.role"], capture_output=True, text=True, timeout=2)
        return out.stdout.strip() == "main"
    except Exception:
        return False


def xmrig_running() -> bool:
    try:
        return subprocess.run(["pgrep", "-x", "xmrig"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2).returncode == 0
    except Exception:
        return False


def ui_open() -> bool:
    """True while an XMR Miner window runs here (bin/miner-ui.py writes logs/ui.pid)."""
    d = read_json(UI_PID) or {}
    pid = d.get("pid")
    return isinstance(pid, int) and pid > 0 and pid_alive(pid) and "miner-ui" in pid_command(pid)


def code_version() -> str:
    """Changes when a release replaces this file or fleet.py: a running helper restarts itself."""
    h = hashlib.sha1()
    for p in (os.path.abspath(__file__), os.path.abspath(fleet.__file__)):
        try:
            with open(p, "rb") as f:
                h.update(f.read())
        except OSError:
            pass
    return h.hexdigest()[:10]


# ---------------------------------------------------------------------- the key
def pub_line(path: Optional[str] = None) -> str:
    """The key line of control.pub (ssh-ed25519 AAAA… comment), or ''."""
    try:
        with open(path or PUB, encoding="utf-8") as f:
            for ln in f:
                ln = ln.strip()
                if ln.startswith("ssh-"):
                    return ln
    except OSError:
        pass
    return ""


def fingerprint(line: str) -> str:
    """Short id of a public key line: the Macs compare these before a command is sent."""
    parts = (line or "").split()
    return hashlib.sha256(parts[1].encode()).hexdigest()[:12] if len(parts) >= 2 else ""


def keygen(force: bool = False) -> str:
    """Main Mac: the signing key (outside the repo, never pushed) and control.pub (in the repo)."""
    if os.path.isfile(KEY) and not force:
        msg = "exists"
    else:
        os.makedirs(os.path.dirname(KEY), mode=0o700, exist_ok=True)
        for p in (KEY, KEY + ".pub"):
            if os.path.exists(p):
                os.remove(p)
        subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", "xmr-miner-main", "-f", KEY],
                       check=True, capture_output=True, timeout=20)
        msg = "made"
    line = pub_line(KEY + ".pub")
    if not line:
        raise RuntimeError(f"no public key next to {KEY}")
    if pub_line() != line:
        with open(PUB, "w", encoding="utf-8") as f:
            f.write("# XMR Miner: the main Mac's public key for fleet commands (bin/control.py). Safe to publish:\n"
                    "# the private half stays on the main Mac, outside this folder.\n" + line + "\n")
        msg += ", control.pub written"
    return msg


def sign(data: bytes) -> str:
    if not os.path.isfile(KEY):
        raise RuntimeError("this Mac has no control key: ./bin/minerctl.sh remote setup")
    r = subprocess.run(["ssh-keygen", "-Y", "sign", "-q", "-f", KEY, "-n", NS], input=data, capture_output=True, timeout=10)
    if r.returncode != 0 or b"BEGIN SSH SIGNATURE" not in r.stdout:
        raise RuntimeError((r.stderr.decode("utf-8", "replace").strip() or "ssh-keygen -Y sign failed")[:200])
    return r.stdout.decode()


def verify(data: bytes, sig: str) -> bool:
    """True only for a signature by control.pub's key, in our namespace, over exactly these bytes."""
    line = pub_line()
    if not line or "BEGIN SSH SIGNATURE" not in sig:
        return False
    os.makedirs(LOGS, exist_ok=True)
    want = f'{SIGNER} namespaces="{NS}" {" ".join(line.split()[:2])}\n'
    try:
        with open(ALLOWED, encoding="utf-8") as f:
            same = f.read() == want
    except OSError:
        same = False
    if not same:
        with open(ALLOWED, "w", encoding="utf-8") as f:
            f.write(want)
    fd, sp = tempfile.mkstemp(dir=LOGS, suffix=".sig")
    try:
        with os.fdopen(fd, "w") as f:
            f.write(sig)
        r = subprocess.run(["ssh-keygen", "-Y", "verify", "-f", ALLOWED, "-I", SIGNER, "-n", NS, "-s", sp],
                           input=data, capture_output=True, timeout=10)
        return r.returncode == 0
    except Exception:
        return False
    finally:
        try:
            os.unlink(sp)
        except OSError:
            pass


def make_cmd(cmd: str, to: str, frm: str) -> str:
    return json.dumps({"v": 1, "cmd": cmd, "to": to, "from": frm, "ts": round(time.time(), 3),
                       "nonce": secrets.token_hex(12)}, separators=(",", ":"), sort_keys=True)


def check_cmd(msg: dict, me: str, seen: dict, now: float) -> str:
    """'' when a signed command may run here; otherwise why not."""
    if not isinstance(msg, dict) or msg.get("v") != 1:
        return "unknown command format"
    if msg.get("cmd") not in CMDS:
        return f"unknown command {msg.get('cmd')!r}"
    if not me or msg.get("to") != me:
        return f"meant for {msg.get('to')}, this Mac is {me or '?'}"
    ts = msg.get("ts")
    if not isinstance(ts, (int, float)) or abs(now - ts) > MAX_AGE:
        return "too old (or the two clocks differ by over a minute)"
    nonce = msg.get("nonce")
    if not isinstance(nonce, str) or not re.fullmatch(r"[0-9a-f]{24}", nonce):
        return "bad nonce"
    if nonce in seen:
        return "already done (a repeat)"
    return ""


# ---------------------------------------------------------------------- events
_ANSI = re.compile(r"\033\[[0-9;?]*[A-Za-z]")
_LOG_TS = re.compile(r"^\[(\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)\.(\d{3})\]\s+(\S+)\s+(.*)$")
_SHARE = re.compile(r'^(accepted|rejected) \((\d+)/(\d+)\) diff (\d+)(?: "([^"]*)")? \((\d+) ms\)')
_POOL_TAG = re.compile(r"^\[([^\]]+)\]\s+(?:(?:\d{1,3}(?:\.\d{1,3}){3}|[0-9a-fA-F]*:[0-9a-fA-F:]+)\s+)?")
_NET_BAD = re.compile(r"error|failed|no active pools|timed out|timeout|disconnect|refused", re.I)


def event_id(e: dict) -> str:
    return f"{e.get('worker', '')}|{e['k']}|{int(round(float(e['ts']) * 1000))}"


def log_events(lines: list[str], backup: str = "") -> tuple[list[dict], list[list[float]]]:
    """(events, sessions) from xmrig's log. A session is [first line, last line] of one xmrig run
    (each run begins with its ABOUT banner). Pool errors closer than ERR_GAP are one outage."""
    ev: list[dict] = []
    sessions: list[list[float]] = []
    new_run = False
    outage: Optional[dict] = None
    pool = ""
    last_i = -1  # the last timestamped line: the end of the current session, parsed only when needed

    def close_session() -> None:
        if sessions and last_i >= 0:
            m = _LOG_TS.match(_ANSI.sub("", lines[last_i]))
            if m:
                y, mo, d, h, mi, s, ms3 = (int(x) for x in m.groups()[:7])
                sessions[-1][1] = time.mktime((y, mo, d, h, mi, s, 0, 0, -1)) + ms3 / 1000.0

    for i, raw in enumerate(lines):
        raw = raw or ""
        if "ABOUT" in raw and raw.lstrip().startswith("* ABOUT"):
            close_session()
            new_run = True
            continue
        if not raw.startswith("[") and "\033" not in raw:
            continue
        # most lines are new jobs, speed and accepted shares: skip them before any regex
        if sessions and not new_run and ("new job" in raw or " speed " in raw or " accepted (" in raw
                                         or not (" net " in raw or " miner " in raw or "rejected" in raw)):
            last_i = i
            continue
        ln = _ANSI.sub("", raw) if "\033" in raw else raw
        m = _LOG_TS.match(ln)
        if not m:
            continue
        y, mo, d, h, mi, s, ms3 = (int(x) for x in m.groups()[:7])
        try:
            ts = time.mktime((y, mo, d, h, mi, s, 0, 0, -1)) + ms3 / 1000.0
        except (OverflowError, ValueError):
            continue
        tag, msg = m.group(8), m.group(9).strip()
        if new_run or not sessions:
            sessions.append([ts, ts])
            if new_run:
                ev.append({"k": "start", "ts": ts, "src": "log"})
            new_run, outage, pool = False, None, ""
        sessions[-1][1] = ts
        last_i = i
        sm = _SHARE.match(msg)
        if sm:
            if sm.group(1) == "rejected":
                ev.append({"k": "reject", "ts": ts, "n": int(sm.group(2)) + int(sm.group(3)), "diff": int(sm.group(4)),
                           "ms": int(sm.group(6)), "why": sm.group(5) or ""})
            continue
        if tag == "miner":
            if msg.startswith("paused"):
                ev.append({"k": "pause", "ts": ts})
            elif msg.startswith("resumed"):
                ev.append({"k": "resume", "ts": ts})
            continue
        if tag != "net" or msg.startswith("new job"):
            continue
        if msg.startswith("use pool "):
            host = msg.split()[2] if len(msg.split()) > 2 else ""
            if backup and host == backup and pool != backup:
                ev.append({"k": "backup", "ts": ts, "pool": host})
            elif backup and pool == backup and host != backup:
                ev.append({"k": "pool", "ts": ts, "pool": host, "main": True})
            elif outage is not None:
                ev.append({"k": "pool", "ts": ts, "pool": host, "down": round(ts - outage["ts"])})
            pool, outage = host, None
            continue
        if _NET_BAD.search(msg) and "fingerprint" not in msg:
            tm = _POOL_TAG.match(msg)
            where, text = (tm.group(1), msg[tm.end():]) if tm else ("", msg)
            idle = "no active pools" in msg
            if outage is not None and ts - outage["last"] <= ERR_GAP:
                outage["n"] += 1
                outage["last"] = ts
                outage["idle"] = outage["idle"] or idle
                if where and not outage["pool"]:
                    outage["pool"] = where
                continue
            outage = {"k": "error", "ts": ts, "last": ts, "n": 1, "msg": text[:160], "pool": where, "idle": idle}
            ev.append(outage)
    close_session()
    return ev, sessions


def backup_pool() -> str:
    """The job file's second -o (bin/machine.sh BACKUP), or ''."""
    try:
        import plistlib
        args = [str(a) for a in plistlib.load(open(fleet.PLIST, "rb")).get("ProgramArguments") or []]
    except Exception:
        return ""
    pools = [args[i + 1] for i, a in enumerate(args[:-1]) if a == "-o"]
    return pools[1] if len(pools) > 1 else ""


def merge(log_ev: list[dict], sessions: list[list[float]], own: list[dict], running: bool, now: float) -> list[dict]:
    """The log's events + minerctl's start/stop notes. A log start next to a note is the same start
    (the note knows who asked). A run that ends with no stop note (after notes began) gets an exit.
    Both wait 30 s: minerctl writes its note just after xmrig starts or dies, and the main Mac keeps
    whatever it saw, so a guess made in that gap would stay in its log for good."""
    starts = [e["ts"] for e in own if e["k"] == "start"]
    stops = [e["ts"] for e in own if e["k"] == "stop"]
    out = [e for e in log_ev if e["k"] != "start"
           or (now - e["ts"] >= 30 and not any(-30 <= e["ts"] - t <= 10 for t in starts))]
    out += [dict(e) for e in own]
    if own:
        first = min(e["ts"] for e in own)
        for i, (st, last) in enumerate(sessions):
            if st < first - 5:
                continue
            nxt = sessions[i + 1][0] if i + 1 < len(sessions) else None
            if nxt is None and (running or now - last < 30):
                continue
            hi = nxt if nxt is not None else now
            if not any(last - 5 <= t <= hi + 1 for t in stops):
                out.append({"k": "exit", "ts": last})
    return sorted(out, key=lambda e: e["ts"])


_LOCAL_CACHE: dict = {}


def _stamp(path: str) -> tuple:
    try:
        s = os.stat(path)
        return (s.st_size, s.st_mtime)
    except OSError:
        return (0, 0)


def local_events(worker: Optional[str] = None, now: Optional[float] = None) -> list[dict]:
    """This Mac's events, oldest first. Cached until xmrig.log or events.jsonl changes."""
    worker = worker or fleet.local_worker() or "this Mac"
    running = xmrig_running()
    key = (worker, XMRIG_LOG, _stamp(XMRIG_LOG), _stamp(EVENTS), running)
    if _LOCAL_CACHE.get("key") == key:
        return [dict(e) for e in _LOCAL_CACHE["ev"]]
    log_ev, sessions = log_events(read_tail_lines(XMRIG_LOG, LOG_TAIL), backup_pool())
    out = merge(log_ev, sessions, read_jsonl(EVENTS), running, now or time.time())
    for e in out:
        e["worker"] = worker
        e["id"] = event_id(e)
    _LOCAL_CACHE.update(key=key, ev=out)
    return [dict(e) for e in out]


def note(kind: str, why: str = "", by: str = "", api: Optional[dict] = None) -> dict:
    """minerctl, at every start and stop: logs/events.jsonl gets when, why and who asked."""
    e: dict = {"ts": round(time.time(), 3), "k": kind, "worker": fleet.local_worker()}
    if why:
        e["why"] = why
    if by:
        e["by"] = by
    if api:
        conn = api.get("connection") or {}
        res = api.get("results") or {}
        e["up"] = int(api.get("uptime") or 0)
        e["acc"] = int(conn.get("accepted") or res.get("shares_good") or 0)
        e["rej"] = int(conn.get("rejected") or 0)
    append_jsonl(EVENTS, [e])
    return e


MARKS = {"start": "▶", "stop": "■", "exit": "■", "error": "⚠", "reject": "✗", "backup": "⇄", "pool": "⇄",
         "pause": "‖", "resume": "▶", "denied": "!"}


def describe(e: dict, now: Optional[float] = None) -> tuple[str, str, str]:
    """(tone, title, detail) for one event; tone is good | bad | warn | info | hide."""
    k, why, by = e.get("k"), e.get("why") or "", short(e.get("by") or "")
    approx = "≈ " if e.get("approx") else ""
    if k == "start":
        title = {"remote": f"Started from {by or 'the main Mac'}", "restart": "Restarted" + (f" from {by}" if by else ""),
                 "settings": "Restarted with new settings", "update": "Started after an update"}.get(why, "Started")
        return "good", title, ""
    if k == "stop":
        if why in ("restart", "settings"):
            return "hide", "Stopped for a restart", ""
        title = {"remote": f"Stopped from {by or 'the main Mac'}", "update": "Stopped for an update"}.get(why, "Stopped")
        det = ""
        if e.get("up"):
            det = f"after {fmt_up(e['up'])} · {int(e.get('acc') or 0):,} ✓ · {int(e.get('rej') or 0)} ✗"
        return "info", title, det
    if k == "exit":
        return "bad", "xmrig ended with no stop recorded", "the Mac shut down or slept, xmrig crashed, or it was killed"
    if k == "reject":
        if e.get("approx"):
            n = int(e.get("count") or 1)
            return "bad", f"{approx}{n} share{'s' if n > 1 else ''} rejected", "exact time needs the new release on that Mac"
        det = " · ".join(x for x in (f'"{e["why"]}"' if e.get("why") else "", f"diff {int(e.get('diff') or 0):,}",
                                     f"{int(e.get('ms') or 0):,} ms") if x)
        return "bad", f"Share rejected #{int(e.get('n') or 0):,}", det
    if k == "error":
        if e.get("approx"):
            n = int(e.get("count") or 1)
            return "bad", f"{approx}Pool connection failed" + (f" ×{n}" if n > 1 else ""), "exact time needs the new release on that Mac"
        n = int(e.get("n") or 1)
        msg = e.get("msg") or "error"
        title = ("No pool answers" if msg.startswith("no active pools") else f"Pool {msg}") + (f" ×{n}" if n > 1 else "")
        bits = [e.get("pool") or ""]
        if n > 1 and e.get("last"):
            bits.append(f"until {when(e['last'], e['ts'])}")
        if e.get("idle"):
            bits.append("mining waited for a pool")
        return "bad", title, " · ".join(b for b in bits if b)
    if k == "backup":
        return "warn", "Switched to the backup pool", e.get("pool") or ""
    if k == "pool":
        if e.get("main"):
            return "good", "Back on the main pool", e.get("pool") or ""
        return "good", f"Reconnected to {e.get('pool') or 'the pool'}", (f"after {e['down']} s" if e.get("down") is not None else "")
    if k == "pause":
        return "warn", "Paused · keyboard or mouse in use", ""
    if k == "resume":
        return "good", "Mining again", ""
    if k == "denied":
        return "bad", "Refused a command", why
    return "info", str(k), ""


# ---------------------------------------------------------------------- window inbox
def take_requests(max_age: float = MAX_AGE) -> list[dict]:
    """The window's side: commands the helper passed on, each claimed once (renamed to .work)."""
    try:
        names = sorted(n for n in os.listdir(INBOX) if n.endswith(".json"))
    except OSError:
        return []
    out = []
    for n in names:
        nonce = n[:-5]
        work = os.path.join(INBOX, nonce + ".work")
        try:
            os.rename(os.path.join(INBOX, n), work)
        except OSError:
            continue  # the helper gave up on it, or another window took it
        req = read_json(work)
        if not req or req.get("cmd") not in CMDS or time.time() - float(req.get("ts") or 0) > max_age:
            finish_request(nonce, {"ok": False, "error": "expired before the window saw it"})
            continue
        req["nonce"] = nonce
        out.append(req)
    return out


def finish_request(nonce: str, result: dict) -> None:
    write_json(os.path.join(INBOX, nonce + ".done"), result)
    try:
        os.remove(os.path.join(INBOX, nonce + ".work"))
    except OSError:
        pass


def to_window(msg: dict, pickup: float = WINDOW_PICKUP, done: float = WINDOW_DONE) -> Optional[dict]:
    """Hand a command to the open window and wait for its answer. None: it never took it (busy)."""
    nonce = msg["nonce"]
    req = os.path.join(INBOX, nonce + ".json")
    fin = os.path.join(INBOX, nonce + ".done")
    write_json(req, {"cmd": msg["cmd"], "from": msg.get("from") or "", "ts": time.time()})
    t0 = time.time()
    while True:
        if os.path.exists(fin):
            res = read_json(fin) or {"ok": False, "error": "unreadable answer"}
            try:
                os.remove(fin)
            except OSError:
                pass
            return res
        el = time.time() - t0
        if el > pickup and os.path.exists(req):
            try:
                os.remove(req)
                return None
            except OSError:
                pass  # taken this instant: wait for the answer
        if el > done:
            return {"ok": False, "error": "the window there took over a minute"}
        time.sleep(0.1)


# ---------------------------------------------------------------------- the helper (every Mac but the main one)
def load_seen() -> dict:
    d = read_json(SEEN) or {}
    now = time.time()
    return {k: v for k, v in d.items() if isinstance(v, (int, float)) and now - v < 3 * MAX_AGE}


class Helper:
    """What answers on :18089. The window / xmrig checks and minerctl are swappable for the self-test."""

    def __init__(self, port: Optional[int] = None, bind: str = "0.0.0.0",
                 is_window: Callable[[], bool] = ui_open, is_running: Callable[[], bool] = xmrig_running) -> None:
        self.port = PORT if port is None else port
        self.bind = bind
        self.me = fleet.local_worker()
        self.token = fleet.read_token()
        self.is_window = is_window
        self.is_running = is_running
        self.lock = threading.Lock()
        self.seen = load_seen()
        self.denied_at = 0.0
        self.version = code_version()
        self.pickup = WINDOW_PICKUP
        self.httpd: Optional[ThreadingHTTPServer] = None

    def hello(self) -> dict:
        win, run = self.is_window(), self.is_running()
        return {"v": 1, "worker": self.me, "window": win, "xmrig": run, "version": self.version,
                "can": list(CMDS) if win else (["stop"] if run else []), "key": fingerprint(pub_line())}

    def deny(self, why: str) -> None:
        if time.time() - self.denied_at > 60:  # a burst of junk is one line, not a flood
            self.denied_at = time.time()
            try:
                note("denied", why=why)
            except Exception:
                pass

    def command(self, body: bytes, ip: str) -> tuple[int, dict]:
        try:
            outer = json.loads(body)
            msg_s, sig = outer["msg"], outer["sig"]
            if not (isinstance(msg_s, str) and isinstance(sig, str) and len(msg_s) < 2048 and len(sig) < 4096):
                raise ValueError
            msg = json.loads(msg_s)
        except Exception:
            return 400, {"ok": False, "error": "bad request"}
        with self.lock:  # one command at a time (and one writer of the allowed-signers file)
            if not verify(msg_s.encode(), sig):
                self.deny(f"bad signature from {ip}")
                return 403, {"ok": False, "error": "bad signature: this Mac's control.pub is not the main Mac's key "
                                                   "(release from the main Mac, then reopen XMR Miner here)"}
            now = time.time()
            self.seen = {k: v for k, v in self.seen.items() if now - v < 3 * MAX_AGE}
            why = check_cmd(msg, self.me, self.seen, now)
            if why:
                self.deny(f"{why} · from {ip}")
                return 409, {"ok": False, "error": why}
            self.seen[msg["nonce"]] = now
            try:
                write_json(SEEN, self.seen)
            except OSError:
                pass
            res = self.execute(msg)
        res.setdefault("worker", self.me)
        return 200, res

    def execute(self, msg: dict) -> dict:
        cmd, frm = msg["cmd"], msg.get("from") or ""
        if self.is_window():
            res = to_window(msg, self.pickup)
            if res is not None:
                res["via"] = "window"
                return res
            if cmd != "stop":
                return {"ok": False, "error": "the XMR Miner window there did not answer (bench or editor open?)"}
        elif cmd != "stop":
            return {"ok": False, "error": f"XMR Miner is not open on {short(self.me)}: open it there to {cmd} mining"}
        return self.direct_stop(frm)

    def direct_stop(self, frm: str) -> dict:
        """The window is closed (or busy): stop xmrig the way t does, without the window."""
        if not self.is_running():
            return {"ok": True, "state": "stopped", "lines": ["Not running."], "via": "helper"}
        api, _ = fleet.api_json(f"http://{fleet.LOCAL}:{fleet.PORT}/2/summary", self.token, 1.5)
        env = dict(os.environ, MINER_WHY="remote", MINER_BY=frm)
        try:
            p = subprocess.run([CTL, "stop"], env=env, capture_output=True, text=True, timeout=45)
            lines = [ln.rstrip() for ln in (p.stdout + p.stderr).splitlines() if ln.strip()]
        except Exception as e:
            lines = [f"stop failed: {e}"]
        ok = any(ln.startswith(("Stopped", "Not running")) for ln in lines)
        res = {"ok": ok, "state": "running" if self.is_running() else "stopped", "lines": lines[-4:], "via": "helper"}
        if isinstance(api, dict):
            b = fleet.brief(api)
            res.update(up=b["up"], acc=b["acc"], rej=b["rej"])
        return res

    def events(self, since: float) -> list[dict]:
        return [e for e in local_events(self.me) if max(e["ts"], e.get("last") or 0) >= since][-500:]

    def server(self) -> ThreadingHTTPServer:
        helper = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "xmr-miner-control/1"
            sys_version = ""

            def log_message(self, fmt: str, *args) -> None:
                pass

            def send(self, code: int, obj: dict) -> None:
                body = json.dumps(obj).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def authed(self) -> bool:
                got = self.headers.get("Authorization") or ""
                if helper.token and hmac.compare_digest(got.encode(), f"Bearer {helper.token}".encode()):
                    return True
                self.send(401, {"error": "token"})
                return False

            def do_GET(self) -> None:
                u = urllib.parse.urlsplit(self.path)
                if not self.authed():
                    return
                if u.path == "/v1/hello":
                    self.send(200, helper.hello())
                elif u.path == "/v1/events":
                    q = urllib.parse.parse_qs(u.query)
                    try:
                        since = float((q.get("since") or ["0"])[0])
                    except ValueError:
                        since = 0.0
                    self.send(200, {"worker": helper.me, "events": helper.events(since)})
                else:
                    self.send(404, {"error": "not found"})

            def do_POST(self) -> None:
                if urllib.parse.urlsplit(self.path).path != "/v1/cmd":
                    self.send(404, {"error": "not found"})
                    return
                if not self.authed():
                    return
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                except ValueError:
                    n = 0
                if n <= 0 or n > 16384:
                    self.send(413, {"error": "size"})
                    return
                code, obj = helper.command(self.rfile.read(n), self.client_address[0])
                self.send(code, obj)

        httpd = ThreadingHTTPServer((self.bind, self.port), Handler)
        httpd.daemon_threads = True
        self.httpd = httpd
        self.port = httpd.server_address[1]
        return httpd


def helper_pid() -> Optional[int]:
    d = read_json(PIDFILE) or {}
    pid = d.get("pid")
    if isinstance(pid, int) and pid > 0 and pid_alive(pid) and "control.py" in pid_command(pid):
        return pid
    return None


def serve() -> int:
    """The helper's life: listen while the window is open or xmrig runs; restart on a new release."""
    if is_main():
        print("This is the main Mac: it sends commands, so it runs no helper.")
        return 0
    if not fleet.read_token() or not pub_line():
        print("No fleet.token or control.pub here: nothing to listen for.")
        return 1
    h = Helper()
    try:
        httpd = h.server()
    except OSError as e:
        print(f"port {PORT} is busy ({e}); another helper is probably running.")
        return 1
    write_json(PIDFILE, {"pid": os.getpid(), "port": h.port, "version": h.version, "started": time.time()})
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    threading.Thread(target=httpd.serve_forever, name="control", daemon=True).start()
    print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} helper {h.version} on :{h.port} for {h.me}", flush=True)
    restart = False
    idle = 0
    try:
        while True:
            time.sleep(2)
            idle = 0 if (ui_open() or xmrig_running()) else idle + 1
            if idle >= 2:
                print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} no window and no miner: exiting", flush=True)
                break
            if code_version() != h.version:
                print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} new release: restarting", flush=True)
                restart = True
                break
    finally:
        httpd.shutdown()
        httpd.server_close()
        if (read_json(PIDFILE) or {}).get("pid") == os.getpid() and not restart:
            try:
                os.remove(PIDFILE)
            except OSError:
                pass
    if restart:
        os.execv(sys.executable, [sys.executable, os.path.abspath(__file__), "serve"])
    return 0


def ensure() -> str:
    """Start the helper here unless it runs, this is the main Mac, or the fleet is off (LAN=off)."""
    if is_main():
        return "main Mac: no helper"
    if not fleet.read_token():
        return "no fleet.token"
    if not pub_line():
        return "no control.pub"
    if fleet.job_arg("--http-host") not in ("", "0.0.0.0"):
        return "LAN=off: this Mac stays private"
    if helper_pid():
        return "running"
    os.makedirs(LOGS, exist_ok=True)
    lock = os.path.join(LOGS, "control.lock")
    try:
        os.mkdir(lock)
    except FileExistsError:
        try:
            if time.time() - os.stat(lock).st_mtime < 30:
                return "starting"
            os.rmdir(lock)
            os.mkdir(lock)
        except OSError:
            return "starting"
    try:
        try:
            if os.path.getsize(HELPER_LOG) > 1_000_000:
                os.remove(HELPER_LOG)
        except OSError:
            pass
        with open(HELPER_LOG, "a") as out:
            subprocess.Popen([sys.executable, os.path.abspath(__file__), "serve"], cwd=ROOT, stdin=subprocess.DEVNULL,
                             stdout=out, stderr=subprocess.STDOUT, start_new_session=True, close_fds=True,
                             env=dict(os.environ, MINER_ROOT=ROOT))
        for _ in range(30):
            if helper_pid():
                return "started"
            time.sleep(0.1)
        return "starting"
    finally:
        try:
            os.rmdir(lock)
        except OSError:
            pass


# ---------------------------------------------------------------------- the main Mac's side
def send(host: str, to: str, cmd: str, token: Optional[str] = None, frm: Optional[str] = None,
         port: Optional[int] = None, timeout: float = 75.0) -> dict:
    """One signed command to one Mac's helper. Always returns {ok, …, error?}."""
    token = fleet.read_token() if token is None else token
    try:
        msg = make_cmd(cmd, to, frm or fleet.local_worker())
        body = json.dumps({"msg": msg, "sig": sign(msg.encode())}).encode()
    except Exception as e:
        return {"ok": False, "error": str(e)}
    req = urllib.request.Request(f"http://{host}:{port or PORT}/v1/cmd", data=body, method="POST",
                                 headers={"Content-Type": "application/json", "User-Agent": "xmr-miner-control/1",
                                          "Authorization": f"Bearer {token}"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            d = json.load(r)
            return d if isinstance(d, dict) else {"ok": False, "error": "bad answer"}
    except urllib.error.HTTPError as e:
        try:
            d = json.load(e)
            if isinstance(d, dict) and d.get("error"):
                return dict(d, ok=False)
        except Exception:
            pass
        return {"ok": False, "error": "wrong fleet.token there" if e.code == 401 else f"HTTP {e.code}"}
    except Exception as e:
        return {"ok": False, "error": {"refused": "no helper answers: open XMR Miner there",
                                       "timeout": "no answer: asleep, off, or another network",
                                       "unreachable": "no route: asleep, off, or another network"}.get(fleet.classify(e), str(e))}


def match_targets(arg: str, workers: list[str], me: str) -> tuple[list[str], str]:
    """'m2' → minerv3-m2-8gb; 'all' → every Mac; 'here' → this one. ([], why) when it is unclear."""
    a = (arg or "").strip().lower()
    if a in ("all", "*", "every", "everyone"):
        return list(workers), ""
    if a in ("here", "this", "me", "self"):
        return [me], ""
    exact = [w for w in workers if a in (w.lower(), short(w).lower())]
    if exact:
        return exact[:1], ""
    pref = [w for w in workers if short(w).lower().startswith(a)]
    if len(pref) == 1:
        return pref, ""
    sub = [w for w in workers if a in w.lower()]
    if len(sub) == 1 and not pref:
        return sub, ""
    cands = pref or sub
    if cands:
        return [], f"'{arg}' could be {', '.join(short(w) for w in cands)}: say which"
    return [], f"no Mac called '{arg}' (the fleet has {', '.join(short(w) for w in workers) or 'no Macs yet'})"


RUNNING = ("mining", "starting", "paused", "pool")
DOING = {"start": "Starting", "stop": "Stopping", "restart": "Restarting"}
DONE = {"start": "Started", "stop": "Stopped", "restart": "Restarted"}


def plan(cmd: str, workers: list[str], rows: list[dict], main: bool, have_key: bool) -> tuple[list[dict], list[tuple]]:
    """([rows to act on], [(worker, why not)]). This Mac is always allowed; the others need the main
    Mac, its key, a helper that answers with the same key, and a state the command changes."""
    by = {r["worker"]: r for r in rows}
    mine = fingerprint(pub_line())
    go, skip = [], []
    for w in workers:
        r = by.get(w)
        if r is None:
            skip.append((w, "not in the fleet view yet"))
            continue
        st = r.get("state")
        running = st in RUNNING
        if cmd == "start" and running:
            skip.append((w, "already mining"))
            continue
        if cmd in ("stop", "restart") and not running:
            skip.append((w, "not mining"))
            continue
        if r.get("here"):
            go.append(r)
            continue
        if not main:
            skip.append((w, "only the main Mac can start or stop other Macs"))
            continue
        if not have_key:
            skip.append((w, "this Mac has no control key yet: ./bin/minerctl.sh remote setup"))
            continue
        ctl = r.get("ctl")
        if not ctl:
            skip.append((w, ctl_why(r)))
            continue
        if ctl.get("key") != mine:
            skip.append((w, "it has another control key: release from here, then reopen XMR Miner there"))
            continue
        if cmd not in (ctl.get("can") or []):
            skip.append((w, f"XMR Miner is not open there: open it to {cmd} mining"))
            continue
        go.append(r)
    return go, skip


def ctl_why(r: dict) -> str:
    """Why a Mac cannot take commands right now, in words."""
    st = r.get("ctl_st")
    if r.get("state") in ("offline",) or st in ("timeout", "unreachable", "dns"):
        return "can't reach it: asleep, off, or another network"
    if st == "auth":
        return "its fleet.token differs: reopen XMR Miner there"
    if r.get("via") == "pool" or not r.get("host"):
        return "not on this LAN (only the pool sees it)"
    if r.get("state") in RUNNING:
        return "no helper there yet: reopen XMR Miner on it once (new release)"
    return "open XMR Miner there to control it"


def ctl_label(r: dict, main: bool) -> str:
    """The fleet card's note under a Mac: what the main Mac can do to it from here."""
    if r.get("here"):
        return ""
    ctl = r.get("ctl")
    if not ctl:
        return ctl_why(r)
    if ctl.get("key") != fingerprint(pub_line()):
        return "control key differs: release, then reopen XMR Miner there"
    can = ctl.get("can") or []
    if not main:
        return "XMR Miner open there" if ctl.get("window") else "helper running there"
    if "start" in can:
        return "XMR Miner open there · s start · t stop · r restart"
    if "stop" in can:
        return "window closed there · t stops it · start needs the window"
    return "helper answers · open XMR Miner there to start it"


# ---------------------------------------------------------------------- every Mac's events (main Mac)
class Feed:
    """Every Mac's events for the main Mac, kept in logs/fleet-events.jsonl: this Mac's own, the
    others' from their helpers (exact times), or from their xmrig counters (≈ the poll time) when a
    Mac has no helper yet."""

    def __init__(self, me: Optional[str] = None, path: Optional[str] = None, remote: bool = True) -> None:
        self.me = me or fleet.local_worker()
        self.path = path or FLEET_EVENTS
        self.remote = remote
        self.lock = threading.Lock()
        self.store: dict[str, dict] = {}
        self.base: dict[str, tuple] = {}
        self.seq = 0
        self.seqs: list[int] = []  # every change in order (seq, id), for the window's ledger
        self.ids: list[str] = []
        self._sorted: Optional[list] = None  # the store by time, rebuilt after a change
        if remote:
            for e in read_jsonl(self.path, 8_000_000):
                if e.get("id"):
                    self.store[e["id"]] = e

    def add(self, evs: list[dict], save: bool = True) -> list[dict]:
        changed = []
        with self.lock:
            for e in evs:
                e = dict(e)
                e.setdefault("id", event_id(e))
                if self.store.get(e["id"]) != e:
                    self.store[e["id"]] = e
                    self.seq += 1
                    self.seqs.append(self.seq)
                    self.ids.append(e["id"])
                    changed.append(e)
            if changed:
                self._sorted = None
                if len(self.seqs) > 40000:
                    del self.seqs[:20000], self.ids[:20000]
        if changed and save and self.remote:
            try:
                append_jsonl(self.path, changed)
                if os.path.getsize(self.path) > 6_000_000:
                    self.compact()
            except OSError:
                pass
        return changed

    def compact(self) -> None:
        with self.lock:
            keep = sorted(self.store.values(), key=lambda e: e["ts"])[-20000:]
            self.store = {e["id"]: e for e in keep}
            self._sorted = None
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            for e in keep:
                f.write(json.dumps(e, separators=(",", ":")) + "\n")
        os.replace(tmp, self.path)

    def since(self, worker: str) -> float:
        with self.lock:
            return max((max(e["ts"], e.get("last") or 0) for e in self.store.values() if e.get("worker") == worker), default=0.0)

    def counters(self, r: dict, now: Optional[float] = None) -> list[dict]:
        """No helper there: a rise in its rejected / failed-connection counts is an event at poll time."""
        if r.get("acc") is None:
            return []
        w, now = r["worker"], now or time.time()
        cur = (int(r.get("up") or 0), int(r.get("rej") or 0), int(r.get("failures") or 0))
        prev = self.base.get(w)
        self.base[w] = cur
        if prev is None or cur[0] < prev[0]:
            return []  # the first look, or xmrig restarted there: a new baseline
        out = []
        if cur[1] > prev[1]:
            out.append({"k": "reject", "ts": round(now, 3), "worker": w, "approx": True, "count": cur[1] - prev[1]})
        if cur[2] > prev[2]:
            out.append({"k": "error", "ts": round(now, 3), "worker": w, "approx": True, "count": cur[2] - prev[2]})
        return self.add(out)

    def refresh(self, rows: list[dict], token: Optional[str] = None) -> None:
        self.add(local_events(self.me))
        if not self.remote:
            return
        token = fleet.read_token() if token is None else token
        for r in rows:
            if r.get("here"):
                continue
            if r.get("ctl") and r.get("host"):
                since = max(0.0, self.since(r["worker"]) - 900)  # an outage still in progress can grow
                d, st = fleet.http_json(f"http://{r['host']}:{PORT}/v1/events?since={since:.3f}", token, 4.0)
                if st == "ok" and isinstance(d, dict):
                    evs = [dict(e, worker=r["worker"]) for e in d.get("events") or [] if isinstance(e, dict) and e.get("k")]
                    for e in evs:
                        e["id"] = event_id(e)
                    self.add(evs)
                    self.base.pop(r["worker"], None)
                    continue
            self.counters(r)

    def latest(self, n: int = 200, include_hidden: bool = False) -> list[dict]:
        """The newest n events, oldest first. Cheap on every frame: sorted once per change."""
        with self.lock:
            if self._sorted is None:
                self._sorted = sorted(self.store.values(), key=lambda e: e["ts"])
            evs = self._sorted
        out = []
        for e in reversed(evs):
            if include_hidden or describe(e)[0] != "hide":
                out.append(e)
                if len(out) >= n:
                    break
        return out[::-1]

    def changed_after(self, seq: int) -> list[dict]:
        """Events added or changed after seq (the window books them in its ledger)."""
        with self.lock:
            i = bisect.bisect_right(self.seqs, seq)
            return [dict(self.store[x], _seq=q) for q, x in zip(self.seqs[i:], self.ids[i:]) if x in self.store]


class FeedWatcher:
    """The window's background refresh of the Feed: every 20 s, never on the draw loop."""

    def __init__(self, rows: Callable[[], list], remote: bool, every: float = 20.0) -> None:
        self.feed = Feed(remote=remote)
        self.rows = rows
        self.every = every
        self._wake = threading.Event()
        self._stop = False
        threading.Thread(target=self._run, name="events", daemon=True).start()

    def _run(self) -> None:
        while not self._stop:
            try:
                self.feed.refresh(self.rows() or [])
            except Exception:
                pass
            self._wake.wait(self.every)
            self._wake.clear()

    def poke(self) -> None:
        self._wake.set()


# ---------------------------------------------------------------------- CLI
def print_events(evs: list[dict], color: bool) -> None:
    c = (lambda n: f"\033[38;5;{n}m") if color else (lambda n: "")
    tone_c = {"good": c(41), "bad": c(203), "warn": c(220), "info": c(253)}
    sec, reset = c(246), ("\033[0m" if color else "")
    now = time.time()
    for e in evs:
        tone, title, det = describe(e, now)
        print(f"{sec}{when(e['ts'], now):>14}  {short(e.get('worker') or ''):<16}{reset} {tone_c.get(tone, '')}"
              f"{MARKS.get(e['k'], '•')} {title}{reset}" + (f"{sec} · {det}{reset}" if det else ""))


def cli_send(args: list[str]) -> int:
    if len(args) < 2 or args[0] not in CMDS:
        print("Usage: ./bin/minerctl.sh remote start|stop|restart <mac | all>   (m2, i7, all …)")
        return 1
    cmd, target = args[0], args[1]
    f = fleet.Fleet()
    f.refresh()
    rows = f.rows()
    workers = [r["worker"] for r in rows]
    ws, why = match_targets(target, workers, f.me)
    if why:
        print(why)
        return 1
    go, skip = plan(cmd, ws, rows, is_main(), os.path.isfile(KEY))
    for w, reason in skip:
        print(f"  {short(w)}: skipped · {reason}")
    rc = 0 if go or not skip else 1
    for r in go:
        w = r["worker"]
        if r.get("here"):
            p = subprocess.run([CTL, cmd], env=dict(os.environ, MINER_WHY="cli"), capture_output=True, text=True, timeout=60)
            out = [ln for ln in (p.stdout + p.stderr).splitlines() if ln.strip()]
            print(f"  {short(w)} (this Mac): " + (" · ".join(out[:2]) or "done"))
            continue
        print(f"  {short(w)}: {DOING[cmd].lower()}…", flush=True)
        res = send(r["host"], w, cmd, f.token)
        if res.get("ok"):
            print(f"  {short(w)}: {DONE[cmd].lower()} at {when(time.time())}" + (f" · via the {res.get('via')}" if res.get("via") else ""))
        else:
            rc = 1
            print(f"  {short(w)}: FAILED · {res.get('error') or ' · '.join(res.get('lines') or []) or 'no answer'}")
    return rc


def cli_events(args: list[str]) -> int:
    n = 40
    if "-n" in args:
        i = args.index("-n")
        if i + 1 < len(args) and args[i + 1].isdigit():
            n = int(args[i + 1])
    here_only = "--here" in args or not is_main()
    feed = Feed(remote=not here_only)
    if here_only:
        feed.add(local_events(), save=False)
    else:
        f = fleet.Fleet()
        f.refresh(allow_scan=False)
        feed.refresh(f.rows(), f.token)
    evs = feed.latest(n, include_hidden="--all" in args)
    if not evs:
        print("No events yet.")
        return 0
    print_events(evs, sys.stdout.isatty())
    return 0


def main(argv: Optional[list] = None) -> int:
    a = list(sys.argv[1:] if argv is None else argv)
    if not a or a[0] in ("-h", "--help", "help"):
        print("control.py serve                      the helper (started by XMR Miner and minerctl start)\n"
              "control.py ensure                     start the helper here if it should run\n"
              "control.py send start|stop|restart <mac|all>   main Mac: signed command\n"
              "control.py events [-n N] [--here] [--all]      timed event log\n"
              "control.py keygen                     main Mac: make the signing key + control.pub\n"
              "control.py note start|stop [--why W] [--by NAME] [--api-stdin]\n"
              "control.py --self-test")
        return 0
    if a[0] == "--self-test":
        return self_test()
    if a[0] == "serve":
        return serve()
    if a[0] == "ensure":
        print(ensure())
        return 0
    if a[0] == "send":
        return cli_send(a[1:])
    if a[0] == "events":
        return cli_events(a[1:])
    if a[0] == "keygen":
        if not is_main():
            print("Only the main Mac holds the control key (./bin/minerctl.sh role).")
            return 1
        print(f"control key: {keygen(force='--force' in a)} · {KEY}")
        print(f"control.pub: {fingerprint(pub_line())} · release so the other Macs get it")
        return 0
    if a[0] == "note" and len(a) > 1:
        kv = {}
        i = 2
        while i < len(a):
            if a[i] in ("--why", "--by") and i + 1 < len(a):
                kv[a[i][2:]] = a[i + 1]
                i += 2
            else:
                i += 1
        api = None
        if "--api-stdin" in a:
            try:
                api = json.load(sys.stdin)
            except Exception:
                api = None
        note(a[1], why=kv.get("why", ""), by=kv.get("by", ""), api=api if isinstance(api, dict) else None)
        return 0
    print(f"unknown: {' '.join(a)}  (control.py --help)")
    return 1


# ---------------------------------------------------------------------- self-test
def self_test() -> int:
    global KEY, CTL
    fails = 0

    def check(name: str, cond: bool) -> None:
        nonlocal fails
        print(("ok  " if cond else "FAIL") + " " + name)
        fails += 0 if cond else 1

    t0 = time.mktime((2026, 9, 23, 18, 52, 4, 0, 0, -1))
    check("when: today", when(t0, t0 + 60) == "18:52:04")
    check("when: another day", when(t0 - 86400 * 2, t0) == "09-21 18:52:04")
    ws = ["minerv3-m4-16gb", "minerv3-m2-8gb", "minerv3-i7-6700hq-16gb"]
    check("target m2", match_targets("m2", ws, ws[0]) == (["minerv3-m2-8gb"], ""))
    check("target i7 + full name", match_targets("i7", ws, ws[0])[0] == [ws[2]] and match_targets("minerv3-i7-6700hq-16gb", ws, ws[0])[0] == [ws[2]])
    check("target all / here", match_targets("all", ws, ws[0])[0] == ws and match_targets("here", ws, ws[0])[0] == [ws[0]])
    check("target m (ambiguous)", match_targets("m", ws, ws[0])[0] == [] and "could be" in match_targets("m", ws, ws[0])[1])
    check("target unknown", "no Mac called" in match_targets("pi", ws, ws[0])[1])

    L = [
        " * ABOUT        XMRig/6.26.0 clang/16.0.0",
        "[2026-09-21 19:15:12.589]  net      use pool gulf.moneroocean.stream:20016 TLSv1.3 2402:1f00:8001:86d::1",
        "[2026-09-21 19:16:00.000]  cpu      accepted (1/0) diff 160003 (120 ms)",
        '[2026-09-22 01:02:47.267]  cpu      rejected (721/1) diff 131388 "Throttled down share submission (please increase difficulty)" (67148 ms)',
        '[2026-09-22 02:00:00.000]  net      [gulf.moneroocean.stream:20016] 205.172.58.170 connect error: "connection timed out"',
        '[2026-09-22 02:00:05.000]  net      [gulf.moneroocean.stream:20016] 205.172.58.170 connect error: "connection timed out"',
        "[2026-09-22 02:00:06.000]  net      no active pools, stop mining",
        '[2026-09-22 02:00:10.000]  net      [gulf.moneroocean.stream:20016] connect error: "connection timed out"',
        "[2026-09-22 02:00:30.000]  net      use pool de.moneroocean.stream:20016 TLSv1.3 103.7.55.233",
        "[2026-09-22 02:30:00.000]  net      use pool gulf.moneroocean.stream:20016 TLSv1.3 205.172.58.170",
        '[2026-09-22 03:00:00.000]  net      DNS error: "temporary failure in name resolution"',
        "[2026-09-22 03:00:20.000]  net      use pool gulf.moneroocean.stream:20016 TLSv1.3 205.172.58.170",
        "[2026-09-22 03:10:00.000]  miner    user active",
        "[2026-09-22 03:10:00.000]  miner    paused, press  r  to resume",
        "[2026-09-22 03:12:00.000]  miner    resumed",
        "[2026-09-22 08:35:08.653]  cpu      accepted (1621/1) diff 135706 (546 ms)",
        " * ABOUT        XMRig/6.26.0 clang/16.0.0",
        "[2026-09-23 09:00:00.000]  net      use pool gulf.moneroocean.stream:20016 TLSv1.3 205.172.58.170",
        "[2026-09-23 09:30:00.000]  miner    speed 10s/60s/15m 4295.1 4311.4 4210.5 H/s max 4454.4 H/s",
    ]
    ev, sess = log_events(L, backup="de.moneroocean.stream:20016")
    kinds = [e["k"] for e in ev]
    check("log: kinds in order", kinds == ["start", "reject", "error", "backup", "pool", "error", "pool", "pause", "resume", "start"])
    rej = ev[1]
    check("log: reject fields", rej["n"] == 722 and rej["diff"] == 131388 and rej["ms"] == 67148 and rej["why"].startswith("Throttled")
          and when(rej["ts"], rej["ts"]) == "01:02:47")
    out = ev[2]
    check("log: one outage, 4 lines", out["n"] == 4 and out["idle"] and out["pool"] == "gulf.moneroocean.stream:20016"
          and out["msg"] == 'connect error: "connection timed out"' and when(out["last"], out["ts"]) == "02:00:10")
    check("log: backup, then back on main", ev[3]["pool"] == "de.moneroocean.stream:20016" and ev[4].get("main"))
    check("log: DNS outage reconnects", ev[5]["msg"].startswith("DNS error") and ev[6].get("down") == 20)
    check("log: pause once (user active + paused)", kinds.count("pause") == 1)
    check("log: two sessions", len(sess) == 2 and when(sess[0][1], sess[0][1]) == "08:35:08")
    tone, title, det = describe(out)
    check("describe outage", tone == "bad" and title == 'Pool connect error: "connection timed out" ×4'
          and "until 02:00:10" in det and "mining waited for a pool" in det)
    check("describe reject", describe(rej)[1] == "Share rejected #722" and describe(rej)[2].startswith('"Throttled down'))
    s1, s2 = sess[0][0], sess[1][0]
    own = [{"k": "start", "ts": s1 - 0.5, "why": "window"}]
    m = merge(ev, sess, own, running=True, now=s2 + 3600)
    check("merge: a log start next to a note is one start", [e["k"] for e in m].count("start") == 2 and m[0].get("why") == "window")
    check("merge: a run with no stop note gets an exit", any(e["k"] == "exit" and abs(e["ts"] - sess[0][1]) < 0.01 for e in m))
    m2 = merge(ev, sess, own + [{"k": "stop", "ts": sess[0][1] + 1, "why": "remote", "by": "minerv3-m4-16gb"}], running=True, now=s2 + 3600)
    check("merge: a stop note means no exit", not any(e["k"] == "exit" for e in m2))
    check("merge: the running session is not an exit", not any(e["k"] == "exit" and e["ts"] > s2 for e in m))
    m3 = merge(ev, sess, [], running=False, now=s2 + 3600)
    check("merge: no exits before notes began", not any(e["k"] == "exit" for e in m3))
    m4 = merge(ev, sess, own, running=False, now=sess[1][1] + 10)
    check("merge: a run that ended 10 s ago waits for minerctl's stop note", not any(e["k"] == "exit" and e["ts"] > s2 for e in m4))
    m5 = merge(ev, sess, own, running=True, now=s2 + 5)
    check("merge: a log start 5 s old waits for minerctl's start note", not any(e["k"] == "start" and e["ts"] == s2 for e in m5))
    check("describe stop from the M4", describe({"k": "stop", "why": "remote", "by": "minerv3-m4-16gb", "up": 3720, "acc": 23094, "rej": 14})
          == ("info", "Stopped from m4-16gb", "after 1h 2m · 23,094 ✓ · 14 ✗"))
    check("describe restart hides its stop", describe({"k": "stop", "why": "restart"})[0] == "hide"
          and describe({"k": "start", "why": "restart", "by": "minerv3-m4-16gb"})[1] == "Restarted from m4-16gb")

    old = (ROOT, KEY)
    td = tempfile.mkdtemp(prefix="control-test-")
    try:
        os.makedirs(os.path.join(td, "logs"))
        set_root(td)
        KEY = os.path.join(td, "keys", "control.key")
        # keygen writes control.pub; a second key plays the stranger
        check("keygen", keygen().startswith("made") and pub_line().startswith("ssh-ed25519 ") and os.path.isfile(KEY))
        check("keygen again keeps the key", keygen() == "exists")
        msg = make_cmd("stop", "minerv3-m2-8gb", "minerv3-m4-16gb").encode()
        sig = sign(msg)
        check("sign + verify", verify(msg, sig))
        check("tampered command fails", not verify(msg.replace(b'"stop"', b'"start"'), sig))
        real = KEY
        KEY = os.path.join(td, "keys2", "other.key")
        os.makedirs(os.path.dirname(KEY))
        subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-f", KEY], check=True, capture_output=True)
        check("another key fails", not verify(msg, sign(msg)))
        KEY = real
        now = time.time()
        good = json.loads(msg)
        check("check_cmd ok", check_cmd(good, "minerv3-m2-8gb", {}, now) == "")
        check("check_cmd wrong Mac", "meant for" in check_cmd(good, "minerv3-i7-6700hq-16gb", {}, now))
        check("check_cmd too old / future", "too old" in check_cmd(good, "minerv3-m2-8gb", {}, now + 61)
              and "too old" in check_cmd(good, "minerv3-m2-8gb", {}, now - 61))
        check("check_cmd repeat", "repeat" in check_cmd(good, "minerv3-m2-8gb", {good["nonce"]: now}, now))
        check("check_cmd bad command", "unknown command" in check_cmd(dict(good, cmd="rm"), "minerv3-m2-8gb", {}, now))

        # a live helper on a free port, with a stub minerctl and a pretend window
        calls = os.path.join(td, "calls.txt")
        stub = os.path.join(td, "minerctl-stub.sh")
        with open(stub, "w") as f:
            f.write('#!/bin/sh\necho "$1 $MINER_WHY $MINER_BY" >> "' + calls + '"\n'
                    'case "$1" in stop) echo "Stopped.";; *) echo "Started.";; esac\n')
        os.chmod(stub, 0o755)
        CTL = stub
        with open(XMRIG_LOG, "w") as f:
            f.write("\n".join(L) + "\n")
        state = {"win": False, "run": True}
        h = Helper(port=0, bind="127.0.0.1", is_window=lambda: state["win"], is_running=lambda: state["run"])
        h.me, h.token, h.pickup = "minerv3-m2-8gb", "tok", 1.0
        httpd = h.server()
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{h.port}"
        d, st = fleet.http_json(base + "/v1/hello", "", 2)
        check("helper: no token → refused", st == "auth")
        d, st = fleet.http_json(base + "/v1/hello", "tok", 2)
        check("helper: hello", st == "ok" and d["worker"] == "minerv3-m2-8gb" and d["can"] == ["stop"] and d["key"] == fingerprint(pub_line()))
        res = send("127.0.0.1", "minerv3-m2-8gb", "stop", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: stop with the window closed", res.get("ok") and res.get("via") == "helper"
              and open(calls).read().split("\n")[0] == "stop remote minerv3-m4-16gb")
        res = send("127.0.0.1", "minerv3-m2-8gb", "start", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: start needs the window", not res.get("ok") and "not open" in res.get("error", ""))
        state["win"] = True
        answered: list = []

        def window() -> None:
            for _ in range(50):
                for req in take_requests():
                    answered.append(req)
                    finish_request(req["nonce"], {"ok": True, "state": "running", "lines": ["Started."]})
                    return
                time.sleep(0.05)

        threading.Thread(target=window, daemon=True).start()
        res = send("127.0.0.1", "minerv3-m2-8gb", "start", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: start goes through the window", res.get("ok") and res.get("via") == "window"
              and answered and answered[0]["cmd"] == "start" and answered[0]["from"] == "minerv3-m4-16gb")
        res = send("127.0.0.1", "minerv3-m2-8gb", "stop", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: a busy window → stop still happens", res.get("ok") and res.get("via") == "helper"
              and not os.listdir(INBOX))
        res = send("127.0.0.1", "minerv3-m2-8gb", "restart", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: a busy window → no start", not res.get("ok") and "did not answer" in res.get("error", ""))
        m = make_cmd("stop", "minerv3-m2-8gb", "minerv3-m4-16gb")
        body = json.dumps({"msg": m, "sig": sign(m.encode())}).encode()

        def post(b: bytes) -> tuple:
            rq = urllib.request.Request(base + "/v1/cmd", data=b, method="POST", headers={"Authorization": "Bearer tok"})
            try:
                with urllib.request.urlopen(rq, timeout=10) as r:
                    return r.status, json.load(r)
            except urllib.error.HTTPError as e:
                return e.code, json.load(e)

        state["win"] = False
        c1, _ = post(body)
        c2, r2 = post(body)
        check("helper: the same signed command twice → the second is refused", c1 == 200 and c2 == 409 and "repeat" in r2["error"])
        KEY = os.path.join(td, "keys2", "other.key")
        res = send("127.0.0.1", "minerv3-m2-8gb", "stop", "tok", "minerv3-m4-16gb", port=h.port)
        KEY = real
        check("helper: a stranger's key → 403", not res.get("ok") and "bad signature" in res.get("error", ""))
        res = send("127.0.0.1", "minerv3-i7-6700hq-16gb", "stop", "tok", "minerv3-m4-16gb", port=h.port)
        check("helper: a command for another Mac → refused", not res.get("ok") and "meant for" in res.get("error", ""))
        check("helper: refusals are noted (once a minute)", sum(1 for e in read_jsonl(EVENTS) if e["k"] == "denied") == 1)
        d, st = fleet.http_json(base + "/v1/events?since=0", "tok", 3)
        ks = [e["k"] for e in (d or {}).get("events", [])]
        check("helper: events", st == "ok" and "reject" in ks and "error" in ks and "denied" in ks
              and ("backup" in ks or not backup_pool()))
        httpd.shutdown()
        httpd.server_close()

        # the main Mac's feed: helper events are exact; a Mac without one is counted from its xmrig
        feed = Feed(me="minerv3-m4-16gb", path=os.path.join(td, "logs", "fleet-events.jsonl"))
        feed.add([dict(e, worker="minerv3-m2-8gb") for e in ev[:3]])
        grown = dict(ev[2], worker="minerv3-m2-8gb", n=9)
        feed.add([grown])
        again = Feed(me="minerv3-m4-16gb", path=feed.path)
        check("feed: saved, and a growing outage stays one event", len(again.store) == 3
              and next(e for e in again.store.values() if e["k"] == "error")["n"] == 9)
        r_i7 = {"worker": "minerv3-i7-6700hq-16gb", "acc": 100, "rej": 0, "up": 1000, "failures": 0}
        check("feed: first look is a baseline", feed.counters(r_i7) == [])
        new = feed.counters(dict(r_i7, rej=2, failures=1, up=1060))
        check("feed: counters → ≈ events", [e["k"] for e in new] == ["reject", "error"] and all(e["approx"] for e in new)
              and describe(new[0])[1] == "≈ 2 shares rejected")
        check("feed: a restart there is a new baseline", feed.counters(dict(r_i7, rej=0, up=5)) == [])
        rows = [{"worker": "minerv3-m4-16gb", "here": True, "state": "stopped"},
                {"worker": "minerv3-m2-8gb", "here": False, "state": "mining", "host": "10.0.0.2",
                 "ctl": {"key": fingerprint(pub_line()), "can": ["stop"], "window": False}},
                {"worker": "minerv3-i7-6700hq-16gb", "here": False, "state": "offline", "host": "10.0.0.3", "ctl": None, "ctl_st": "timeout"}]
        go, skip = plan("stop", ws, rows, main=True, have_key=True)
        check("plan stop all: only the M2 (the M4 is stopped, the i7 unreachable)", [r["worker"] for r in go] == ["minerv3-m2-8gb"]
              and dict(skip)["minerv3-m4-16gb"] == "not mining")
        go, skip = plan("start", ws, rows, main=True, have_key=True)
        check("plan start: here yes, M2 already mining", [r["worker"] for r in go] == ["minerv3-m4-16gb"]
              and dict(skip)["minerv3-m2-8gb"] == "already mining")
        rows[1]["state"] = "stopped"
        go, skip = plan("start", ["minerv3-m2-8gb"], rows, main=True, have_key=True)
        check("plan start: window closed there → why", go == [] and "not open there" in dict(skip)["minerv3-m2-8gb"])
        go, skip = plan("start", ["minerv3-m2-8gb"], rows, main=False, have_key=True)
        check("plan: a follower cannot command others", "only the main Mac" in dict(skip)["minerv3-m2-8gb"])
        rows[1]["ctl"]["key"] = "000000000000"
        go, skip = plan("stop", ["minerv3-m2-8gb"], [dict(rows[1], state="mining")], main=True, have_key=True)
        check("plan: another key there → why", go == [] and "another control key" in dict(skip)["minerv3-m2-8gb"])
    finally:
        CTL = os.environ.get("MINER_CTL") or os.path.join(old[0], "bin", "minerctl.sh")
        set_root(old[0])
        KEY = old[1]
        import shutil
        shutil.rmtree(td, ignore_errors=True)
    print("self-test", "passed" if fails == 0 else f"{fails} failed")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
