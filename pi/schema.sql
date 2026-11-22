-- Smart Parking - SQLite schema
CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts          TEXT    NOT NULL,              -- ISO 8601, gio Pi
    dist_in     INTEGER,                       -- cm, HC-SR04 cong vao
    dist_out    INTEGER,                       -- cm, HC-SR04 cong ra (-1 neu khong lap)
    ldr         INTEGER,                       -- 0..1023
    cars        INTEGER,
    capacity    INTEGER,
    gate        INTEGER,                       -- 1 mo, 0 dong
    light       INTEGER,                       -- 1 bat, 0 tat
    mode        TEXT,                          -- AUTO / MANUAL
    link        TEXT                           -- bt / serial / sim
);
CREATE INDEX IF NOT EXISTS idx_readings_ts ON readings(ts);

CREATE TABLE IF NOT EXISTS events (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    ts      TEXT NOT NULL,
    type    TEXT NOT NULL,                     -- car_in, car_out, full, gate_manual_open, link_switch...
    detail  TEXT
);

CREATE TABLE IF NOT EXISTS commands (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    ts      TEXT NOT NULL,
    cmd     TEXT NOT NULL,                     -- GATE:OPEN, LIGHT:ON, ...
    source  TEXT                               -- web / auto
);
