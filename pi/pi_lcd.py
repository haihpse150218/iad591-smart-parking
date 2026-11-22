"""LCD1602 I2C (PCF8574) gan TRUC TIEP vao Raspberry Pi - thay cho LED 4 so trong rubric.

Day noi (Pi 3B+):
  LCD GND -> chan 6 (GND)      LCD VCC -> chan 2 (5V)
  LCD SDA -> chan 3 (GPIO2)    LCD SCL -> chan 5 (GPIO3)
Bat I2C: sudo raspi-config -> Interface Options -> I2C -> Enable
Kiem tra dia chi: i2cdetect -y 1   (thuong 27 hoac 3f)

Hien thi (cap nhat 1 s/lan):
  dong 1:  Trong 3/4    BT
  dong 2:  V:12 R:9  14:05
"""
import threading
import time
from datetime import datetime

# Chan PCF8574 -> HD44780: P0=RS, P1=RW, P2=E, P3=den nen, P4..P7=D4..D7
RS, EN, BACKLIGHT = 0x01, 0x04, 0x08


class PiLcd:
    def __init__(self, addr=0x27, busnum=1, bus=None):
        if bus is None:
            try:
                from smbus2 import SMBus
            except ImportError:
                from smbus import SMBus      # python3-smbus co san tren Raspberry Pi OS
            bus = SMBus(busnum)
        self.bus, self.addr = bus, addr
        for nib in (0x30, 0x30, 0x30, 0x20):  # vao che do 4 bit
            self._pulse(nib)
            time.sleep(0.005)
        for cmd in (0x28, 0x0C, 0x06, 0x01):  # 2 dong 5x8, bat man hinh, tu tang con tro, xoa
            self._send(cmd, 0)
        time.sleep(0.002)

    def _write(self, b):
        self.bus.write_byte(self.addr, b | BACKLIGHT)

    def _pulse(self, b):
        self._write(b | EN)
        time.sleep(0.0005)
        self._write(b & ~EN)
        time.sleep(0.0001)

    def _send(self, val, mode):
        self._pulse((val & 0xF0) | mode)
        self._pulse(((val << 4) & 0xF0) | mode)

    def line(self, row, text):
        self._send(0x80 | (0x40 if row else 0x00), 0)
        for ch in text[:16].ljust(16):
            self._send(ord(ch) if ord(ch) < 128 else ord("?"), RS)


def render(snap, today, now=None):
    """Trang thai -> 2 dong 16 ky tu (khong dau, LCD khong co tieng Viet)."""
    now = now or datetime.now()
    if not snap.get("online") or "cars" not in snap:
        l1 = "Mat ket noi"
    else:
        free = snap.get("cap", 4) - snap.get("cars", 0)
        link = {"bt": "BT", "serial": "USB", "sim": "SIM"}.get(snap.get("link"), "--")
        l1 = (f"Trong {free}/{snap.get('cap', 4)}" if free > 0 else "BAI DAY  0/" + str(snap.get("cap", 4))).ljust(13) + link
    l2 = f"V:{today.get('car_in', 0)} R:{today.get('car_out', 0)}".ljust(11) + now.strftime("%H:%M")
    return l1[:16], l2[:16]


class LcdUpdater(threading.Thread):
    def __init__(self, device, stats_fn, lcd):
        super().__init__(daemon=True)
        self.device, self.stats_fn, self.lcd = device, stats_fn, lcd

    def run(self):
        last = None
        while True:
            try:
                lines = render(self.device.snapshot(), self.stats_fn())
                if lines != last:
                    self.lcd.line(0, lines[0])
                    self.lcd.line(1, lines[1])
                    last = lines
            except Exception as e:  # noqa: BLE001 - long day I2C thi thu lai, khong lam sap app
                print(f"[lcd] {e}", flush=True)
                last = None
                time.sleep(2)
            time.sleep(1)


def start(device, stats_fn, addr):
    """Khoi dong LCD neu co; khong co LCD thi bo qua (app van chay)."""
    try:
        lcd = PiLcd(addr)
    except Exception as e:  # noqa: BLE001
        print(f"[lcd] khong dung LCD tren Pi ({e})", flush=True)
        return None
    LcdUpdater(device, stats_fn, lcd).start()
    print(f"[lcd] LCD1602 tren Pi o dia chi 0x{addr:02x}", flush=True)
    return lcd
