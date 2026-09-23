#!/usr/bin/env python3
"""Fleet: every Mac mining to this wallet, seen from any one of them.

Two sources, merged by worker name:
  LAN   each Mac's own xmrig HTTP API (0.0.0.0:18088, Bearer token from fleet.token,
        worker_id = the worker name). Live: 10s/60s/15m, shares, uptime, pool, ping.
  pool  MoneroOcean's per-worker stats for the wallet. Works from anywhere, ~1 min behind.

Who gets polled: this Mac (127.0.0.1), the hosts in fleet.local (one host[:port] per line,
for Tailscale names or fixed IPs), and Macs a scan of this Mac's subnet found, remembered by
worker name in logs/fleet.json so a new DHCP address is found again. GETs only; every API
stays in xmrig's restricted mode (read-only, /1/config -> 403).

Each other Mac's control helper (bin/control.py, :18089) is asked /v1/hello too: it answers while
XMR Miner is open there or its miner runs, so a Mac with the miner stopped is still found, and the
main Mac learns what it may send there (start / stop / restart).
"""
from __future__ import annotations

import errno
import ipaddress
import json
import os
import plistlib
import re
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from typing import Optional

ROOT = os.environ.get("MINER_ROOT") or os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PORT = 18088
CTL_PORT = 18089  # bin/control.py's helper on every Mac but the main one
LOCAL = "127.0.0.1"
TOKEN_FILE = os.path.join(ROOT, "fleet.token")
HOSTS_FILE = os.path.join(ROOT, "fleet.local")
WALLET_FILE = os.path.join(ROOT, "wallet.local")
PLIST = os.path.join(ROOT, "com.minerv3.xmrig.plist")
CACHE = os.path.join(ROOT, "logs", "fleet.json")
POOL_API = "https://api.moneroocean.stream/miner/{wallet}/stats/allWorkers"
STATS_API = "https://api.moneroocean.stream/miner/{wallet}/stats"  # amtDue / amtPaid for the wallet
USER_API = "https://api.moneroocean.stream/user/{wallet}"          # payout_threshold
PICO = 1e12  # atomic units per XMR

LAN_EVERY = 5.0      # s between LAN polls in the watcher
POOL_EVERY = 60.0    # the pool's numbers move about once a minute
SCAN_EVERY = 300.0   # rescan at most this often, and only when a pool worker has no LAN address
STALE_SHARE = 600    # pool: no share for 10 min and 0 H/s -> idle

_TOKEN_OK = re.compile(r"^[A-Za-z0-9._~+/=-]+$")


# ---------------------------------------------------------------------- config
def read_first(path: str) -> str:
    """First line that is not blank or a # comment, stripped; '' when missing."""
    try:
        with open(path, encoding="utf-8") as f:
            for ln in f:
                ln = ln.strip()
                if ln and not ln.startswith("#"):
                    return ln
    except OSError:
        pass
    return ""


def read_token() -> str:
    tok = read_first(TOKEN_FILE).replace(" ", "")
    return tok if _TOKEN_OK.match(tok or "") else ""


def read_wallet() -> str:
    w = os.environ.get("XMR_WALLET") or read_first(WALLET_FILE)
    return "" if w in ("", "YOUR_XMR_ADDRESS") else w.replace(" ", "")


def local_worker() -> str:
    """This Mac's worker name from the job file (-u wallet.worker), or ''."""
    try:
        args = [str(a) for a in plistlib.load(open(PLIST, "rb")).get("ProgramArguments") or []]
    except Exception:
        return ""
    for i, a in enumerate(args[:-1]):
        if a == "-u" and "." in args[i + 1]:
            return args[i + 1].rsplit(".", 1)[-1]
    return ""


def job_arg(name: str) -> str:
    """Value of --name=value in the job file, or ''."""
    try:
        args = [str(a) for a in plistlib.load(open(PLIST, "rb")).get("ProgramArguments") or []]
    except Exception:
        return ""
    return next((a.split("=", 1)[1] for a in args if a.startswith(name + "=")), "")


def config_hosts(path: str = HOSTS_FILE) -> list[tuple[str, int]]:
    """fleet.local: host or host:port per line (# comments ok)."""
    out = []
    try:
        with open(path, encoding="utf-8") as f:
            for ln in f:
                ln = ln.split("#", 1)[0].strip()
                if not ln:
                    continue
                host, _, port = ln.partition(":") if ln.count(":") == 1 else (ln, "", "")
                out.append((host, int(port) if port.isdigit() else PORT))
    except OSError:
        pass
    return out


