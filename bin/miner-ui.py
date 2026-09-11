#!/usr/bin/env python3
"""Claude-style miner TUI. Mining starts only on s. Slash commands filter as you type."""
from __future__ import annotations

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
PEAK_HS = 4204.0

RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[38;5;245m"
ORANGE = "\033[38;5;208m"
GREEN = "\033[32m"
RED = "\033[31m"
BLUE = "\033[38;5;75m"
WHITE = "\033[37m"
UNDER = "\033[4m"
REV = "\033[7m"
HIDE = "\033[?25l"
SHOW = "\033[?25h"
CLEAR = "\033[2J\033[H"


@dataclass(frozen=True)
class Cmd:
    name: str
    desc: str
    action: str


COMMANDS = (
    Cmd("/usage", "Show speed, shares, uptime, and activity stats", "usage"),
    Cmd("/status", "Show miner status, pool, and live hashrate", "status"),
    Cmd("/config", "Show threads, mode, pool, and worker from the job file", "config"),
    Cmd("/logs", "Tail the last 40 lines of xmrig.log", "logs"),
    Cmd("/err", "Tail the last 40 lines of the error log", "err"),
    Cmd("/open", "Open this folder in Finder", "open"),
    Cmd("/bench", "Offline thread sweep (~10 min). Never starts mining", "bench"),
    Cmd("/flex", "Toggle pool algo switch (off = rx/0 only)", "flex"),
    Cmd("/help", "List commands", "help"),
    Cmd("/quit", "Quit this UI (does not stop a running miner)", "quit"),
)

ALIASES = {
    "/stats": "usage",
    "/plist": "config",
    "/error": "err",
    "/refresh": "status",
    "usage": "usage",
    "logs": "logs",
    "err": "err",
    "plist": "config",
    "help": "help",
    "?": "help",
}

TABS = ("Status", "Config", "Usage", "Logs", "Stats")


_ANSI = re.compile(r"\033\[[0-9;]*m")


def vis_len(s: str) -> int:
    return len(_ANSI.sub("", s or ""))


def trunc(s: Optional[str], n: int) -> str:
    if s is None:
        s = "-"
    else:
        s = str(s)
    if n <= 0:
        return ""
    if vis_len(s) <= n:
        return s
    if n == 1:
        return "~"
    # slice by visible chars, keep no ansi in trunc'd tail
    plain = _ANSI.sub("", s)
    return plain[: n - 1] + "~"


def short_path(path: str, n: int) -> str:
    home = os.path.expanduser("~")
    if path.startswith(home):
        path = "~" + path[len(home) :]
    return trunc(path, n)


def fit_row(left: str, right: str, width: int) -> str:
    """Left text, right-aligned badge, one visible row of `width`."""
    lv, rv = vis_len(left), vis_len(right)
    if rv == 0:
        return left
    gap = width - lv - rv
    if gap < 1:
        keep = max(8, width - rv - 1)
        left = trunc(_ANSI.sub("", left), keep)
        lv = vis_len(left)
        gap = max(1, width - lv - rv)
    return left + " " * gap + right


def parse_job(path: str = PLIST) -> dict:
    out = {
        "threads": "-",
        "mode": "-",
        "init": "-",
        "algo": "rx/0",
        "pool": "-",
        "worker": "-",
        "user": "-",
        "http": "18088",
        "cwd": ROOT,
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
        }
    )
    return out


def api_summary() -> Optional[dict]:
    try:
        with urllib.request.urlopen(API, timeout=1.2) as r:
            return json.load(r)
    except Exception:
        return None


def ctl_status() -> tuple[str, list[str]]:
    try:
        raw = subprocess.check_output([CTL, "status"], text=True, timeout=4)
    except Exception:
        return "STOPPED", []
    lines = [ln.rstrip() for ln in raw.splitlines() if ln.strip()]
    if not lines:
        return "STOPPED", []
    return lines[0].strip(), lines[1:]


def cpu_info() -> tuple[str, str]:
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
    return brand, ram_s


