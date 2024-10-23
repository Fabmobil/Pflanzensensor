# Ermittle den aktuellen Benutzernamen
$USERNAME = $env:USERNAME

# Erstelle das Plugin-Verzeichnis falls es nicht existiert
$pluginPath = "C:\Benutzer\$USERNAME\.arduinoIDE\plugins"
if (-not (Test-Path -Path $pluginPath)) {
    try {
        New-Item -ItemType Directory -Path $pluginPath -ErrorAction Stop
    } catch {
        Write-Host "Fehler beim Erstellen des Plugin-Verzeichnisses"
        Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
        exit 1
    }
}

# Lösche alte Versionen des Plugins
$oldPlugins = Get-ChildItem "$pluginPath\arduino-littlefs-upload-*.vsix"
if ($oldPlugins) {
    try {
        Remove-Item "$pluginPath\arduino-littlefs-upload-*.vsix" -Force -ErrorAction Stop
    } catch {
        Write-Host "Fehler beim Löschen der alten Plugin-Version"
        Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
        exit 1
    }
}

# Kopiere die neue VSIX-Datei
$vsixFiles = Get-ChildItem -Path "." -Filter "arduino-littlefs-upload-*.vsix"
foreach ($file in $vsixFiles) {
    try {
        Copy-Item -Path $file.FullName -Destination $pluginPath -Force -ErrorAction Stop
    } catch {
        Write-Host "Fehler beim Kopieren der VSIX-Datei"
        Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
        exit 1
    }
}

# Lösche das bestehende Arduino-Verzeichnis
$arduinoPath = "C:\Benutzer\$USERNAME\Dokumente\Arduino"
if (Test-Path -Path $arduinoPath) {
    try {
        Remove-Item -Path $arduinoPath -Recurse -Force -ErrorAction Stop
    } catch {
        Write-Host "Fehler beim Löschen des Arduino-Verzeichnisses"
        Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
        exit 1
    }
}

# Erstelle ein temporäres Verzeichnis
$tempDir = Join-Path $env:TEMP "arduino_temp"
if (Test-Path -Path $tempDir) {
    Remove-Item -Path $tempDir -Recurse -Force
}
try {
    New-Item -ItemType Directory -Path $tempDir -ErrorAction Stop
} catch {
    Write-Host "Fehler beim Erstellen des temporären Verzeichnisses"
    Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
    exit 1
}

# Entpacke zunächst in temporäres Verzeichnis
try {
    tar -xf "Arduino.zip" -C $tempDir
} catch {
    Write-Host "Fehler beim Entpacken der Arduino.zip"
    Remove-Item -Path $tempDir -Recurse -Force
    Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
    exit 1
}

# Erstelle das Zielverzeichnis
try {
    New-Item -ItemType Directory -Path $arduinoPath -ErrorAction Stop
} catch {
    Write-Host "Fehler beim Erstellen des Arduino-Verzeichnisses"
    Remove-Item -Path $tempDir -Recurse -Force
    Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
    exit 1
}

# Verschiebe den Inhalt des Arduino-Ordners ins Zielverzeichnis
try {
    Copy-Item -Path "$tempDir\Arduino\*" -Destination $arduinoPath -Recurse -Force -ErrorAction Stop
} catch {
    Write-Host "Fehler beim Verschieben der Dateien"
    Remove-Item -Path $tempDir -Recurse -Force
    Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
    exit 1
}

# Räume auf
Remove-Item -Path $tempDir -Recurse -Force

Write-Host "Setup erfolgreich abgeschlossen!"
Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