def load_cache() -> dict:
    try:
        with open(CACHE, encoding="utf-8") as f:
            d = json.load(f)
        return d if isinstance(d, dict) else {}
    except Exception:
        return {}


def save_cache(d: dict) -> None:
    try:
        os.makedirs(os.path.dirname(CACHE), exist_ok=True)
        tmp = CACHE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(d, f, indent=2)
        os.replace(tmp, CACHE)
    except OSError:
        pass


# ---------------------------------------------------------------------- HTTP
def classify(exc: BaseException) -> str:
    """refused = the host answered but nothing listens (Mac awake, miner stopped or API local-only);
    timeout/unreachable = asleep, off, or another network; dns = unknown name."""
    if isinstance(exc, urllib.error.URLError) and not isinstance(exc, urllib.error.HTTPError):
        exc = exc.reason if isinstance(exc.reason, BaseException) else exc
    if isinstance(exc, socket.gaierror):
        return "dns"
    if isinstance(exc, (TimeoutError, socket.timeout)):
        return "timeout"
    if isinstance(exc, ConnectionRefusedError):
        return "refused"
    no = getattr(exc, "errno", None)
    if no == errno.ECONNREFUSED:
        return "refused"
    if no in (errno.EHOSTUNREACH, errno.ENETUNREACH, errno.EHOSTDOWN):
        return "unreachable"
    if no == errno.ETIMEDOUT or "timed out" in str(exc):
        return "timeout"
    return "error"


def http_json(url: str, token: str = "", timeout: float = 1.5):
    """(data, status); status is ok | auth | refused | timeout | unreachable | dns | http NNN | error."""
    headers = {"User-Agent": "xmr-miner-fleet/1"}  # the pool's CDN refuses Python's default agent
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.load(r), "ok"
    except urllib.error.HTTPError as e:
        return None, "auth" if e.code in (401, 403) else f"http {e.code}"
    except ValueError:
        return None, "error"
    except Exception as e:
        return None, classify(e)


def api_json(url: str, token: str = "", timeout: float = 1.5):
    """http_json with the token, retried bare on auth: an xmrig started before fleet.token
    existed answers 401 to any Authorization header."""
    d, st = http_json(url, token, timeout)
    if st == "auth" and token:
        d2, st2 = http_json(url, "", timeout)
        if st2 == "ok":
            return d2, st2
    return d, st


def brief(d: dict) -> dict:
    """The fields the fleet view shows, from an xmrig /1/summary or /2/summary."""
    tot = (d.get("hashrate") or {}).get("total") or []
    conn = d.get("connection") or {}
    res = d.get("results") or {}

    def at(i: int):
        v = tot[i] if len(tot) > i else None
        return float(v) if isinstance(v, (int, float)) else None

    acc = conn.get("accepted", res.get("shares_good")) or 0
    rej = conn.get("rejected")
    if rej is None:
        rej = max(0, int(res.get("shares_total") or acc) - int(acc))
    hs10, hs60, hs15 = at(0), at(1), at(2)
    state = "paused" if d.get("paused") else ("mining" if (hs10 or hs60) else "starting")
    return {
        "worker": d.get("worker_id") or "",
        "state": state,
        "hs": hs10 if hs10 is not None else hs60,
        "hs10": hs10, "hs60": hs60, "hs15": hs15,
        "acc": int(acc or 0), "rej": int(rej or 0),
        "up": int(d.get("uptime") or 0),
        "pool": conn.get("pool") or "",
        "ping": conn.get("ping"),
        "failures": conn.get("failures"),
        "cpu": (d.get("cpu") or {}).get("brand") or "",
        "version": d.get("version") or "",
        "algo": d.get("algo") or "",
    }


def poll(host: str, port: int, token: str, timeout: float = 1.5) -> dict:
    d, st = api_json(f"http://{host}:{port}/2/summary", token, timeout)
    ok = st == "ok" and isinstance(d, dict) and "hashrate" in d
    return {"host": host, "port": port, "status": "ok" if ok else (st if st != "ok" else "error"),
            "brief": brief(d) if ok else None, "at": time.time()}


def hello(host: str, token: str, timeout: float = 1.5) -> tuple:
    """(answer, status) from a Mac's control helper: worker, window open, xmrig running, what it takes."""
    d, st = http_json(f"http://{host}:{CTL_PORT}/v1/hello", token, timeout)
    if st == "ok" and isinstance(d, dict) and d.get("worker"):
        return d, "ok"
    return None, (st if st != "ok" else "error")


