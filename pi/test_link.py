"""python3 -m unittest test_link  - kiem tra parser va logic link (khong can mach)."""
import time
import unittest

import db
from link import DeviceLink, parse
from sim import FakeArduino


class ParseTest(unittest.TestCase):
    def test_state(self):
        m = parse('{"d1":23,"ldr":512,"cars":2,"cap":4,"gate":0,"light":1,"mode":"AUTO"}\r\n')
        self.assertEqual(m["cars"], 2)

    def test_garbage(self):
        for bad in ["", "abc", "{bad json", '{"d1":', "[1,2]", "\x00\xff{"]:
            self.assertIsNone(parse(bad), bad)


class LinkTest(unittest.TestCase):
    def setUp(self):
        db.connect(":memory:")

    def test_sim_end_to_end(self):
        dev = DeviceLink(open_port=FakeArduino, ports=[("sim", "sim")])
        dev.start()
        time.sleep(2.5)
        self.assertTrue(dev.snapshot()["online"])
        dev.send("CARS:3")
        dev.send("MODE:MANUAL")
        time.sleep(1.5)
        s = dev.snapshot()
        self.assertEqual((s["cars"] >= 2, s["mode"]), (True, "MANUAL"))
        self.assertGreater(len(db.history(5)), 0)

    def test_fallback_to_serial(self):
        def opener(path):
            if path == "bt":
                raise OSError("no rfcomm")
            return FakeArduino()
        dev = DeviceLink(open_port=opener, ports=[("bt", "bt"), ("serial", "usb")])
        dev.start()
        time.sleep(1.5)
        self.assertEqual(dev.snapshot()["link"], "serial")


class LossyArduino(FakeArduino):
    """Lam mat 2 lenh dau tien (nhu SoftwareSerial mat byte) -> link phai gui lai."""
    def __init__(self, path="sim"):
        super().__init__(path)
        self.drop, self.got = 2, []

    def write(self, data):
        if self.drop:
            self.drop -= 1
            return
        self.got.append(data.decode().strip())
        super().write(data)


class AckRetryTest(unittest.TestCase):
    def setUp(self):
        db.connect(":memory:")

    def test_resend_until_ack(self):
        devs = []
        dev = DeviceLink(open_port=lambda p: devs.append(LossyArduino()) or devs[-1], ports=[("sim", "sim")])
        dev.start()
        time.sleep(1)
        dev.send("CARS:2")
        dev.send("MODE:MANUAL")
        time.sleep(5)
        s = dev.snapshot()
        self.assertEqual(s["mode"], "MANUAL")          # so xe co the doi do sim cho xe den ngau nhien
        self.assertEqual(devs[0].got, ["CARS:2", "MODE:MANUAL"])   # moi lenh toi dung 1 lan, dung thu tu
        self.assertIsNone(dev._pending)


class FakeBus:
    def __init__(self):
        self.writes = []

    def write_byte(self, addr, b):
        self.writes.append((addr, b))


class PiLcdTest(unittest.TestCase):
    def test_render(self):
        from datetime import datetime
        from pi_lcd import render
        snap = {"online": True, "cars": 1, "cap": 4, "link": "bt"}
        l1, l2 = render(snap, {"car_in": 12, "car_out": 9}, datetime(2026, 11, 22, 14, 5))
        self.assertEqual((l1, l2), ("Trong 3/4    BT", "V:12 R:9   14:05"))
        self.assertTrue(render({"online": True, "cars": 4, "cap": 4, "link": "serial"}, {})[0].startswith("BAI DAY"))
        self.assertEqual(render({"online": False}, {})[0], "Mat ket noi")
        for line in render(snap, {"car_in": 999, "car_out": 999}):
            self.assertLessEqual(len(line), 16)

    def test_driver_writes_to_bus(self):
        from pi_lcd import PiLcd
        bus = FakeBus()
        lcd = PiLcd(0x27, bus=bus)
        n = len(bus.writes)
        lcd.line(0, "Trong 3/4")
        # 1 lenh dat con tro + 16 ky tu, moi byte = 2 nibble x 2 lan ghi (E len/xuong)
        self.assertEqual(len(bus.writes) - n, (1 + 16) * 4)
        self.assertTrue(all(a == 0x27 and b & 0x08 for a, b in bus.writes))


if __name__ == "__main__":
    unittest.main()
