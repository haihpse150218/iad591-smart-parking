"""Smart Parking - Gateway + Application layer (Raspberry Pi 3B+).

Chay that :  python3 app.py               (Bluetooth /dev/rfcomm0, du phong /dev/ttyACM0)
Gia lap   :  python3 app.py --sim         (khong can mach, test tren laptop)
Mo trinh duyet: http://<IP-Pi>:5000
"""
import argparse
import csv
import io

from flask import Flask, Response, jsonify, render_template, request

import db
from link import DeviceLink

ALLOWED = {"GATE:OPEN", "GATE:CLOSE", "LIGHT:ON", "LIGHT:OFF", "LIGHT:AUTO",
           "MODE:AUTO", "MODE:MANUAL", "BUZZ"}

app = Flask(__name__)
device = None


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/api/latest")
def latest():
    return jsonify(device.snapshot())


@app.get("/api/history")
def history():
    minutes = request.args.get("min", default=30, type=int)
    return jsonify(db.history(max(1, min(minutes, 24 * 60))))


@app.get("/api/events")
def events():
    return jsonify({"events": db.recent_events(20), "today": db.stats_today()})


@app.post("/api/cmd")
def cmd():
    c = str((request.get_json(silent=True) or {}).get("cmd", "")).strip().upper()
    if c.startswith("CARS:") and c[5:].isdigit():
        pass
    elif c.startswith("LINK:"):
        ok = device.prefer(c[5:].lower())
        return jsonify({"ok": ok}), (200 if ok else 400)
    elif c not in ALLOWED:
        return jsonify({"ok": False, "error": "lenh khong hop le"}), 400
    device.send(c, "web")
    return jsonify({"ok": True, "cmd": c})


@app.get("/export.csv")
def export_csv():
    rows = db.all_readings()
    buf = io.StringIO()
    if rows:
        w = csv.DictWriter(buf, fieldnames=rows[0].keys())
        w.writeheader()
        w.writerows(rows)
    return Response(buf.getvalue(), mimetype="text/csv",
                    headers={"Content-Disposition": "attachment; filename=parking_readings.csv"})


def main():
    global device
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", action="store_true", help="gia lap Arduino")
    ap.add_argument("--port", type=int, default=5000)
    args = ap.parse_args()

    db.connect()
    if args.sim:
        from sim import FakeArduino
        device = DeviceLink(open_port=FakeArduino, ports=[("sim", "sim")])
    else:
        device = DeviceLink()
    device.start()
    app.run(host="0.0.0.0", port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