def pool_workers(wallet: str, timeout: float = 6.0):
    """({worker: {hs, lts, valid, invalid}}, status) from MoneroOcean's allWorkers."""
    if not wallet:
        return {}, "no wallet"
    d, st = http_json(POOL_API.format(wallet=wallet), "", timeout)
    if st != "ok" or not isinstance(d, dict):
        return {}, st
    out = {}
    for name, v in d.items():
        if name == "global" or not isinstance(v, dict):
            continue
        out[name] = {"hs": float(v.get("hash") or 0.0), "lts": int(v.get("lts") or 0),
                     "valid": int(v.get("validShares") or 0), "invalid": int(v.get("invalidShares") or 0)}
    return out, "ok"


def parse_balance(stats: dict, user: Optional[dict]) -> dict:
    """XMR owed and paid from /miner/<wallet>/stats, plus the payout threshold from /user/<wallet>."""
    thr = (user or {}).get("payout_threshold")
    return {"due": float(stats.get("amtDue") or 0) / PICO, "paid": float(stats.get("amtPaid") or 0) / PICO,
            "txns": int(stats.get("txnCount") or 0),
            "threshold": float(thr) / PICO if isinstance(thr, (int, float)) and thr > 0 else None}


def pool_balance(wallet: str, timeout: float = 6.0):
    """({due, paid, txns, threshold}, status). The threshold is optional; the balance is not."""
    if not wallet:
        return None, "no wallet"
    d, st = http_json(STATS_API.format(wallet=wallet), "", timeout)
    if st != "ok" or not isinstance(d, dict):
        return None, st
    u, ust = http_json(USER_API.format(wallet=wallet), "", timeout)
    return parse_balance(d, u if ust == "ok" and isinstance(u, dict) else None), "ok"


# ---------------------------------------------------------------------- scan
def own_ipv4() -> Optional[tuple[str, int]]:
    """(address, prefix) of the interface that holds the default route; None when offline."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))  # TEST-NET: picks the route, sends nothing
        ip = s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()
    prefix = 24
    try:
        out = subprocess.check_output(["ifconfig"], text=True, timeout=2)
        m = re.search(rf"inet {re.escape(ip)} netmask 0x([0-9a-f]{{8}})", out)
        if m:
            prefix = bin(int(m.group(1), 16)).count("1")
    except Exception:
        pass
    return ip, prefix


def subnet_hosts(ip: str, prefix: int) -> list[str]:
    """Hosts of this subnet minus ourselves; never wider than /22 (1,022 addresses)."""
    net = ipaddress.ip_network(f"{ip}/{max(22, min(30, prefix))}", strict=False)
    return [str(h) for h in net.hosts() if str(h) != ip]


def scan(token: str, port: int = PORT, connect_timeout: float = 0.35) -> list[dict]:
    """Every host on this subnet whose :port answers like an xmrig API with our token, or whose
    control helper (:18089) does: a Mac with XMR Miner open but its miner stopped."""
    me = own_ipv4()
    if not me:
        return []

    def probe(h: str) -> Optional[dict]:
        try:
            socket.create_connection((h, port), connect_timeout).close()
        except OSError:
            try:
                socket.create_connection((h, CTL_PORT), connect_timeout).close()
            except OSError:
                return None
            d, st = hello(h, token)
            return {"host": h, "port": port, "status": "refused", "brief": None, "at": time.time(), "hello": d} if d else None
        r = poll(h, port, token)
        return r if r["status"] in ("ok", "auth") else None

    with ThreadPoolExecutor(64) as ex:
        return [r for r in ex.map(probe, subnet_hosts(*me)) if r]


# ---------------------------------------------------------------------- merge
ACTIVE = ("mining", "starting", "paused", "pool")
DOWN_NOTE = {
    "refused": ("stopped", "awake, miner not running"),
    "timeout": ("offline", "no answer: asleep, off, or another network"),
    "unreachable": ("offline", "no route: asleep, off, or another network"),
    "dns": ("offline", "unknown host name"),
    "auth": ("no token", "its fleet.token differs: update it (reopen XMR Miner there, then s)"),
}


def eff_hs(r: dict) -> float:
    """LAN 10 s rate when we have it, else the pool's estimate."""
    v = r.get("hs")
    return float(v) if v is not None else float(r.get("pool_hs") or 0.0)


def age(ts: Optional[float], now: Optional[float] = None) -> str:
    if not ts:
        return "—"
    s = max(0, int((now or time.time()) - ts))
    return f"{s}s" if s < 90 else f"{s // 60}m" if s < 5400 else f"{s // 3600}h" if s < 172800 else f"{s // 86400}d"


