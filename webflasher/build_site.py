#!/usr/bin/env python3
"""Baut die Webflasher-Seite für GitHub Pages.

Liest die Releases des Repositories über die GitHub-API, lädt die Firmware-
Dateien der Firmware 3 (Namen siehe firmware/tools/release_assets.sh) in die
Seite und schreibt je Release ein Manifest für ESP Web Tools. Die Dateien
müssen auf derselben Seite liegen: GitHub liefert Release-Downloads ohne
CORS-Kopfzeilen aus, der Browser dürfte sie nicht laden.

    build_site.py --out _site [--repo Fabmobil/Pflanzensensor] [--max 10]
                  [--esp-web-tools node_modules/esp-web-tools/dist/web]
                  [--local firmware/dist --local-version 3.0.0-dev]

GITHUB_TOKEN (falls gesetzt) hebt das Abfragelimit der API an.
"""
import argparse
import json
import os
import shutil
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent

ESP8266 = [("pflanzensensor-esp8266.bin", 0x0)]
ESP32 = [
    ("pflanzensensor-esp32-bootloader.bin", 0x1000),
    ("pflanzensensor-esp32-partitions.bin", 0x8000),
    ("pflanzensensor-esp32-boot_app0.bin", 0xE000),
    ("pflanzensensor-esp32.bin", 0x10000),
]
OPTIONAL = ["pflanzensensor-esp8266.bin.md5", "pflanzensensor-esp32.bin.md5"]


def api(url):
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json",
                                               "User-Agent": "pflanzensensor-webflasher"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def download(url, dest):
    req = urllib.request.Request(url, headers={"Accept": "application/octet-stream",
                                               "User-Agent": "pflanzensensor-webflasher"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=300) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def manifest(version, files):
    """ESP-Web-Tools-Manifest; nur Plattformen, deren Dateien vollständig sind."""
    builds = []
    if all(n in files for n, _ in ESP8266):
        builds.append({"chipFamily": "ESP8266", "parts": [{"path": n, "offset": o} for n, o in ESP8266]})
    if all(n in files for n, _ in ESP32):
        builds.append({"chipFamily": "ESP32", "parts": [{"path": n, "offset": o} for n, o in ESP32]})
    return {
        "name": "Pflanzensensor",
        "version": version,
        # Nichts löschen, wenn man es nicht ausdrücklich ankreuzt: Einstellungen,
        # Chronik und Datei-Log liegen hinter der Firmware und bleiben stehen.
        "new_install_prompt_erase": True,
        # Nach dem Flashen auf Improv warten (WLAN-Einrichtung über USB); der
        # erste Start nach 2.x räumt alte Dateien weg und braucht etwas länger.
        "new_install_improv_wait_time": 20,
        "builds": builds,
    }


def wanted(asset_names):
    need = {n for n, _ in ESP8266} | {n for n, _ in ESP32}
    return [n for n in asset_names if n in need or n in OPTIONAL]


def add_release(out, entries, tag, version, name, date, prerelease, url, fetch):
    """fetch(name, dest) holt eine Datei; legt Manifest und Eintrag an."""
    d = out / "firmware" / tag
    d.mkdir(parents=True, exist_ok=True)
    files = fetch(d)
    m = manifest(version, files)
    if not m["builds"]:
        shutil.rmtree(d)
        return
    (d / "manifest.json").write_text(json.dumps(m, indent=2))
    # Die Seite setzt das Manifest im Browser selbst zusammen (Firmware +
    # Provisionierungsblock mit Hardware und Einstellungen); dafür die Teile.
    parts = {b["chipFamily"]: [{"path": f"firmware/{tag}/{p['path']}", "offset": p["offset"]}
                               for p in b["parts"]] for b in m["builds"]}
    entries.append({
        "tag": tag, "version": version, "name": name or tag, "date": date,
        "prerelease": prerelease, "url": url,
        "manifest": f"firmware/{tag}/manifest.json",
        "chips": [b["chipFamily"] for b in m["builds"]],
        "parts": parts,
    })


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="_site")
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "Fabmobil/Pflanzensensor"))
    ap.add_argument("--max", type=int, default=10, help="höchstens so viele Releases")
    ap.add_argument("--esp-web-tools", help="Verzeichnis dist/web aus dem npm-Paket esp-web-tools")
    ap.add_argument("--local", help="Verzeichnis mit Release-Dateien (lokaler Test)")
    ap.add_argument("--local-version", default="dev")
    ap.add_argument("--offline", action="store_true", help="GitHub nicht abfragen")
    a = ap.parse_args()

    out = Path(a.out)
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for f in ("index.html", "app.js", "style.css", "hardware.js", "provision.js", "flasher.js", "configimport.js"):
        shutil.copy(HERE / f, out / f)
    shutil.copy(HERE.parent / "firmware" / "web" / "favicon.ico", out / "favicon.ico")
    if a.esp_web_tools:
        shutil.copytree(a.esp_web_tools, out / "esp-web-tools")

    entries = []
    if a.local:
        src = Path(a.local)

        def fetch_local(d):
            names = wanted([p.name for p in src.iterdir()])
            for n in names:
                shutil.copy(src / n, d / n)
            return set(names)

        add_release(out, entries, "local", a.local_version, f"Lokaler Build {a.local_version}", "",
                    True, "", fetch_local)

    if not a.offline:
        releases = api(f"https://api.github.com/repos/{a.repo}/releases?per_page=50")
        for r in releases:
            if r.get("draft"):
                continue
            assets = {x["name"]: x for x in r.get("assets", [])}
            names = wanted(assets)
            if not names:
                continue  # Firmware 2.x oder ohne passende Dateien

            def fetch_remote(d, assets=assets, names=names):
                for n in names:
                    download(assets[n]["url"], d / n)
                return set(names)

            add_release(out, entries, r["tag_name"], r["tag_name"].lstrip("v"), r.get("name"),
                        (r.get("published_at") or "")[:10], bool(r.get("prerelease")),
                        r.get("html_url", ""), fetch_remote)
            if len([e for e in entries if e["tag"] != "local"]) >= a.max:
                break

    (out / "releases.json").write_text(json.dumps({"repo": a.repo, "releases": entries}, indent=2))
    (out / ".nojekyll").write_text("")
    print(f"{len(entries)} Release(s) in {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
