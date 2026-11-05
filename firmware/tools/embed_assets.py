#!/usr/bin/env python3
"""
Bettet die Weboberfläche gzip-komprimiert in die Firmware ein (E6).

Liest firmware/web/ (HTML, js/, css/, img/, favicon.ico), komprimiert jede
Datei mit gzip -9, bildet ein kurzes ETag (SHA-1) und schreibt
src/web/assets.h. Bricht ab, wenn die Gesamtgröße über MAX_TOTAL liegt.

Als PlatformIO-Pre-Skript setzt es zusätzlich PS_VERSION aus der Datei VERSION
und schreibt das Baudatum nach src/core/build_date.h.
Auch direkt aufrufbar: python3 tools/embed_assets.py
"""
import gzip
import hashlib
import io
import os
import pathlib
import sys

MAX_TOTAL = 200 * 1024
TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript",
    ".css": "text/css",
    ".png": "image/png",
    ".gif": "image/gif",
    ".ico": "image/x-icon",
    ".svg": "image/svg+xml",
    ".json": "application/json",
}


def project_dir():
    try:
        Import("env")  # noqa: F821 (SCons)
        return pathlib.Path(env["PROJECT_DIR"])  # noqa: F821
    except Exception:
        return pathlib.Path(__file__).resolve().parent.parent


def gz(data):
    buf = io.BytesIO()
    # mtime=0: reproduzierbar, das ETag ändert sich nur mit dem Inhalt
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as f:
        f.write(data)
    return buf.getvalue()


def ident(path):
    return "k_" + "".join(c if c.isalnum() else "_" for c in path.strip("/"))


def generate(root):
    web = root / "web"
    out = root / "src" / "web" / "assets.h"
    files = []
    for p in sorted(web.rglob("*")):
        if not p.is_file() or p.suffix not in TYPES:
            continue
        rel = "/" + p.relative_to(web).as_posix()
        if len(rel) >= 40:
            sys.exit(f"Pfad zu lang für die Asset-Tabelle: {rel}")
        files.append((rel, p))
    lines = [
        "// Erzeugt von tools/embed_assets.py - nicht von Hand ändern.",
        "#pragma once",
        "",
        '#include "web/asset_types.h"',
        "",
        "namespace ps {",
        "namespace assets {",
        "",
    ]
    table = []
    total = 0
    for rel, p in files:
        raw = p.read_bytes()
        data = gz(raw)
        total += len(data)
        name = ident(rel)
        etag = hashlib.sha1(raw).hexdigest()[:10]
        hexed = ",".join(f"0x{b:02x}" for b in data)
        lines.append(f"// {rel}: {len(raw)} B, gzip {len(data)} B")
        lines.append(f"const uint8_t {name}[] PS_PROGMEM __attribute__((aligned(4))) = {{{hexed}}};")
        html = "true" if p.suffix == ".html" else "false"
        table.append(f'    {{"{rel}", "{TYPES[p.suffix]}", "{etag}", {name}, {len(data)}, {html}}},')
    if total > MAX_TOTAL:
        sys.exit(f"Web-Assets zu groß: {total} B > {MAX_TOTAL} B")
    lines.append("")
    if table:
        lines.append("const Asset kAssets[] PS_PROGMEM = {")
        lines.extend(table)
        lines.append("};")
    else:
        lines.append("const Asset kAssets[1] PS_PROGMEM = {};")
    lines.append(f"constexpr uint8_t kCount = {len(table)};")
    lines.append(f"constexpr uint32_t kTotalBytes = {total};")
    lines.append("")
    lines.append("}  // namespace assets")
    lines.append("}  // namespace ps")
    content = "\n".join(lines) + "\n"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text() != content:
        out.write_text(content)
    print(f"embed_assets: {len(table)} Dateien, {total} B gzip")


MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
# Nix-Shells setzen SOURCE_DATE_EPOCH auf 1980-01-01 (ZIP-Epoche); GCC macht
# daraus __DATE__ = "Jan  1 1980". Diesen Platzhalter ignorieren, echte Werte
# (reproduzierbare Builds) übernehmen.
NIX_PLACEHOLDER_EPOCH = 315532800


def build_date():
    import time

    sde = os.environ.get("SOURCE_DATE_EPOCH", "")
    if sde.isdigit() and int(sde) > NIX_PLACEHOLDER_EPOCH:
        t = time.gmtime(int(sde))
    else:
        t = time.localtime()
    # gleiches Format wie __DATE__ ("Nov  4 2026"), unabhängig von der Locale
    return f"{MONTHS[t.tm_mon - 1]} {t.tm_mday:2d} {t.tm_year}"


def write_build_date(root):
    out = root / "src" / "core" / "build_date.h"
    content = (
        "// Erzeugt von tools/embed_assets.py - nicht von Hand ändern.\n"
        "#pragma once\n"
        f'#define PS_BUILD_DATE "{build_date()}"\n'
    )
    if not out.exists() or out.read_text() != content:
        out.write_text(content)


root = project_dir()
generate(root)
write_build_date(root)
version = (root / "VERSION").read_text().strip()
try:
    env.Append(CPPDEFINES=[("PS_VERSION", '\\"%s\\"' % version)])  # noqa: F821
except NameError:
    pass