class Fleet:
    """State for one viewer: who to poll, the last LAN + pool answers, and the merged rows."""

    def __init__(self) -> None:
        self.token = read_token()
        self.wallet = read_wallet()
        self.me = local_worker()
        self.cache = load_cache()  # {"peers": {worker: {host, port, seen}}, "scanned": ts}
        self.lan: dict = {}
        self.ctl: dict = {}  # host -> {"hello": answer or None, "status"}: the control helpers
        self.pool: dict = {}
        self.pool_status = "not yet"
        self.pool_at = 0.0
        self.balance: Optional[dict] = None
        self.scan_at = float(self.cache.get("scanned") or 0)
        self.last_scan: list = []
        self.lock = threading.Lock()

    def targets(self) -> list[tuple[str, int, str]]:
        out, seen = [], set()

        def add(h: str, p: int, src: str) -> None:
            if (h, p) not in seen:
                seen.add((h, p))
                out.append((h, p, src))

        add(LOCAL, PORT, "this Mac")
        for h, p in config_hosts():
            add(h, p, "fleet.local")
        for w, v in (self.cache.get("peers") or {}).items():
            if w != self.me and v.get("host"):
                add(v["host"], int(v.get("port") or PORT), "scan")
        return out

    def remember(self, results: list) -> None:
        peers = self.cache.setdefault("peers", {})
        changed = False
        for r in results:
            b = r.get("brief")
            w = (b or {}).get("worker") or (r.get("hello") or {}).get("worker")
            if w and r["host"] != LOCAL and w != self.me:
                peers[w] = {"host": r["host"], "port": r["port"], "seen": int(r["at"])}
                changed = True
        if changed:
            save_cache(self.cache)

    def refresh_lan(self) -> None:
        ts = self.targets()
        peers = [t for t in ts if t[0] != LOCAL] if self.token else []
        with ThreadPoolExecutor(max(1, len(ts) + len(peers))) as ex:
            polls = [ex.submit(lambda t: dict(poll(t[0], t[1], self.token), src=t[2]), t) for t in ts]
            hellos = {t[0]: ex.submit(hello, t[0], self.token) for t in peers}
            res = [f.result() for f in polls]
            ctl = {h: f.result() for h, f in hellos.items()}
        for r in res:
            r["hello"] = (ctl.get(r["host"]) or (None, ""))[0]
        with self.lock:
            self.lan = {(r["host"], r["port"]): r for r in res}
            self.ctl = {h: {"hello": d, "status": st} for h, (d, st) in ctl.items()}
            self.remember(res)

    def refresh_pool(self, force: bool = False) -> None:
        if not force and time.time() - self.pool_at < POOL_EVERY:
            return
        d, st = pool_workers(self.wallet)
        bal, bst = pool_balance(self.wallet)
        with self.lock:
            self.pool_at, self.pool_status = time.time(), st
            if st == "ok":
                self.pool = d
            if bst == "ok":
                self.balance = bal

    def want_scan(self) -> bool:
        if not self.token or time.time() - self.scan_at < SCAN_EVERY:
            return False
        if not self.cache.get("scanned"):
            return True
        found = {r["brief"]["worker"] for r in self.lan.values() if r.get("brief")} | {self.me}
        return any(v["hs"] > 0 and w not in found for w, v in self.pool.items())

    def do_scan(self) -> list:
        found = scan(self.token)
        with self.lock:
            self.scan_at = time.time()
            self.cache["scanned"] = int(self.scan_at)
            self.last_scan = found
            self.remember(found)
            save_cache(self.cache)
        return found

    def refresh(self, allow_scan: bool = True) -> None:
        self.refresh_pool()
        self.refresh_lan()
        if allow_scan and self.want_scan():
            self.do_scan()
            self.refresh_lan()

    def rows(self, now: Optional[float] = None) -> list[dict]:
        """One row per Mac: this Mac first, then by hashrate."""
        now = now or time.time()
        with self.lock:
            lan, pool = list(self.lan.values()), dict(self.pool)
            peers = dict(self.cache.get("peers") or {})
            ctl = dict(self.ctl)
        known = {(v.get("host"), int(v.get("port") or PORT)): w for w, v in peers.items()}
        rows: dict[str, dict] = {}
        for r in lan:
            here = r["host"] == LOCAL
            b = r.get("brief")
            c = ctl.get(r["host"]) or {}
            hi = c.get("hello")
            base = {"here": here, "host": r["host"], "via": "lan", "pool_hs": None, "lts": None, "pool_acc": None,
                    "hs": None, "hs15": None, "acc": None, "rej": None, "up": None, "ping": None, "failures": None,
                    "note": "", "ctl": hi, "ctl_st": c.get("status")}
            if b:
                w = self.me if (here and self.me) else (b["worker"] or r["host"])
                row = dict(base, worker=w, state=b["state"], hs=b["hs"], hs15=b["hs15"], acc=b["acc"],
                           rej=b["rej"], up=b["up"], ping=b["ping"], failures=b.get("failures"))
            else:
                w = self.me if here else ((hi or {}).get("worker") or known.get((r["host"], r["port"]), r["host"]))
                state, note = DOWN_NOTE.get(r["status"], ("error", r["status"]))
                if here and state == "stopped":
                    note = "not mining"
                if hi:  # its helper answers: the Mac is awake; xmrig is stopped (or its API is still coming up)
                    state = "starting" if hi.get("xmrig") else "stopped"
                    note = "XMR Miner open there" if hi.get("window") else ""
                row = dict(base, worker=w, state=state, note=note)
            prev = rows.get(w)
            if prev is None or (prev["state"] not in ACTIVE and row["state"] in ACTIVE):
                rows[w] = row
        for w, p in pool.items():
            fresh = p["hs"] > 0 and now - p["lts"] < STALE_SHARE
            row = rows.get(w)
            if row is None:
                row = rows[w] = {"worker": w, "here": w == self.me, "host": "", "via": "pool", "hs": None,
                                 "hs15": None, "acc": None, "rej": None, "up": None, "ping": None, "failures": None,
                                 "ctl": None, "ctl_st": None,
                                 "state": "pool" if fresh else "idle",
                                 "note": "" if fresh else "no share for a while"}
                if fresh and not row["here"]:
                    row["note"] = "not found on this LAN yet: update it (reopen XMR Miner there, then s)"
            elif row["state"] not in ACTIVE and fresh and not row["here"] and not row.get("ctl"):
                # the pool still gets its shares, so the Mac is mining; only the LAN view is missing
                why = {"stopped": "its API is local-only: update it (reopen XMR Miner there, then s)",
                       "no token": row["note"]}.get(row["state"], "mining, but not reachable from here")
                row.update(state="pool", note=why)
            row["pool_hs"], row["lts"], row["pool_acc"] = p["hs"], p["lts"], p.get("valid")
        return sorted(rows.values(), key=lambda r: (not r["here"], -eff_hs(r), r["worker"]))

    def meta(self, rows: list[dict]) -> dict:
        active = [r for r in rows if r["state"] in ACTIVE]
        return {"total": sum(eff_hs(r) for r in active), "mining": len(active), "macs": len(rows),
                "pool": self.pool_status, "pool_at": self.pool_at,
                "pool_total": sum(p["hs"] for p in self.pool.values()), "balance": self.balance,
                "scan_at": self.scan_at, "token": bool(self.token), "at": time.time()}


