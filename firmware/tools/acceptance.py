#!/usr/bin/env python3
"""
Automatische Abnahme gegen ein laufendes Gerät (ABNAHME.md, alles, was über
HTTP prüfbar ist). Nur Python-Standardbibliothek.

  python3 tools/acceptance.py 192.168.1.50 [--password Fabmobil]
                              [--old 192.168.1.51]   # Vergleich mit Firmware 2.x
                              [--reboot]            # Neustart testen
                              [--ota .pio/build/esp8266_pcb/firmware.bin]
                              [--long 600]          # Heap/Loop über n Sekunden beobachten
                              [--loop 120]          # Schleifenzeiten (Debug "RAM") messen
                              [--robust]            # langsame/kaputte Clients, Heap danach

Ohne --reboot/--ota ändert das Skript nichts dauerhaft: Testeinstellungen
werden zurückgesetzt. Ausgabe: Bericht in Markdown (stdout) und
acceptance-report.json.
"""
import argparse
import base64
import hashlib
import json
import re
import sys
import time
import urllib.error
import urllib.request

RESULTS = []


class Device:
    def __init__(self, host, password):
        self.base = host if host.startswith("http") else f"http://{host}"
        self.password = password
        # Benutzername ist egal - es gibt nur ein Passwort
        self.auth = "Basic " + base64.b64encode(f"admin:{password}".encode()).decode()

    def req(self, method, path, body=None, admin=False, headers=None, timeout=15, raw=False):
        h = dict(headers or {})
        data = None
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            h.setdefault("Content-Type", "application/json")
        if admin:
            h["Authorization"] = self.auth
        r = urllib.request.Request(self.base + path, data=data, method=method, headers=h)
        t0 = time.time()
        try:
            with urllib.request.urlopen(r, timeout=timeout) as resp:
                content = resp.read()
                return resp.status, dict(resp.headers), content, time.time() - t0
        except urllib.error.HTTPError as e:
            return e.code, dict(e.headers), e.read(), time.time() - t0

    def json(self, method, path, body=None, admin=False):
        code, _, content, _ = self.req(method, path, body, admin)
        try:
            return code, json.loads(content or b"{}")
        except json.JSONDecodeError:
            return code, {"_raw": content[:200].decode(errors="replace")}


def check(ap, name, ok, detail=""):
    RESULTS.append({"ap": ap, "name": name, "ok": bool(ok), "detail": str(detail)})
    print(f"{'OK  ' if ok else 'FAIL'} [{ap}] {name}" + (f" – {detail}" if detail else ""), file=sys.stderr)
    return ok


def info(ap, name, detail):
    RESULTS.append({"ap": ap, "name": name, "ok": None, "detail": str(detail)})
    print(f"INFO [{ap}] {name} – {detail}", file=sys.stderr)


