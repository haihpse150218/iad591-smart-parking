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

    def handle(self, msg):
        if "ev" in msg:
            if msg["ev"] == "boot" and self.state:
                # Mo /dev/ttyACM0 lam Arduino reset -> khoi phuc so xe va che do
                self.cmds.put(f"CARS:{self.state.get('cars', 0)}")
                self.cmds.put(f"MODE:{self.state.get('mode', 'AUTO')}")
            if msg["ev"] not in ("pong", "servo_off"):   # servo_off chi de debug tren Serial Monitor
                db.add_event(msg["ev"], msg.get("cars", msg.get("ang", "")))
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
                while not self.cmds.empty():
                    self._dev.write((self.cmds.get() + "\n").encode())
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