class Watcher:
    """Background refresh for the Terminal UI: LAN every 5 s, pool every 60 s, scans when needed.
    latest() never blocks the draw loop."""

    def __init__(self, start: bool = True) -> None:
        self.fleet = Fleet()
        self.snap: Optional[tuple[list, dict]] = None
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._t = threading.Thread(target=self._run, name="fleet", daemon=True)
        if start:
            self._t.start()

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                self.fleet.refresh()
                rows = self.fleet.rows()
                self.snap = (rows, self.fleet.meta(rows))
            except Exception:
                pass
            self._wake.wait(LAN_EVERY)
            self._wake.clear()

    def latest(self) -> Optional[tuple[list, dict]]:
        return self.snap

    def poke(self) -> None:
        """Poll again now (after a start or stop), not in up to 5 s."""
        self._wake.set()

    def stop(self) -> None:
        self._stop.set()
        self._wake.set()


# ---------------------------------------------------------------------- CLI
MARK = {"mining": "●", "starting": "◐", "paused": "◐", "pool": "◍", "stopped": "○", "offline": "○",
        "idle": "○", "no token": "!", "error": "!"}


def fmt_hs(v) -> str:
    return "—" if v is None else f"{float(v):,.0f}"


def fmt_up(s) -> str:
    if not s:
        return "—"
    h, m = divmod(int(s) // 60, 60)
    return f"{h}h {m}m" if h else f"{m}m"


def table(rows: list[dict], meta: dict, color: bool = False, verbose: bool = False) -> list[str]:
    c = (lambda n: f"\033[38;5;{n}m") if color else (lambda n: "")
    ink, sec, good, bad, warn, reset = c(253), c(246), c(41), c(203), c(220), ("\033[0m" if color else "")
    tint = {"mining": good, "starting": warn, "paused": warn, "pool": warn, "no token": bad, "error": bad}
    pool_s = (f"pool {fmt_hs(meta.get('pool_total'))} H/s · {age(meta.get('pool_at'))} ago" if meta.get("pool") == "ok"
              else f"pool: {meta.get('pool')}")
    out = [f"{ink}Fleet · {meta.get('mining', 0)} of {meta.get('macs', 0)} mining · {fmt_hs(meta.get('total'))} H/s{sec}   {pool_s}{reset}", ""]
    out.append(f"{sec}  {'worker':<24} {'state':<9} {'10s H/s':>8} {'15m H/s':>8} {'shares':>9} {'up':>7} {'pool H/s':>9} {'share':>6}  via{reset}")
    now = time.time()
    for r in rows:
        acc = "—" if r.get("acc") is None else f"{r['acc']:,}" + (f"/{r['rej']}✗" if r.get("rej") else "")
        via = "this Mac" if r["here"] else ("LAN " + r["host"] if (verbose and r["via"] == "lan") else r["via"].upper() if r["via"] == "lan" else "pool")
        col = tint.get(r["state"], sec)
        out.append(f"{col}{MARK.get(r['state'], '?')}{ink} {r['worker'][:24]:<24} {col}{r['state']:<9}{ink} {fmt_hs(r.get('hs')):>8} "
                   f"{fmt_hs(r.get('hs15')):>8} {acc:>9} {fmt_up(r.get('up')):>7} {sec}{fmt_hs(r.get('pool_hs')):>9} {age(r.get('lts'), now):>6}  {via}{reset}")
        if r.get("note"):
            out.append(f"{sec}  {'':<24} └ {r['note']}{reset}")
    if not meta.get("token"):
        out += ["", f"{warn}fleet.token is missing: only this Mac's local API and the pool are shown.{reset}"]
    return out


def firewall_on() -> Optional[bool]:
    try:
        out = subprocess.check_output(["/usr/libexec/ApplicationFirewall/socketfilterfw", "--getglobalstate"],
                                      text=True, timeout=2, stderr=subprocess.DEVNULL)
        return "enabled" in out.lower()
    except Exception:
        return None


def here() -> int:
    """This Mac's side of the fleet: is its API reachable from the others?"""
    tok, host, me = read_token(), job_arg("--http-host") or "127.0.0.1", own_ipv4()
    loc = poll(LOCAL, PORT, tok)
    lan = poll(me[0], PORT, tok) if me else None
    fw = firewall_on()
    print("This Mac")
    print(f"  worker     {local_worker() or '—'}")
    print(f"  job file   API on {host}:{PORT}" + (" (LAN)" if host == "0.0.0.0" else " (this Mac only)"))
    print(f"  token      {'fleet.token' if tok else 'missing'}")
    print(f"  LAN        {me[0]}/{me[1]}" if me else "  LAN        offline")
    print(f"  local API  {loc['status']}" + (f" · {fmt_hs(loc['brief']['hs'])} H/s · worker_id {loc['brief']['worker']}" if loc["brief"] else ""))
    if lan:
        print(f"  LAN API    {lan['status']}" + (" (what the other Macs see)" if lan["status"] == "ok" else ""))
    print(f"  firewall   {'on' if fw else 'off' if fw is False else '?'}")
    tips = []
    if not tok:
        tips.append("fleet.token is missing: ./bin/minerctl.sh update (it is tracked in the repo).")
    if host != "0.0.0.0" and tok:
        tips.append("The job file keeps the API local (LAN=off in machine.local?).")
    if loc["status"] == "ok" and lan and lan["status"] != "ok" and host == "0.0.0.0":
        tips.append("The running xmrig predates the fleet job file: press t then s (or minerctl stop, start).")
    if loc["status"] == "refused":
        tips.append("The miner is not running here; the fleet sees this Mac once you press s.")
    if fw:
        xm = os.path.join(ROOT, "bin", "xmrig")
        tips.append("macOS firewall is on: click Allow when macOS asks about xmrig, or run once:")
        tips.append(f"  sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add '{xm}' --unblockapp '{xm}'")
    for t in tips:
        print(("  → " if not t.startswith("  ") else "    ") + t.strip())
    return 0


def do_scan_cli() -> int:
    f = Fleet()
    if not f.token:
        print("fleet.token is missing; nothing on the LAN will answer.")
        return 1
    me = own_ipv4()
    print(f"Scanning {me[0]}/{max(22, me[1])} for :{PORT} …" if me else "offline")
    t0 = time.time()
    found = f.do_scan()
    for r in found:
        b = r.get("brief") or {}
        w = b.get("worker") or (r.get("hello") or {}).get("worker") or "(token rejected)"
        print(f"  {r['host']:<15} {w:<24} " + (f"{fmt_hs(b.get('hs'))} H/s" if b else "miner stopped · XMR Miner helper answers"))
    print(f"{len(found)} found in {time.time() - t0:.1f} s; remembered in logs/fleet.json")
    return 0


def self_test() -> int:
    import tempfile
    fails = 0

    def check(name: str, cond: bool) -> None:
        nonlocal fails
        print(("ok  " if cond else "FAIL") + " " + name)
        fails += 0 if cond else 1

    check("classify refused", classify(ConnectionRefusedError()) == "refused"
          and classify(urllib.error.URLError(ConnectionRefusedError())) == "refused")
    check("classify timeout/dns/unreachable", classify(socket.timeout()) == "timeout" and classify(socket.gaierror()) == "dns"
          and classify(OSError(errno.EHOSTUNREACH, "no route")) == "unreachable")
    s = {"worker_id": "minerv3-m2-8gb", "uptime": 3600, "hashrate": {"total": [3200.5, 3150.0, None]},
         "connection": {"pool": "gulf.moneroocean.stream:20016", "accepted": 50, "rejected": 1, "ping": 90}}
    b = brief(s)
    check("brief", b["worker"] == "minerv3-m2-8gb" and b["hs"] == 3200.5 and b["hs15"] is None and b["acc"] == 50
          and b["rej"] == 1 and b["state"] == "mining" and b["failures"] is None)
    check("brief starting", brief({"hashrate": {"total": [None, None, None]}})["state"] == "starting")
    check("subnet /24", len(subnet_hosts("192.168.1.55", 24)) == 253 and "192.168.1.55" not in subnet_hosts("192.168.1.55", 24))
    check("subnet clamps to /22", len(subnet_hosts("10.1.2.3", 16)) == 1021)
    with tempfile.NamedTemporaryFile("w", suffix=".local", delete=False) as tf:
        tf.write("# extra hosts\nmbp.tailnet.ts.net\n100.64.0.7:18090  # other port\n\n")
    check("fleet.local parse", config_hosts(tf.name) == [("mbp.tailnet.ts.net", 18088), ("100.64.0.7", 18090)])
    os.unlink(tf.name)

    now = time.time()
    f = Fleet()
    f.me, f.token = "minerv3-m4-16gb", "t"
    f.cache = {"peers": {"minerv3-m2-8gb": {"host": "10.0.0.2", "port": PORT}}}
    local_ok = {"host": LOCAL, "port": PORT, "status": "ok", "at": now,
                "brief": brief(dict(s, worker_id="minerv3-m4-16gb", hashrate={"total": [4100.0, 4000.0, 4050.0]}))}
    m2_ok = {"host": "10.0.0.2", "port": PORT, "status": "ok", "at": now, "brief": b}
    f.lan = {(LOCAL, PORT): local_ok, ("10.0.0.2", PORT): m2_ok}
    f.pool = {"minerv3-m4-16gb": {"hs": 3900.0, "lts": int(now - 20)}, "minerv3-m2-8gb": {"hs": 3100.0, "lts": int(now - 5)},
              "minerv3-i7-6700hq-16gb": {"hs": 687.0, "lts": int(now - 16)}, "old-rig": {"hs": 0.0, "lts": int(now - 90000)}}
    rows = f.rows(now)
    names = [r["worker"] for r in rows]
    by = {r["worker"]: r for r in rows}
    check("rows: this Mac first, then by H/s", names[:3] == ["minerv3-m4-16gb", "minerv3-m2-8gb", "minerv3-i7-6700hq-16gb"])
    check("rows: LAN beats pool for H/s", by["minerv3-m2-8gb"]["hs"] == 3200.5 and by["minerv3-m2-8gb"]["pool_hs"] == 3100.0)
    check("rows: pool-only Mac", by["minerv3-i7-6700hq-16gb"]["state"] == "pool" and "reopen XMR Miner" in by["minerv3-i7-6700hq-16gb"]["note"])
    check("rows: idle worker", by["old-rig"]["state"] == "idle")
    meta = f.meta(rows)
    check("meta total", meta["mining"] == 3 and abs(meta["total"] - (4100 + 3200.5 + 687)) < 0.01)
    f.lan[("10.0.0.2", PORT)] = {"host": "10.0.0.2", "port": PORT, "status": "refused", "at": now, "brief": None}
    r2 = {r["worker"]: r for r in f.rows(now)}["minerv3-m2-8gb"]
    check("refused + fresh pool shares = mining with a local-only API", r2["state"] == "pool" and "local-only" in r2["note"])
    f.pool["minerv3-m2-8gb"] = {"hs": 0.0, "lts": int(now - 4000)}
    f.lan[("10.0.0.2", PORT)]["status"] = "timeout"
    check("timeout + stale pool = offline", {r["worker"]: r for r in f.rows(now)}["minerv3-m2-8gb"]["state"] == "offline")
    f.lan[(LOCAL, PORT)] = {"host": LOCAL, "port": PORT, "status": "refused", "at": now, "brief": None}
    r4 = {r["worker"]: r for r in f.rows(now)}["minerv3-m4-16gb"]
    check("this Mac stopped", r4["state"] == "stopped" and r4["note"] == "not mining" and r4["here"])
    f.lan[("10.0.0.2", PORT)] = {"host": "10.0.0.2", "port": PORT, "status": "refused", "at": now, "brief": None}
    f.pool["minerv3-m2-8gb"] = {"hs": 0.0, "lts": int(now - 4000)}
    f.ctl = {"10.0.0.2": {"hello": {"worker": "minerv3-m2-8gb", "window": True, "xmrig": False, "can": ["start", "stop", "restart"]}, "status": "ok"}}
    r5 = {r["worker"]: r for r in f.rows(now)}["minerv3-m2-8gb"]
    check("miner stopped, helper answers = stopped + window open", r5["state"] == "stopped" and r5["ctl"]["window"]
          and r5["note"] == "XMR Miner open there" and r5["host"] == "10.0.0.2")
    f.ctl = {}
    t = table(f.rows(now), f.meta(f.rows(now)))
    check("table", t[0].startswith("Fleet · ") and any("minerv3-i7-6700hq-16gb" in ln for ln in t) and "\033" not in "".join(t))
    check("rows: pool share count", by["minerv3-m4-16gb"]["pool_acc"] is None and by["old-rig"]["pool_acc"] is None)
    bal = parse_balance({"amtDue": 1715517028, "amtPaid": 0, "txnCount": 0}, {"payout_threshold": 300000000000})
    check("balance", abs(bal["due"] - 0.001715517028) < 1e-12 and bal["paid"] == 0 and bal["threshold"] == 0.3)
    check("balance without threshold", parse_balance({"amtDue": 5}, None)["threshold"] is None)
    check("age", age(now - 30, now) == "30s" and age(now - 600, now) == "10m" and age(None) == "—")
    print("self-test", "passed" if fails == 0 else f"{fails} failed")
    return 0 if fails == 0 else 1


def main(argv: Optional[list] = None) -> int:
    a = list(sys.argv[1:] if argv is None else argv)
    if "--self-test" in a:
        return self_test()
    if a and a[0] == "here":
        return here()
    if a and a[0] == "scan":
        return do_scan_cli()
    if a and a[0] in ("-h", "--help", "help"):
        print("fleet.py [--json] [--watch [secs]] [--no-scan] [-v]   every Mac: LAN API + pool\n"
              "fleet.py scan                                       find miners on this subnet now\n"
              "fleet.py here                                       can the other Macs see this one?\n"
              "fleet.py --self-test")
        return 0
    watch = None
    if "--watch" in a:
        i = a.index("--watch")
        nxt = a[i + 1] if i + 1 < len(a) else ""
        watch = float(nxt) if nxt.replace(".", "", 1).isdigit() else 5.0
    f = Fleet()
    color = sys.stdout.isatty() and "--json" not in a
    try:
        while True:
            f.refresh(allow_scan="--no-scan" not in a)
            rows = f.rows()
            meta = f.meta(rows)
            if "--json" in a:
                print(json.dumps({"meta": meta, "rows": rows}, indent=2))
            else:
                lines = table(rows, meta, color=color, verbose="-v" in a)
                sys.stdout.write(("\033[H\033[2J" if watch else "") + "\n".join(lines) + "\n")
                sys.stdout.flush()
            if not watch:
                return 0
            time.sleep(watch)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