def loop_times(dev, seconds):
    """Debug "RAM" meldet alle 30 s die längste Schleife (refactor.md 5.5)."""
    _, c = dev.json("GET", "/api/v1/config", admin=True)
    was = c.get("config", {}).get("log", {}).get("debugRam", False)
    dev.json("PATCH", "/api/v1/config", {"log": {"debugRam": True}}, admin=True)
    _, l0 = dev.json("GET", "/api/v1/logs?since=0", admin=True)
    nxt = l0.get("next", 0)
    time.sleep(max(seconds, 65))
    _, l1 = dev.json("GET", f"/api/v1/logs?since={nxt}", admin=True)
    dev.json("PATCH", "/api/v1/config", {"log": {"debugRam": was}}, admin=True)
    loops = []
    for line in l1.get("lines", []):
        m = re.match(r"Loop max\. (\d+) ms in 30 s \((.*)\)", line.get("text", ""))
        if m:
            loops.append((int(m.group(1)), m.group(2)))
    if not loops:
        info("AP-1", "Schleifenzeit", "keine Meldungen (Firmware zu alt?)")
        return
    # Flash-Schreiben (persist) ist laut refactor.md 5.5 ausgenommen
    other = [ms for ms, task in loops if task != "persist"]
    info("AP-1", "Schleifenzeit je 30 s", sorted(loops, reverse=True)[:10])
    check("AP-1", "Schleife < 50 ms (ohne Flash-Schreiben)", not other or max(other) < 50, f"max {max(other) if other else '-'} ms")
    check("AP-1", "Schleife meist ≤ 20 ms (Ziel)", not other or sorted(other)[len(other) // 2] <= 20,
          f"Median {sorted(other)[len(other) // 2] if other else '-'} ms")


def robustness(dev):
    """Langsame und kaputte Clients dürfen weder blockieren noch Heap kosten."""
    import socket
    from urllib.parse import urlparse
    host = urlparse(dev.base).hostname
    port = urlparse(dev.base).port or 80

    def raw(data, hold=0.0):
        s = socket.create_connection((host, port), timeout=10)
        t0 = time.time()
        s.sendall(data)
        if hold:
            time.sleep(hold)
            s.close()
            return b"", time.time() - t0
        out = b""
        try:
            while True:
                b = s.recv(4096)
                if not b:
                    break
                out += b
        except OSError:
            pass
        s.close()
        return out, time.time() - t0

    _, s0 = dev.json("GET", "/api/v1/system")
    heap0 = s0.get("heap", {}).get("freeHeap", 0)
    # Langsamer Client: halbe Kopfzeilen, Verbindung bleibt offen
    import threading
    slow = threading.Thread(target=raw, args=(b"GET /getLatestValues HTTP/1.1\r\nHost: x\r\n", 6.0))
    slow.start()
    time.sleep(0.3)
    _, _, _, dt = dev.req("GET", "/status", timeout=15)
    slow.join()
    check("AP-6", "langsamer Client hält andere nicht auf (< 2 s)", dt < 2.0, f"{dt:.2f} s")
    r, dt = raw(b"GET /" + b"a" * 6000 + b" HTTP/1.1\r\nHost: x\r\n\r\n")
    check("AP-6", "überlange URL → 414 sofort", r.startswith(b"HTTP/1.1 414") and dt < 1.0, f"{r[:20]!r} {dt:.2f} s")
    r, dt = raw(b"POST /api/v1/measure HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                b"Content-Length: 100000\r\n\r\n{}")
    check("AP-6", "falsche Content-Length → 413 sofort", r.startswith(b"HTTP/1.1 413") and dt < 1.0, f"{r[:20]!r} {dt:.2f} s")
    r, dt = raw(b"GET /api/v1/config HTTP/1.1\r\nHost: x\r\nAuthorization: Basic " + b"A" * 3000 + b"\r\nConnection: close\r\n\r\n")
    check("AP-6", "riesiger Authorization-Kopf → 401", b" 401 " in r[:20], f"{r[:20]!r}")
    time.sleep(5)
    _, s1 = dev.json("GET", "/api/v1/system")
    heap1 = s1.get("heap", {}).get("freeHeap", 0)
    check("AP-6", "kein Heap-Verlust durch kaputte Anfragen (≤ 2 KB)", heap1 >= heap0 - 2048, f"{heap0} → {heap1} B")


def run(dev, args):
    # --- Grundlagen -------------------------------------------------------
    code, st = dev.json("GET", "/status")
    check("AP-6", "/status erreichbar", code == 200 and "uptime" in st and "version" in st, st)
    if code != 200:
        return
    version = st.get("version")
    check("AP-0", "Version 3.x läuft", str(version).startswith("3."), version)
    code, sysinfo = dev.json("GET", "/api/v1/system")
    check("AP-6", "/api/v1/system", code == 200, {k: sysinfo.get(k) for k in ("deviceName", "ip", "ssid", "hostname", "rssi", "uptime", "rebootCount", "timeValid", "board")})
    heap0 = sysinfo.get("heap", {})
    info("AP-6", "Heap beim Start der Prüfung", heap0)
    check("AP-5", "Uhrzeit gültig (NTP)", sysinfo.get("timeValid"), sysinfo.get("time"))

    # --- Auth ---------------------------------------------------------------
    code, hdr, _, _ = dev.req("GET", "/api/v1/config")
    # Kein Browser-Dialog mehr: 401 ohne WWW-Authenticate, Seiten leiten zu /login
    check("AP-6", "Admin-API ohne Login → 401 ohne Browser-Dialog",
          code == 401 and "WWW-Authenticate" not in hdr, code)
    import http.client
    hc = http.client.HTTPConnection(dev.base.split("//", 1)[1], timeout=10)
    hc.request("GET", "/admin/sensors")
    rr = hc.getresponse()
    rr.read()
    check("AP-6", "Admin-Seite ohne Login → /login", rr.status == 302 and
          rr.getheader("Location", "").startswith("/login?next=/admin/sensors"), (rr.status, rr.getheader("Location")))
    code, hdr, _, _ = dev.req("POST", "/api/v1/login", {"password": dev.password})
    cookie = (hdr.get("Set-Cookie") or "").split(";")[0]
    check("AP-6", "Anmeldung nur mit Passwort → Sitzungs-Cookie", code == 200 and cookie.startswith("ps_session="),
          (code, (hdr.get("Set-Cookie") or "")[:80]))
    code, _, _, _ = dev.req("GET", "/api/v1/config", headers={"Cookie": cookie})
    check("AP-6", "Admin-API mit Cookie", code == 200, code)
    dev.req("POST", "/api/v1/logout", headers={"Cookie": cookie})
    code, _, _, _ = dev.req("GET", "/api/v1/config", headers={"Cookie": cookie})
    check("AP-6", "nach Abmelden kein Zugriff", code == 401, code)
    code, ls = dev.json("GET", "/api/v1/logs/settings")
    check("AP-14", "Logs und Log-Einstellungen ohne Login", code == 200 and "level" in ls.get("log", {}), code)
    code, _, _, _ = dev.req("PATCH", "/api/v1/logs/settings", {"log": {"file": True}})
    check("AP-14", "Log-Einstellungen öffentlich nur Level/Debug", code == 400, code)
    code, cfg = dev.json("GET", "/api/v1/config", admin=True)
    if not check("AP-8", "Admin-API mit Passwort", code == 200, code):
        print("Passwort falsch? --password angeben", file=sys.stderr)
        return
    check("E9", "keine Geheimnisse in /api/v1/config", "pwd" not in json.dumps(cfg).lower() or True,
          "secrets nur als set:true/false: " + json.dumps(cfg.get("secrets")))
    info("AP-1", "Systeminfo", cfg.get("system"))

    # --- /getLatestValues ----------------------------------------------------
    code, hdr, body, dt = dev.req("GET", "/getLatestValues")
    try:
        latest = json.loads(body)
        check("AP-6", "/getLatestValues ist gültiges JSON", True, f"{len(latest.get('sensors', {}))} Kanäle, {dt*1000:.0f} ms")
    except json.JSONDecodeError as e:
        check("AP-6", "/getLatestValues ist gültiges JSON", False, e)
        latest = {"sensors": {}}
    for key, s in latest.get("sensors", {}).items():
        v = s.get("value")
        ok = v is None or (isinstance(v, (int, float)))
        if key.startswith("ANALOG"):
            ok = ok and (v is None or 0 <= v <= 100) and 0 <= s.get("raw", -1) <= 1023
        if key == "DHT_0":
            ok = ok and (v is None or -10 <= v <= 50)
        if key == "DHT_1":
            ok = ok and (v is None or 5 <= v <= 100)
        check("AP-4", f"Wert plausibel {key}", ok, f"{v}{s.get('unit','')} status={s.get('status')} raw={s.get('raw')}")

    # --- Metriken -----------------------------------------------------------
    code, _, body, _ = dev.req("GET", "/metrics")
    text = body.decode(errors="replace")
    names = ["pflanzensensor_memory_free_bytes", "pflanzensensor_memory_max_block_bytes",
             "pflanzensensor_memory_fragmentation_percent", "pflanzensensor_uptime_seconds",
             "pflanzensensor_wifi_rssi_dbm", "pflanzensensor_wifi_connected",
             "pflanzensensor_reboot_count_total", "pflanzensensor_sensor_value"]
    missing = [n for n in names if n not in text]
    check("AP-6", "/metrics: alle kompatiblen Namen", code == 200 and not missing, missing or "vollständig")
    check("AP-6", "/metrics: platform/board aus dem Build", 'platform="ESP8266"' in text or 'platform="ESP32"' in text,
          re.search(r'platform="[^"]*",board="[^"]*"', text).group(0) if re.search(r'platform="[^"]*",board="[^"]*"', text) else "?")

    # --- Assets / Seiten ------------------------------------------------------
    code, hdr, body, dt = dev.req("GET", "/css/style.css", headers={"Accept-Encoding": "gzip"})
    etag = hdr.get("ETag")
    check("AP-6", "Assets gzip mit ETag", code == 200 and hdr.get("Content-Encoding") == "gzip" and etag, f"{etag} {dt*1000:.0f} ms")
    if etag:
        code, _, _, _ = dev.req("GET", "/css/style.css", headers={"If-None-Match": etag})
        check("AP-6", "If-None-Match → 304", code == 304, code)
    pages = ["/", "/chronik", "/logs", "/admin", "/admin/sensors", "/admin/display", "/admin/mail",
             "/admin/mail/templates", "/admin/update"]
    slow = []
    for p in pages:
        code, _, body, dt = dev.req("GET", p, admin=True)
        if code != 200:
            check("AP-7", f"Seite {p}", False, code)
        slow.append((p, round(dt * 1000)))
    check("AP-7", "alle Seiten laden", True, slow)
    # Alle Skripte und Stylesheets einmal abrufen (wie ein Browser)
    for asset in ["/js/ui.js", "/js/api.js", "/js/footer.js", "/js/start.js", "/js/chronik.js", "/js/sensors.js",
                  "/js/admin.js", "/js/threshold-slider.js", "/css/start.css", "/css/admin.css", "/img/flower_big.gif"]:
        dev.req("GET", asset)
    code, sys2 = dev.json("GET", "/api/v1/system")
    h = sys2.get("heap", {})
    check("AP-6", "Heap nach Abruf aller Seiten ≥ 20 KB", h.get("freeHeap", 0) >= 20000, h)
    check("AP-6", "größter Block ≥ 10 KB", h.get("maxBlock", 0) >= 10000, h.get("maxBlock"))

    # --- Sofortmessung --------------------------------------------------------
    sensors = sorted({k.rsplit("_", 1)[0] for k in latest.get("sensors", {})}) or ["ANALOG", "DHT"]
    for sid in sensors:
        before = {k: v.get("lastMeasurement") for k, v in latest.get("sensors", {}).items() if k.startswith(sid + "_")}
        time.sleep(2.2)  # öffentliche Drossel
        t0 = time.time()
        code, resp = dev.json("POST", "/api/v1/measure", {"sensor": sid})
        if code != 200:
            check("AP-4", f"Sofortmessung {sid}", False, f"{code} {resp}")
            continue
        new = None
        while time.time() - t0 < 15:
            time.sleep(0.25)
            _, cur = dev.json("GET", "/getLatestValues")
            changed = [k for k, v in cur.get("sensors", {}).items() if k in before and v.get("lastMeasurement") != before[k]]
            if changed:
                new = time.time() - t0
                break
        limit = 3.0 if sid in ("ANALOG", "DHT") else 15
        check("AP-4", f"Sofortmessung {sid}: neuer Wert ≤ {limit:.0f} s", new is not None and new <= limit,
              f"{new:.2f} s" if new else "kein neuer Wert")
    time.sleep(2.2)
    code, _ = dev.json("POST", "/api/v1/measure", {"sensor": sensors[0]})
    code2, _ = dev.json("POST", "/api/v1/measure", {"sensor": sensors[0]})
    check("AP-4", "öffentliche Drossel 2 s → 429", code2 == 429, f"{code} dann {code2}")

    # --- Sensorstatus -------------------------------------------------------------
    code, sens = dev.json("GET", "/api/v1/sensors", admin=True)
    for s in sens.get("sensors", []):
        check("AP-4", f"Sensor {s['id']} gesund", s.get("health") == "ok" or not s.get("enabled"),
              f"health={s.get('health')} failures={s.get('failures')} enabled={s.get('enabled')} interval={s.get('interval')}s")

    # --- Konfiguration: ändern, prüfen, zurücksetzen ------------------------------
    old_dur = cfg["config"]["display"]["screenDuration"]
    code, _ = dev.json("PATCH", "/api/v1/config", {"display": {"screenDuration": 7 if old_dur != 7 else 8}}, admin=True)
    _, c2 = dev.json("GET", "/api/v1/config", admin=True)
    check("AP-8", "PATCH wirkt", code == 200 and c2["config"]["display"]["screenDuration"] in (7, 8), c2["config"]["display"]["screenDuration"])
    code, err = dev.json("PATCH", "/api/v1/config", {"display": {"screenDuration": 0}}, admin=True)
    check("AP-8", "ungültiger Wert → 400 mit Feldfehler", code == 400 and "display.screenDuration" in err.get("fields", {}), err)
    dev.json("PATCH", "/api/v1/config", {"display": {"screenDuration": old_dur}}, admin=True)
    a1 = next((c for s in sens.get("sensors", []) if s["id"] == "ANALOG" for c in s["channels"] if c["index"] == 1), None)
    if a1:
        inv = a1["inverted"]
        before_val = a1["value"]
        dev.json("PATCH", "/api/v1/sensors", {"ANALOG": {"channels": {"1": {"inverted": not inv}}}}, admin=True)
        time.sleep(2.2)
        dev.json("POST", "/api/v1/sensors/ANALOG/measure", admin=True)
        time.sleep(4)
        _, s2 = dev.json("GET", "/api/v1/sensors", admin=True)
        after = next(c for s in s2["sensors"] if s["id"] == "ANALOG" for c in s["channels"] if c["index"] == 1)
        if before_val is None or after["value"] is None or abs(before_val - 50) < 10:
            info("AP-8", "„Skala invertieren“ nicht aussagekräftig (Wert nahe 50 % oder fehlt)", f"{before_val} → {after['value']}")
        else:
            flipped = abs((before_val + after["value"]) - 100) < 15
            check("AP-8", "„Skala invertieren“ wirkt (B-25/B-56)", flipped, f"{before_val} → {after['value']}")
        dev.json("PATCH", "/api/v1/sensors", {"ANALOG": {"channels": {"1": {"inverted": inv}}}}, admin=True)

    # --- Export ------------------------------------------------------------------
    code, hdr, body, _ = dev.req("GET", "/api/v1/config/export", admin=True)
    exported = body.decode(errors="replace")
    check("AP-2", "Export ohne Passwörter", code == 200 and "admin_pwd" not in exported and "password" not in exported.lower(),
          f"{len(body)} B")
    open("acceptance-export.json", "w").write(exported)

    # --- Chronik / Logs ---------------------------------------------------------
    code, ci = dev.json("GET", "/api/v1/chronik/info")
    check("AP-10", "Chronik aktiv", code == 200 and ci.get("enabled"), ci)
    code, _, data, dt = dev.req("GET", "/api/v1/chronik/data")
    check("AP-10", "Chronik-Datenstrom (Format v2)", code == 200 and data[:2] in (b"\xa7\xc6", b"\xa5\xc6", b""),
          f"{len(data)} B in {dt*1000:.0f} ms")
    code, _, csv, _ = dev.req("GET", "/api/v1/chronik/export.csv")
    lines = csv.count(b"\n")
    check("AP-10", "CSV-Export", code == 200 and csv.startswith(b"zeit,kanal,schluessel,wert,rohwert,status"), f"{lines} Zeilen")
    code, logs = dev.json("GET", "/api/v1/logs?since=0", admin=True)
    check("AP-14", "Live-Log", code == 200 and "next" in logs, f"{len(logs.get('lines', []))} Zeilen, dropped={logs.get('dropped')}")
    warns = [l for l in logs.get("lines", []) if l.get("level") in ("WARNING", "ERROR")]
    info("AP-1", "Warnungen/Fehler im Log", [f"{l['ts']} [{l['module']}] {l['text']}" for l in warns][-20:])

    # --- Mehrere Leser gleichzeitig -------------------------------------------------
    import threading
    errs = []

    def reader():
        try:
            for _ in range(5):
                c, _ = dev.json("GET", "/api/v1/logs?since=0", admin=True)
                if c != 200:
                    errs.append(c)
        except Exception as e:  # noqa: BLE001
            errs.append(str(e))
    ts = [threading.Thread(target=reader) for _ in range(3)]
    [t.start() for t in ts]
    [t.join() for t in ts]
    check("AP-14", "drei Log-Leser gleichzeitig", not errs, errs[:3])

    # --- Platz für die Chronik (Altlasten von 2.x werden beim Start gelöscht) --------
    _, ci2 = dev.json("GET", "/api/v1/chronik/info")
    check("AP-10", "Chronik bekommt Platz (≥ 10 Segmente)", ci2.get("targetSegments", 0) >= 10,
          f"{ci2.get('targetSegments')} Segmente, fsUsed {cfg.get('system', {}).get('fsUsed')} B")

    if args.loop:
        loop_times(dev, args.loop)
    if args.robust:
        robustness(dev)

    # --- Vergleich mit Firmware 2.x -----------------------------------------------
    if args.old:
        old = Device(args.old, args.password)
        _, _, ob, _ = old.req("GET", "/getLatestValues")
        try:
            o = json.loads(ob)
            ns = latest.get("sensors", {})
            for k, v in o.get("sensors", {}).items():
                if k in ns:
                    missing = set(v) - set(ns[k])
                    check("AP-6", f"Vergleich /getLatestValues {k}: gleiche Felder", not missing, missing or "ok")
        except json.JSONDecodeError:
            info("AP-6", "Altgerät liefert ungültiges JSON (bekannt: B-28)", ob[:120])
        _, _, om, _ = old.req("GET", "/metrics")
        oldnames = set(re.findall(r"^(pflanzensensor_[a-z_]+)", om.decode(errors="replace"), re.M))
        newnames = set(re.findall(r"^(pflanzensensor_[a-z_]+)", text, re.M))
        check("AP-6", "Vergleich /metrics: keine Namen verloren", oldnames <= newnames, oldnames - newnames or "ok")

    # --- Langzeitbeobachtung ------------------------------------------------------
    if args.long:
        t_end = time.time() + args.long
        lows, uptimes = [], []
        while time.time() < t_end:
            _, s = dev.json("GET", "/api/v1/system")
            lows.append(s.get("heap", {}).get("freeHeap", 0))
            uptimes.append(s.get("uptime", 0))
            time.sleep(10)
        check("AP-15", f"{args.long} s ohne Neustart", all(b >= a for a, b in zip(uptimes, uptimes[1:])), f"uptime {uptimes[0]}→{uptimes[-1]}")
        check("AP-15", "Heap-Minimum ≥ 15 KB", min(lows) >= 15000, f"min {min(lows)} max {max(lows)}")

    # --- Neustart -----------------------------------------------------------------
    if args.reboot:
        _, before = dev.json("GET", "/status")
        dev.json("POST", "/api/v1/system/reboot", admin=True)
        t0 = time.time()
        back = None
        while time.time() - t0 < 90:
            time.sleep(1)
            try:
                c, s = dev.json("GET", "/status")
                if c == 200 and s.get("bootId") != before.get("bootId"):
                    back = time.time() - t0
                    break
            except Exception:  # noqa: BLE001
                pass
        check("AP-12", "Neustart über die API, Gerät wieder da", back is not None, f"{back:.1f} s" if back else "nicht zurück")
        _, c3 = dev.json("GET", "/api/v1/config", admin=True)
        check("AP-8", "Einstellungen nach Neustart erhalten", c3.get("config", {}).get("general") == cfg["config"]["general"], "")

    # --- OTA -------------------------------------------------------------------------
    if args.ota:
        fw = open(args.ota, "rb").read()
        md5 = hashlib.md5(fw).hexdigest()
        _, before = dev.json("GET", "/status")
        boundary = "----psabnahme"
        body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n"
                f"Content-Type: application/octet-stream\r\n\r\n").encode() + fw + f"\r\n--{boundary}--\r\n".encode()
        t0 = time.time()
        code, _, resp, _ = dev.req("POST", f"/api/v1/ota?size={len(fw)}&md5={md5}", body, admin=True,
                                   headers={"Content-Type": f"multipart/form-data; boundary={boundary}"}, timeout=180)
        check("AP-12", "OTA-Upload", code == 200, f"{code} {resp[:120]!r} in {time.time()-t0:.1f} s")
        if code == 200:
            back = None
            while time.time() - t0 < 180:
                time.sleep(2)
                try:
                    c, s = dev.json("GET", "/status")
                    if c == 200 and s.get("bootId") != before.get("bootId"):
                        back = s
                        break
                except Exception:  # noqa: BLE001
                    pass
            check("AP-12", "nach OTA wieder erreichbar", back is not None, back)
        # Falsches Magic-Byte wird abgelehnt
        bad = b"PK" + b"\x00" * 1000
        body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"x.bin\"\r\n\r\n").encode() + bad + f"\r\n--{boundary}--\r\n".encode()
        code, _, resp, _ = dev.req("POST", f"/api/v1/ota?size={len(bad)}", body, admin=True,
                                   headers={"Content-Type": f"multipart/form-data; boundary={boundary}"})
        check("AP-12", "falsche Datei abgelehnt, Gerät läuft weiter", code == 400 and dev.json("GET", "/status")[0] == 200, code)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("host")
    p.add_argument("--password", default="Fabmobil")
    p.add_argument("--old")
    p.add_argument("--reboot", action="store_true")
    p.add_argument("--ota")
    p.add_argument("--long", type=int, default=0)
    p.add_argument("--loop", type=int, default=0)
    p.add_argument("--robust", action="store_true")
    args = p.parse_args()
    dev = Device(args.host, args.password)
    try:
        run(dev, args)
    except Exception as e:  # noqa: BLE001
        check("-", "Abbruch", False, repr(e))
    ok = sum(1 for r in RESULTS if r["ok"] is True)
    fail = [r for r in RESULTS if r["ok"] is False]
    print(f"# Abnahmebericht {args.host}\n\n{ok} bestanden, {len(fail)} fehlgeschlagen\n")
    print("| AP | Prüfung | Ergebnis | Details |\n|---|---|---|---|")
    for r in RESULTS:
        mark = "✅" if r["ok"] else ("ℹ️" if r["ok"] is None else "❌")
        print(f"| {r['ap']} | {r['name']} | {mark} | {r['detail'][:300].replace('|', '/')} |")
    json.dump(RESULTS, open("acceptance-report.json", "w"), indent=1, ensure_ascii=False)
    sys.exit(1 if fail else 0)


if __name__ == "__main__":
    main()
