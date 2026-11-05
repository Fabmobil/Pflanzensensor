"""PlatformIO-Pre-Skript (nur native): C-Dateien mit C11 übersetzen.

GCC 15 übersetzt C standardmäßig als C23; dort ist `bool` ein Schlüsselwort,
und ricmoo/QRCode definiert es per typedef selbst - der Build bricht ab.
-std=gnu++17 aus build_flags gilt nur für C++.
"""
Import("env")  # noqa: F821

env.Append(CFLAGS=["-std=gnu11"])  # noqa: F821
