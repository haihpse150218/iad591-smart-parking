"""SQLite cho Smart Parking (dung chung giua thread doc serial va Flask)."""
import os
import sqlite3
import threading
from datetime import datetime, timedelta

HERE = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.environ.get("PARKING_DB", os.path.join(HERE, "data", "parking.db"))

_lock = threading.Lock()
_conn = None


def now_iso():
    return datetime.now().isoformat(timespec="seconds")


def connect(path=None):
    global _conn
    path = path or DB_PATH
    if path != ":memory:":
        os.makedirs(os.path.dirname(path), exist_ok=True)
    _conn = sqlite3.connect(path, check_same_thread=False)
    _conn.row_factory = sqlite3.Row
    with open(os.path.join(HERE, "schema.sql"), encoding="utf-8") as f:
        _conn.executescript(f.read())
    return _conn


def _exec(sql, args=()):
    with _lock:
        _conn.execute(sql, args)
        _conn.commit()


def _query(sql, args=()):
    with _lock:
        return [dict(r) for r in _conn.execute(sql, args).fetchall()]


def add_reading(r, link):
    _exec(
        "INSERT INTO readings(ts,dist_in,dist_out,ldr,cars,capacity,gate,light,mode,link)"
        " VALUES (?,?,?,?,?,?,?,?,?,?)",
        (now_iso(), r.get("d1"), r.get("d2"), r.get("ldr"), r.get("cars"), r.get("cap"),
         r.get("gate"), r.get("light"), r.get("mode"), link),
    )


def add_event(ev_type, detail=""):
    _exec("INSERT INTO events(ts,type,detail) VALUES (?,?,?)", (now_iso(), ev_type, str(detail)))


def add_command(cmd, source="web"):
    _exec("INSERT INTO commands(ts,cmd,source) VALUES (?,?,?)", (now_iso(), cmd, source))


def history(minutes=30, max_points=600):
    since = (datetime.now() - timedelta(minutes=minutes)).isoformat(timespec="seconds")
    rows = _query("SELECT * FROM readings WHERE ts >= ? ORDER BY id", (since,))
    step = max(1, len(rows) // max_points)     # giam mau neu qua nhieu diem
    return rows[::step]


def recent_events(limit=20):
    return _query("SELECT * FROM events ORDER BY id DESC LIMIT ?", (limit,))


def stats_today():
    day = datetime.now().strftime("%Y-%m-%d")
    rows = _query(
        "SELECT type, COUNT(*) AS n FROM events WHERE ts >= ? GROUP BY type", (day,))
    return {r["type"]: r["n"] for r in rows}


def all_readings():
    return _query("SELECT * FROM readings ORDER BY id")


def reset():
    """Xoa toan bo lich su (nut 'Reset du lieu' tren dashboard)."""
    with _lock:
        _conn.executescript("DELETE FROM readings; DELETE FROM events; DELETE FROM commands;")
        _conn.commit()
