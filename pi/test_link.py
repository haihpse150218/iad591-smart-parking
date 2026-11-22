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


if __name__ == "__main__":
    unittest.main()
