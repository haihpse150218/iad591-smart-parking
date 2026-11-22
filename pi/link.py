"""Ket noi Arduino: uu tien Bluetooth (/dev/rfcomm0), mat ket noi thi tu chuyen sang USB Serial.

Moi dong Arduino gui la 1 JSON:
  {"d1":23,"d2":400,"ldr":512,"cars":2,"cap":4,"gate":0,"light":1,"mode":"AUTO"}   (trang thai, 1 s/lan)
  {"ev":"car_in","cars":3}                                                          (su kien)
"""
import json
import os
import queue
import threading
import time

import db

PORTS = [
    ("bt", os.environ.get("BT_PORT", "/dev/rfcomm0")),
    ("serial", os.environ.get("USB_PORT", "/dev/ttyACM0")),
    ("serial", "/dev/ttyUSB0"),
]
BAUD = 9600
SILENCE_S = 5          # khong nhan gi trong 5 s -> coi nhu mat ket noi, thu cong khac
ACK_TIMEOUT_S = 1.2    # gui lenh ma khong thay {"ev":"ack"} trong 1.2 s -> gui lai
MAX_TRIES = 3          # SoftwareSerial (HC-05) mat byte khi Arduino dang gui -> can gui lai


def parse(line):
    """Dong JSON -> dict; dong hong / rac -> None."""
    line = line.strip()
    if not line.startswith("{"):
        return None
    try:
        msg = json.loads(line)
    except ValueError:
        return None
    return msg if isinstance(msg, dict) else None


class DeviceLink(threading.Thread):
    def __init__(self, open_port=None, ports=None):
        super().__init__(daemon=True)
        self.ports = ports or PORTS
        self.open_port = open_port or self._open_serial
        self.state = {}             # trang thai moi nhat
        self.link = "none"          # bt / serial / sim / none
        self.last_rx = 0.0
        self.cmds = queue.Queue()
        self._dev = None
        self._idx = 0               # cong dang dung / cong se thu dau tien
        self._switch = False
        self._pending = None        # [lenh, so lan da gui, thoi diem gui] - chi 1 lenh tren duong truyen

    @staticmethod
    def _open_serial(path):
        import serial               # pyserial, chi can tren Pi
        return serial.Serial(path, BAUD, timeout=1)

    # ---------- API cho Flask ----------
    def send(self, cmd, source="web"):
        self.cmds.put(cmd)
        db.add_command(cmd, source)

    def snapshot(self):
        online = time.time() - self.last_rx < SILENCE_S
        return {**self.state, "link": self.link if online else "offline", "online": online}

    # ---------- vong lap ----------
    def _connect(self):
        n = len(self.ports)
        for k in range(n):
            i = (self._idx + k) % n
            name, path = self.ports[i]
            try:
                dev = self.open_port(path)
            except Exception as e:  # noqa: BLE001 - cong khong ton tai / dang ban
                print(f"[link] {path}: {e}", flush=True)
                continue
            old = self.link
            self._idx = i
            self._dev, self.link, self.last_rx = dev, name, time.time()
            print(f"[link] connected {name} {path}", flush=True)
            db.add_event("link_switch", f"{old} -> {name} ({path})")
            return True
        return False

    def _close(self):
        try:
            self._dev.close()
        except Exception:  # noqa: BLE001
            pass
        self._dev = None

    def prefer(self, name):
        """Ep chuyen kenh tu dashboard: 'bt' hoac 'serial'."""
        for i, (n, _) in enumerate(self.ports):
            if n == name:
                self._idx = i
                self._switch = True
                return True
        return False

    def _pump_cmds(self):
        """Gui tung lenh mot, cho ack; het han thi gui lai, qua MAX_TRIES thi bo."""
        now = time.time()
        p = self._pending
        if p is not None and now - p[2] > ACK_TIMEOUT_S:
            if p[1] >= MAX_TRIES:
                print(f"[link] no ack for {p[0]} after {p[1]} tries", flush=True)
                db.add_event("cmd_fail", p[0])
                self._pending = p = None
            else:
                self._write(p)
        if p is None and not self.cmds.empty():
            self._pending = [self.cmds.get(), 0, 0.0]
            self._write(self._pending)

    def _write(self, p):
        self._dev.write((p[0] + "\n").encode())
        p[1] += 1
        p[2] = time.time()

    def handle(self, msg):
        if "ev" in msg:
            if msg["ev"] == "ack":
                if self._pending and str(msg.get("c", "")).upper() == self._pending[0].upper():
                    self._pending = None
                return
            if msg["ev"] == "bad_cmd" and self._pending:
                self._pending[2] = 0.0          # lenh vua gui bi hong -> gui lai ngay
            if msg["ev"] == "boot" and self.state:
                # Mo /dev/ttyACM0 lam Arduino reset -> khoi phuc so xe va che do
                self.cmds.put(f"CARS:{self.state.get('cars', 0)}")
                self.cmds.put(f"MODE:{self.state.get('mode', 'AUTO')}")
            if msg["ev"] not in ("pong", "servo_off"):   # servo_off chi de debug tren Serial Monitor
                db.add_event(msg["ev"], msg.get("cars", msg.get("ang", msg.get("raw", ""))))
        else:
            self.state = msg
            db.add_reading(msg, self.link)

    def run(self):
        while True:
            if self._dev is None and not self._connect():
                time.sleep(2)
                continue
            if self._switch:
                self._switch = False
                self._close()
                continue
            try:
                self._pump_cmds()
                raw = self._dev.readline()
                if raw:
                    msg = parse(raw.decode(errors="ignore"))
                    if msg is not None:
                        self.last_rx = time.time()
                        self.handle(msg)
                if time.time() - self.last_rx > SILENCE_S:
                    print(f"[link] {self.link} silent > {SILENCE_S}s, reconnecting", flush=True)
                    self._close()
                    self._idx += 1      # lan sau thu cong ke tiep (BT -> Serial)
            except Exception as e:  # noqa: BLE001 - rut day, BT rot
                print(f"[link] error on {self.link}: {e}", flush=True)
                self._close()
                self._idx += 1
                time.sleep(1)
