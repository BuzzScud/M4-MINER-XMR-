#!/usr/bin/env python3
"""Write a small Desktop txt summary of the last miner session. Used on stop/kill."""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime
from typing import Optional

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SNAP = os.path.join(ROOT, "logs", "last-session.json")
DESKTOP = os.path.expanduser("~/Desktop")


def fmt_hs(v) -> str:
    try:
        if v is None:
            return "—"
        return f"{float(v):,.0f} H/s"
    except (TypeError, ValueError):
        return "—"


def fmt_up(seconds) -> str:
    try:
        seconds = max(0, int(seconds))
    except (TypeError, ValueError):
        return "—"
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h {m}m"
    if m:
        return f"{m}m {s}s"
    return f"{s}s"


def fmt_hp(val) -> str:
    if val is True:
        return "yes"
    if val is False:
        return "no"
    if isinstance(val, (list, tuple)) and len(val) >= 2:
        try:
            return f"{int(val[0])}/{int(val[1])}"
        except (TypeError, ValueError):
            return "—"
    if val is None:
        return "—"
    return str(val)


def snapshot_from_api(api: dict) -> dict:
    hs = api.get("hashrate") or {}
    tot = hs.get("total") or []
    conn = api.get("connection") or {}
    res = api.get("results") or {}
    acc = int(conn.get("accepted") or res.get("shares_good") or 0)
    rej = int(conn.get("rejected") or 0)
    return {
        "saved_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "state": "RUNNING",
        "hs": tot[0] if tot else None,
        "hs10": tot[0] if len(tot) > 0 else None,
        "hs60": tot[1] if len(tot) > 1 else None,
        "hs15": tot[2] if len(tot) > 2 else None,
        "highest": hs.get("highest"),
        "acc": acc,
        "rej": rej,
        "up": api.get("uptime") or conn.get("uptime") or 0,
        "algo": api.get("algo") or "rx/0",
        "pool": conn.get("pool") or "—",
        "worker": api.get("worker_id") or "—",
        "ping": conn.get("ping"),
        "failures": conn.get("failures"),
        "version": api.get("version"),
        "threads": None,
        "mode": None,
        "hugepages": api.get("hugepages"),
    }


def load_json(path: str) -> Optional[dict]:
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return None


def load_snapshot(api_raw: Optional[str] = None, snap_path: str = SNAP) -> dict:
    if api_raw and api_raw.strip():
        try:
            data = json.loads(api_raw)
            if isinstance(data, dict) and "hashrate" in data:
                snap = snapshot_from_api(data)
                file_snap = load_json(snap_path) or {}
                for k in ("threads", "mode", "worker"):
                    if not snap.get(k) and file_snap.get(k):
                        snap[k] = file_snap[k]
                if file_snap.get("worker") and (not snap.get("worker") or snap.get("worker") == "—"):
                    snap["worker"] = file_snap["worker"]
                return snap
            if isinstance(data, dict) and "acc" in data:
                return data
        except Exception:
            pass
    return load_json(snap_path) or {}


def render(snap: dict, reason: str, stopped: Optional[str] = None) -> str:
    stopped = stopped or datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    acc = int(snap.get("acc") or 0)
    rej = int(snap.get("rej") or 0)
    total = acc + rej
    quality = f"{(acc / total * 100.0):.0f}% accepted" if total else "—"
    ping = snap.get("ping")
    ping_s = f"{int(ping)} ms" if ping not in (None, "") else "—"
    fail = snap.get("failures")
    fail_s = str(fail) if fail not in (None, "") else "—"
    lines = [
        "XMR miner session",
        f"Stopped:  {stopped}",
        f"Reason:   {reason}",
        "",
        f"Status:     {snap.get('state') or 'unknown'} (last sample)",
        f"Algo:       {snap.get('algo') or '—'}",
        f"Pool:       {snap.get('pool') or '—'}",
        f"Worker:     {snap.get('worker') or '—'}",
        "",
        f"Uptime:     {fmt_up(snap.get('up'))}",
        f"Speed 10s:  {fmt_hs(snap.get('hs10') if snap.get('hs10') is not None else snap.get('hs'))}",
        f"Speed 60s:  {fmt_hs(snap.get('hs60'))}",
        f"Speed 15m:  {fmt_hs(snap.get('hs15'))}",
        f"Highest:    {fmt_hs(snap.get('highest'))}",
        "",
        f"Shares:     {acc} accepted / {rej} rejected",
        f"Quality:    {quality}",
        "",
        f"Threads:    {snap.get('threads') or '—'}",
        f"Mode:       {snap.get('mode') or '—'}",
        f"Version:    {snap.get('version') or '—'}",
        f"Hugepages:  {fmt_hp(snap.get('hugepages'))}",
        f"Ping:       {ping_s}   failures {fail_s}",
    ]
    if snap.get("saved_at"):
        lines.append(f"Sampled:    {snap['saved_at']}")
    return "\n".join(lines) + "\n"


def write_desktop(text: str, desktop: str = DESKTOP) -> str:
    os.makedirs(desktop, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    path = os.path.join(desktop, f"XMR-miner-summary-{ts}.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    return path


def main(argv: Optional[list[str]] = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reason", default="stop", help="stop or kill")
    p.add_argument("--api-stdin", action="store_true", help="read XMRig /2/summary JSON from stdin")
    p.add_argument("--snapshot", default=SNAP)
    p.add_argument("--desktop", default=DESKTOP)
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--stdout-only", action="store_true", help="print summary, do not write a file")
    args = p.parse_args(argv)

    if args.self_test:
        snap = {
            "state": "RUNNING",
            "hs": 4178,
            "hs10": 4178,
            "hs60": 4062,
            "hs15": 4166,
            "highest": 4501,
            "acc": 1945,
            "rej": 0,
            "up": 58080,
            "algo": "rx/0",
            "pool": "gulf.moneroocean.stream:20016",
            "worker": "minerv3-m4-16gb",
            "ping": 143,
            "failures": 1,
            "version": "6.26.0",
            "threads": "10",
            "mode": "fast",
            "hugepages": [0, 1178],
            "saved_at": "2026-09-12 12:32:00",
        }
        text = render(snap, "stop", "2026-09-12 12:34:00")
        ok = all(
            s in text
            for s in ("1945 accepted", "16h 8m", "4,178 H/s", "minerv3-m4-16gb", "0/1178")
        )
        print("ok" if ok else "FAIL")
        if not ok:
            print(text)
        return 0 if ok else 1

    raw = sys.stdin.read() if args.api_stdin else None
    snap = load_snapshot(raw, args.snapshot)
    text = render(snap, args.reason)
    if args.stdout_only:
        sys.stdout.write(text)
        return 0
    path = write_desktop(text, args.desktop)
    print(f"Desktop summary: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