def bar(pct: float, width: int, color: str = BLUE) -> str:
    pct = max(0.0, min(100.0, float(pct)))
    fill = int(round(pct / 100.0 * width))
    fill = max(0, min(width, fill))
    return f"{color}{'█' * fill}{DIM}{'░' * (width - fill)}{RESET}"


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


def read_tail(path: str, n: int = 40) -> list[str]:
    if not os.path.isfile(path):
        return [f"(no file yet) {path}"]
    try:
        with open(path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(0, size - 64_000), os.SEEK_SET)
            data = f.read().decode("utf-8", "replace")
        lines = data.splitlines()[-n:]
        return lines or ["(empty)"]
    except Exception as e:
        return [f"(could not read) {e}"]


@dataclass
class App:
    cols: int = 80
    rows: int = 24
    buf: str = ""
    sel: int = 0
    mode: str = "home"  # home | overlay | confirm
    tab: int = 2  # Usage
    log_which: str = "log"
    body: list[str] = field(default_factory=list)
    confirm_buf: str = ""
    started: float = field(default_factory=time.time)
    last_draw: float = 0.0
    fd: int = 0
    old_tty: Optional[list] = None
    dump: bool = False
    force_palette: Optional[str] = None

    def size(self) -> None:
        try:
            s = shutil.get_terminal_size((80, 24))
            self.cols = max(60, s.columns)
            self.rows = max(18, s.lines)
        except Exception:
            self.cols, self.rows = 80, 24

    def write(self, s: str) -> None:
        sys.stdout.write(s.replace("\n", "\r\n") if not self.dump else s)

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
        state, extra = ctl_status()
        job = parse_job()
        api = api_summary() if state in ("RUNNING", "STARTING") else None
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
        return {
            "state": state,
            "extra": extra,
            "job": job,
            "api": api,
            "hs": hs,
            "highest": highest,
            "acc": acc,
            "rej": rej,
            "up": up,
            "algo": algo,
            "hugepages": hugepages,
        }

    def header_lines(self, live: dict) -> list[str]:
        # Gutter is 4 visible cols ("  ● "), so every text column starts at col 5.
        job = live["job"]
        state = live["state"]
        algo = job.get("algo") or "rx/0"
        threads = job.get("threads") or "-"
        mode = job.get("mode") or "-"
        width = self.cols

        if state == "RUNNING":
            badge = f"{GREEN}RUNNING{RESET}"
            bar = GREEN
        elif state == "STARTING":
            badge = f"{ORANGE}STARTING{RESET}"
            bar = ORANGE
        else:
            badge = f"{RED}STOPPED{RESET}"
            bar = RED

        # 1-space margin so the icon, status bar, and input-box edge share column 2.
        left = f" {ORANGE}●{RESET} {BOLD}XMR Miner{RESET}"
        line1 = fit_row(left, badge, width)

        spec = f"RandomX · {algo} · {threads} threads · {mode}"
        if state == "RUNNING":
            hs = live["hs"]
            hs_s = f"{hs:,.0f} H/s" if hs else "warming up"
            spec += f" · {hs_s}"
            if live["acc"]:
                spec += f" · {live['acc']} accepted"
        elif state == "STARTING":
            spec += " · building dataset"
        text_w = max(20, width - 4)
        path = short_path(ROOT, text_w)
        line2 = f"   {DIM}{trunc(spec, text_w)}{RESET}"
        line3 = f"   {DIM}{path}{RESET}"

        using = f" {bar}│{RESET} Using {algo} (from job file) · /config"
        return [line1, line2, line3, "", using]

    def paint_row(self, text: str, fill: bool = True) -> str:
        # Strip ansi to pad; keep simple: assume caller passes already-sized or we just print.
        return text

    def draw_home(self, live: dict) -> None:
        cols, rows = self.cols, self.rows
        ms = self.matches()
        pal_n = min(len(ms), max(3, rows - 16)) if ms else 0
        prompt_h = 4
        head = self.header_lines(live)
        head_h = len(head) + 1
        body_h = max(1, rows - head_h - pal_n - prompt_h - (1 if pal_n else 0))

        if not self.dump:
            self.write(CLEAR + HIDE)
        for ln in head:
            self.write(ln + "\n")
        self.write("\n")

        body = list(self.body)
        if not body:
            body = [""]
        shown = body[-body_h:]
        while len(shown) < body_h:
            shown.append("")
        for ln in shown:
            self.write(trunc(ln, cols) + "\n")

        if pal_n:
            start = 0
            if len(ms) > pal_n:
                start = min(max(0, self.sel - pal_n + 1), len(ms) - pal_n)
            self.draw_palette(ms, cols, start=start, vis=pal_n)

        shown_buf = self.buf if self.force_palette is None else self.force_palette
        state = live["state"]
        if pal_n:
            footer = f"  {ORANGE}›› tab complete{RESET} {DIM}(enter to run) · esc cancel{RESET}"
        elif state == "RUNNING":
            footer = f"  {GREEN}›› mining on{RESET} {DIM}(t to stop) · /usage{RESET}"
        elif state == "STARTING":
            footer = f"  {ORANGE}›› starting{RESET} {DIM}(t to stop) · wait ~1 min{RESET}"
        else:
            footer = f"  {ORANGE}›› mining off{RESET} {DIM}(s to start) · / for commands{RESET}"
        self.draw_prompt_box(shown_buf, 'try "/usage" or press s to start', footer)

    def draw_prompt_box(self, text: str, placeholder: str, footer: str) -> None:
        """Claude-style rounded input field with placeholder and status line."""
        inner = max(24, self.cols - 3)
        box = DIM
        self.write(f" {box}╭{'─' * inner}╮{RESET}\n")
        self.write(f" {box}│{RESET} {ORANGE}>{RESET} ")
        room = max(1, inner - 3)
        vis = text if len(text) <= room else text[-room:]
        self.write(vis)
        if not self.dump:
            self.write("\033[s")
        extra_n = 0
        if not text and placeholder:
            extra = trunc(placeholder, room)
            extra_n = len(extra)
            self.write(f"{DIM}{extra}{RESET}")
        pad = max(0, inner - 3 - len(vis) - extra_n)
        self.write(" " * pad + f"{box}│{RESET}\n")
        self.write(f" {box}╰{'─' * inner}╯{RESET}\n")
        self.write(footer)
        if not self.dump:
            self.write("\033[u")
            self.write(SHOW)
        else:
            self.write("\n")

    def draw_palette(self, ms: list[Cmd], cols: int, start: int = 0, vis: int = 8) -> None:
        slice_ = ms[start : start + vis]
        name_w = max((len(c.name) for c in slice_), default=8)
        name_w = min(18, max(12, name_w))
        sel = max(0, min(self.sel, len(ms) - 1))
        indent = "   "  # col 4, lines up with ">" inside the input box
        for i, c in enumerate(slice_):
            abs_i = start + i
            desc_w = max(10, cols - len(indent) - name_w - 2)
            desc = trunc(c.desc, desc_w)
            name = c.name.ljust(name_w)
            if abs_i == sel:
                row = f"{indent}{ORANGE}{name}{RESET}  {WHITE}{desc}{RESET}"
            else:
                row = f"{indent}{DIM}{name}{RESET}  {DIM}{desc}{RESET}"
            self.write(row + "\n")

    def kv(self, k: str, v: str, key_w: int = 24) -> str:
        return f"  {DIM}{k:<{key_w}}{RESET}{v}"

    def draw_overlay(self, live: dict) -> None:
        cols, rows = self.cols, self.rows
        tab = TABS[self.tab]
        if not self.dump:
            self.write(CLEAR + HIDE)
        parts = []
        for i, name in enumerate(TABS):
            if i == self.tab:
                parts.append(f"{BOLD}{UNDER}{WHITE}{name}{RESET}")
            else:
                parts.append(f"{DIM}{name}{RESET}")
        self.write("  " + "   ".join(parts) + "\n")
        self.write(f"{BLUE}{'─' * cols}{RESET}\n")
        if tab == "Status":
            lines = self.tab_status(live)
        elif tab == "Config":
            lines = self.tab_config(live)
        elif tab == "Usage":
            lines = self.tab_usage(live)
        elif tab == "Logs":
            lines = self.tab_logs()
        else:
            lines = self.tab_stats(live)
        footer = f"  {DIM}← → tabs · s start · t stop · Esc to cancel{RESET}"
        room = max(3, rows - 4)
        vis = lines[:room]
        while len(vis) < room:
            vis.append("")
        for ln in vis:
            self.write(trunc(ln, cols) + "\n")
        self.write(footer)
        if not self.dump:
            self.write(HIDE)
        else:
            self.write("\n")

    def tab_status(self, live: dict) -> list[str]:
        job = live["job"]
        state = live["state"]
        if state == "RUNNING":
            badge = f"{GREEN}RUNNING{RESET}"
        elif state == "STARTING":
            badge = f"{ORANGE}STARTING{RESET}"
        else:
            badge = f"{RED}STOPPED{RESET}"
        hs = live["hs"]
        hs_s = f"{hs:,.0f} H/s" if hs else ("warming up…" if state != "STOPPED" else "—")
        lines = [
            "",
            f"  {BOLD}Session{RESET}",
            self.kv("Status", badge),
            self.kv("Speed", hs_s),
            self.kv("Highest", f"{live['highest']:,.0f} H/s"),
            self.kv("Shares", f"{live['acc']} accepted  /  {live['rej']} rejected"),
            self.kv("Uptime", fmt_uptime(live["up"]) if live["up"] else "—"),
            self.kv("Algo", live["algo"]),
            self.kv("Pool", job.get("pool_host") or job.get("pool") or "—"),
            self.kv("Worker", job.get("worker") or "—"),
            self.kv("UI open", fmt_uptime(int(time.time() - self.started))),
        ]
        if live["extra"] and state == "RUNNING":
            lines.append("")
            for e in live["extra"]:
                lines.append(f"  {e}")
        return lines

    def tab_config(self, live: dict) -> list[str]:
        j = live["job"]
        flex = "ON" if os.path.isfile(os.path.join(ROOT, "flex.on")) else "OFF"
        return [
            "",
            f"  {BOLD}Job file{RESET}",
            self.kv("algo", j.get("algo") or "—"),
            self.kv("mode", j.get("mode") or "—"),
            self.kv("threads", j.get("threads") or "—"),
            self.kv("init", j.get("init") or "—"),
            self.kv("pool", j.get("pool") or "—"),
            self.kv("worker", j.get("worker") or "—"),
            self.kv("http", f"127.0.0.1:{j.get('http','18088')}"),
            self.kv("donate", j.get("donate") or "0"),
            self.kv("flex", flex),
            self.kv("cwd", trunc(str(j.get("cwd") or ROOT), max(20, self.cols - 28))),
            "",
            f"  {DIM}Edit the plist, then t and s to apply. Start is s only.{RESET}",
        ]

    def tab_usage(self, live: dict) -> list[str]:
        job = live["job"]
        state = live["state"]
        hs = live["hs"] or 0.0
        peak = live["highest"] or PEAK_HS
        pct = (float(hs) / peak * 100.0) if peak and state == "RUNNING" and hs else 0.0
        acc, rej = live["acc"], live["rej"]
        total_sh = acc + rej
        share_pct = (acc / total_sh * 100.0) if total_sh else (0.0 if state == "STOPPED" else 100.0)
        bw = max(20, min(48, self.cols - 22))
        hs_s = f"{hs:,.0f} H/s" if hs else ("warming up…" if state != "STOPPED" else "—")
        brand, ram = cpu_info()

        if state == "STOPPED":
            insight = "Miner is off. Hashrate is 0 until you type s."
            insight_note = "Start is s only. Slash commands never start mining."
        elif state == "STARTING":
            insight = "RandomX is building the 2 GB dataset."
            insight_note = "Full speed in about a minute. Do not start a second miner."
        elif pct >= 80:
            insight = f"{pct:.0f}% of peak for this M4 at {job.get('threads','-')} threads."
            insight_note = "Near the offline sweep. If H/s drops, check compressor / a second miner."
        else:
            insight = f"{pct:.0f}% of peak · still warming or cores are busy."
            insight_note = "Longer dataset init is more expensive even when cached. Wait, then /usage."

        contrib = []
        if state == "RUNNING" and hs:
            contrib = [
                self.kv("CPU threads", f"100% of hashrate · {job.get('threads','-')} threads", 22),
                self.kv("Mode", f"{job.get('mode','-')}  (2 GB dataset)", 22),
                self.kv("Pool", job.get("pool_host") or "—", 22),
            ]
        else:
            contrib = [
                self.kv("CPU threads", "0% · miner is not hashing", 22),
                self.kv("Mode", f"{job.get('mode','-')}  (idle)", 22),
                self.kv("Pool", job.get("pool_host") or "—", 22),
            ]

        return [
            "",
            f"  {BOLD}Session{RESET}",
            self.kv("Status", state),
            self.kv("Speed", hs_s),
            self.kv("Shares", f"{acc} accepted  /  {rej} rejected"),
            self.kv("Uptime", fmt_uptime(live["up"]) if live["up"] else "—"),
            self.kv("UI duration (wall)", fmt_uptime(int(time.time() - self.started))),
            self.kv("Algo", live["algo"]),
            "",
            f"  {BOLD}Current session{RESET}",
            f"  {bar(pct, bw)}  {pct:.0f}% of peak",
            f"  {DIM}Peak {peak:,.0f} H/s from offline sweep · {job.get('threads','-')} threads · {job.get('mode','-')}{RESET}",
            "",
            f"  {BOLD}Share quality{RESET}",
            f"  {bar(share_pct, bw, GREEN if share_pct >= 95 or total_sh == 0 else ORANGE)}  {share_pct:.0f}% accepted",
            f"  {DIM}{acc} good · {rej} rejected{RESET}",
            "",
            f"  {BOLD}What's contributing to your hashrate?{RESET}",
            f"  {DIM}Approximate, based on this machine — does not include other devices{RESET}",
            "",
            f"  {insight}",
            f"  {DIM}{insight_note}{RESET}",
            "",
            *contrib,
            "",
            f"  {DIM}{trunc(brand, self.cols - 4)} · {ram}{RESET}",
        ]

    def tab_logs(self) -> list[str]:
        path = ERR if self.log_which == "err" else LOG
        label = "xmrig.err.log" if self.log_which == "err" else "xmrig.log"
        lines = [f"  {BOLD}{label}{RESET}  {DIM}(last 40 · e toggles error log){RESET}", ""]
        for ln in read_tail(path, 40):
            lines.append("  " + ln)
        return lines

    def tab_stats(self, live: dict) -> list[str]:
        api = live["api"] or {}
        hs = api.get("hashrate") or {}
        tot = hs.get("total") or [None, None, None]
        def f(i):
            try:
                v = tot[i]
                return f"{v:,.0f} H/s" if v is not None else "—"
            except Exception:
                return "—"
        cpu = (api.get("cpu") or {}).get("brand") or cpu_info()[0]
        return [
            "",
            f"  {BOLD}Hashrate{RESET}",
            self.kv("10s", f(0)),
            self.kv("60s", f(1) if len(tot) > 1 else "—"),
            self.kv("15m", f(2) if len(tot) > 2 else "—"),
            self.kv("highest", f"{live['highest']:,.0f} H/s"),
            "",
            f"  {BOLD}Connection{RESET}",
            self.kv("pool", (api.get("connection") or {}).get("pool") or live["job"].get("pool") or "—"),
            self.kv("ping", str((api.get("connection") or {}).get("ping") or "—")),
            self.kv("failures", str((api.get("connection") or {}).get("failures") or "—")),
            "",
            f"  {BOLD}Machine{RESET}",
            self.kv("cpu", trunc(str(cpu), max(18, self.cols - 28))),
            self.kv("hugepages", {True: "yes", False: "no"}.get(live["hugepages"], "—")),
            self.kv("version", str(api.get("version") or "—")),
            self.kv("worker", str(api.get("worker_id") or live["job"].get("worker") or "—")),
        ]

    def draw_confirm(self) -> None:
        if not self.dump:
            self.write(CLEAR + HIDE)
        self.write(f"\n  {BOLD}Offline sweep{RESET}\n")
        self.write(f"  {DIM}~10 minutes. Mines nothing. Refuses if the miner is running.{RESET}\n\n")
        self.draw_prompt_box(
            self.confirm_buf,
            "yes",
            f"  {ORANGE}›› type yes to run{RESET} {DIM}· esc cancel{RESET}",
        )

    def draw(self) -> None:
        self.size()
        live = self.live()
        if self.mode == "overlay":
            self.draw_overlay(live)
        elif self.mode == "confirm":
            self.draw_confirm()
        else:
            self.draw_home(live)
        sys.stdout.flush()
        self.last_draw = time.time()

    def open_overlay(self, tab_name: str, log_which: str = "log") -> None:
        if tab_name in TABS:
            self.tab = TABS.index(tab_name)
        self.log_which = log_which
        self.mode = "overlay"
        self.buf = ""
        self.sel = 0

    def set_body(self, lines: list[str]) -> None:
        self.body = [""] + lines + [""]

    def do_start(self) -> None:
        try:
            out = subprocess.check_output([CTL, "start"], text=True, timeout=8)
        except subprocess.CalledProcessError as e:
            out = e.output or "start failed"
        except Exception as e:
            out = str(e)
        self.set_body([ln for ln in out.splitlines() if ln] or ["Started."])
        self.mode = "home"

    def do_stop(self) -> None:
        try:
            out = subprocess.check_output([CTL, "stop"], text=True, timeout=8)
        except Exception as e:
            out = str(e)
        self.set_body([ln for ln in out.splitlines() if ln] or ["Stopped."])
        self.mode = "home"

    def do_open(self) -> None:
        subprocess.Popen(["open", ROOT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.set_body(["opened Finder."])
        self.mode = "home"

    def do_flex(self) -> None:
        path = os.path.join(ROOT, "flex.on")
        if os.path.isfile(path):
            os.remove(path)
            self.set_body(["flex OFF. next s is rx/0 only."])
        else:
            open(path, "w").close()
            self.set_body(["flex ON. next s lets the pool pick the algo (not always XMR).", "stop with t first if it is running, then s."])
        self.mode = "home"

    def do_bench(self) -> None:
        if self.live()["state"] != "STOPPED":
            self.set_body(["miner is running. press t, then /bench again."])
            self.mode = "home"
            self.confirm_buf = ""
            return
        script = os.path.join(ROOT, "bin", "xmr_bench_sweep.sh")
        self.restore_tty()
        self.write(SHOW + RESET + "\n")
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
        self.set_body(["bench finished. /usage for stats."])

    def run_action(self, action: str) -> bool:
        """Return False to quit."""
        if action == "quit":
            return False
        if action == "usage":
            self.open_overlay("Usage")
        elif action == "status":
            self.open_overlay("Status")
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
            self.mode = "confirm"
            self.confirm_buf = ""
        elif action == "help":
            self.buf = "/"
            self.sel = 0
            self.mode = "home"
        else:
            self.set_body(["unknown. /help"])
        return True

    def take_tty(self) -> None:
        self.fd = sys.stdin.fileno()
        self.old_tty = termios.tcgetattr(self.fd)
        tty.setraw(self.fd)

    def restore_tty(self) -> None:
        if self.old_tty is not None:
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_tty)

    def read_key(self) -> Optional[str]:
        r, _, _ = select.select([self.fd], [], [], 0.45)
        if not r:
            return None
        b = os.read(self.fd, 1)
        if not b:
            return "quit"
        if b == b"\x03":
            return "quit"
        if b == b"\x04":
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
        if ch.isprintable():
            return ch
        return None

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
            if key in ("r", "R", "enter"):
                self.body = []
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
                self.body = []
                return True
            if action:
                return self.run_action(action)
            self.set_body(["unknown. type / for commands"])
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
        if key in "12345":
            self.tab = int(key) - 1
            return True
        return True

    def on_key_confirm(self, key: str) -> bool:
        if key in ("quit", "esc"):
            self.mode = "home"
            self.confirm_buf = ""
            self.set_body(["cancelled."])
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
                self.set_body(["cancelled."])
            return True
        if key and len(key) == 1:
            self.confirm_buf += key
        return True

    def loop(self) -> None:
        self.take_tty()
        try:
            self.draw()
            while True:
                key = self.read_key()
                if key is None:
                    # live refresh while running / starting / overlay
                    if time.time() - self.last_draw >= 1.0:
                        self.draw()
                    continue
                ok = True
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
            sys.stdout.write(SHOW + RESET + "\n")
            sys.stdout.flush()


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
    ms = filter_cmds("/usage")
    check("/usage exact first", ms and ms[0].name == "/usage")
    ms = filter_cmds("/xyznope")
    check("unknown empty", ms == [])
    check("alias /stats", resolve_action("/stats", None) == "usage")
    check("alias /plist", resolve_action("/plist", None) == "config")
    job = parse_job()
    check("plist threads", job.get("threads") not in ("", None))
    check("plist pool", "monero" in str(job.get("pool") or "").lower() or job.get("pool") != "-")
    b = bar(33, 10)
    check("bar has fill", "█" in b and "░" in b)
    check("uptime", fmt_uptime(125) == "2m 5s")
    import io
    from contextlib import redirect_stdout
    buf = io.StringIO()
    with redirect_stdout(buf):
        dump_frame("home")
    home = buf.getvalue()
    check("input box top", "╭" in home and "╮" in home)
    check("input box bottom", "╰" in home and "╯" in home)
    check("placeholder", 'try "/usage"' in home)
    check("status chevrons", "››" in home)
    plain = _ANSI.sub("", home)
    title = next((ln for ln in plain.splitlines() if "XMR Miner" in ln), "")
    spec = next((ln for ln in plain.splitlines() if "RandomX" in ln), "")
    using = next((ln for ln in plain.splitlines() if "Using rx/0" in ln), "")
    check("title has badge", title.startswith(" ● XMR Miner") and title.rstrip().endswith("STOPPED"))
    check("spec indent", spec.startswith("   RandomX"))
    check("using bar col", using.startswith(" │ Using") or using.startswith(" │ Using"))
    tcol = title.index("X")
    scol = spec.index("R")
    ucol = using.index("U")
    check("text column aligned", tcol == scol == ucol)
    row80 = fit_row(" ● XMR Miner", "STOPPED", 80)
    check("badge row 80", vis_len(row80) == 80 and row80.endswith("STOPPED"))
    row40 = fit_row(" ● XMR Miner", "STOPPED", 24)
    check("badge row narrow", vis_len(row40) == 24 and row40.endswith("STOPPED"))
    print("self-test", "passed" if fails == 0 else f"{fails} failed")
    return 0 if fails == 0 else 1


def dump_frame(kind: str) -> None:
    app = App(dump=True, cols=100, rows=32)
    os.environ.setdefault("TERM", "xterm-256color")
    app.size = lambda: None  # type: ignore
    app.cols, app.rows = 100, 32
    if kind == "palette":
        app.force_palette = "/usage"
        app.sel = 0
        app.mode = "home"
        app.draw_home(app.live())
    elif kind in ("slash", "/"):
        app.force_palette = "/"
        app.sel = 0
        app.mode = "home"
        app.draw_home(app.live())
    elif kind == "usage":
        app.mode = "overlay"
        app.tab = TABS.index("Usage")
        app.draw_overlay(app.live())
    else:
        app.mode = "home"
        app.draw_home(app.live())
    sys.stdout.write("\n")


def main() -> int:
    os.chdir(ROOT)
    args = sys.argv[1:]
    if args and args[0] in ("--self-test", "self-test"):
        return self_test()
    if args and args[0] in ("--dump", "dump"):
        dump_frame(args[1] if len(args) > 1 else "home")
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
