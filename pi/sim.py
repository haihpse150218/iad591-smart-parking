"""Gia lap Arduino de test dashboard tren laptop khi chua co mach.

FakeArduino co cung giao dien voi serial.Serial (readline / write / close)
va chay cung logic voi smart_parking.ino: xe den ngau nhien, LDR theo chu ky ngay/dem.
"""
import json
import math
import random
import time

CAPACITY = 4
DARK_LDR = 300


class FakeArduino:
    def __init__(self, path="sim"):
        self.cars, self.auto, self.gate, self.light_manual, self.light = 0, True, 0, False, 0
        self.gate_close_at = 0
        self.out = [json.dumps({"ev": "boot"})]
        self.next_report = 0
        self.t0 = time.time()

    # --- giao dien giong pyserial ---
    def write(self, data):
        for c in data.decode().strip().upper().splitlines():
            print(f"[sim] nhan lenh: {c}", flush=True)
            if c == "GATE:OPEN":
                self._gate(1); self._ev("gate_manual_open")
            elif c == "GATE:CLOSE":
                self._gate(0); self._ev("gate_manual_close")
            elif c == "LIGHT:ON":
                self.light_manual, self.light = True, 1
            elif c == "LIGHT:OFF":
                self.light_manual, self.light = True, 0
            elif c == "LIGHT:AUTO":
                self.light_manual = False
            elif c == "MODE:AUTO":
                self.auto, self.light_manual = True, False
            elif c == "MODE:MANUAL":
                self.auto = False
            elif c.startswith("CARS:"):
                self.cars = max(0, min(CAPACITY, int(c[5:] or 0))); self._ev("cars_set")
            elif c == "BUZZ":
                pass
            self._report()

    def readline(self):
        if not self.out:
            time.sleep(0.2)
            self._tick()
        return (self.out.pop(0) + "\n").encode() if self.out else b""

    def close(self):
        pass

    # --- logic ---
    def _ev(self, name):
        self.out.append(json.dumps({"ev": name, "cars": self.cars}))

    def _gate(self, v):
        self.gate = v
        if v:
            self.gate_close_at = time.time() + 3

    def _tick(self):
        now = time.time()
        self.d1, self.d2 = random.randint(30, 200), random.randint(30, 200)
        if random.random() < 0.04:                       # xe den cong vao
            self.d1 = 5
            if self.cars < CAPACITY:
                if self.auto:
                    self._gate(1); self.cars += 1; self._ev("car_in")
                else:
                    self._ev("car_wait")
            else:
                self._ev("full")
        elif random.random() < 0.03 and self.cars > 0:   # xe ra
            self.d2 = 6
            self.cars -= 1; self._ev("car_out")
            if self.auto:
                self._gate(1)
        if self.gate and self.auto and now >= self.gate_close_at:
            self.gate = 0
        # LDR: chu ky ngay/dem 2 phut
        self.ldr = int(520 + 450 * math.sin((now - self.t0) / 120 * 2 * math.pi) + random.randint(-15, 15))
        if not self.light_manual:
            self.light = int(self.ldr < DARK_LDR)
        if now >= self.next_report:
            self.next_report = now + 1
            self._report()

    def _report(self):
        self.out.append(json.dumps({
            "d1": getattr(self, "d1", 400), "d2": getattr(self, "d2", 400), "ldr": getattr(self, "ldr", 500),
            "cars": self.cars, "cap": CAPACITY, "gate": self.gate, "light": self.light,
            "mode": "AUTO" if self.auto else "MANUAL"}, separators=(",", ":")))
