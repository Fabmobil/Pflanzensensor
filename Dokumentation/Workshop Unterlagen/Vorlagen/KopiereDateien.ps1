# Ermittle den aktuellen Benutzernamen
$USERNAME = $env:USERNAME

# Erstelle das Plugin-Verzeichnis falls es nicht existiert
$pluginPath = "C:\Users\$USERNAME\.arduinoIDE\plugins"
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
$arduinoPath = "C:\Users\$USERNAME\Documents\Arduino"
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

# Arduino15.zip:
# Zielverzeichnis definieren
$targetPath = "C:\Users\$username\AppData\Local\"

# Temporäres Verzeichnis für die Zwischenspeicherung
$tempDir = [System.IO.Path]::GetTempPath()
$tempZipFile = Join-Path $tempDir "Arduino15_temp.zip"

# Prüfen ob das Zielverzeichnis existiert, wenn nicht erstellen
if(-not (Test-Path -Path $targetPath)) {
    New-Item -ItemType Directory -Path $targetPath -Force
    Write-Host "Verzeichnis $targetPath wurde erstellt."
}

# Aktuelles Verzeichnis ermitteln (wo das Skript liegt)
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$zipPath = Join-Path $scriptPath "Arduino15.zip"

# Prüfen ob die ZIP-Datei existiert
if(Test-Path -Path $zipPath) {
    try {
        # ZIP-Datei in temporäres Verzeichnis kopieren
        Write-Host "Kopiere ZIP-Datei in temporäres Verzeichnis..."
        Copy-Item -Path $zipPath -Destination $tempZipFile -Force

        # ZIP-Datei aus temporärem Verzeichnis entpacken
        Write-Host "Entpacke ZIP-Datei..."
        Expand-Archive -Path $tempZipFile -DestinationPath $targetPath -Force
        Write-Host "Arduino15.zip wurde erfolgreich nach $targetPath entpackt."

        # Temporäre ZIP-Datei aufräumen
        if(Test-Path -Path $tempZipFile) {
            Remove-Item -Path $tempZipFile -Force
            Write-Host "Temporäre Dateien wurden aufgeräumt."
        }
    }
    catch {
        Write-Host "Fehler beim Verarbeiten der ZIP-Datei: $_" -ForegroundColor Red
        # Aufräumen im Fehlerfall
        if(Test-Path -Path $tempZipFile) {
            Remove-Item -Path $tempZipFile -Force
        }
    }
}
else {
    Write-Host "FEHLER: Arduino15.zip wurde nicht im Verzeichnis $scriptPath gefunden." -ForegroundColor Red
}

# Trage ESP8266 Board URL ein, falls sie fehlt
$preferencesPath = "$env:LOCALAPPDATA\Arduino15\preferences.txt"
$esp8266URL = "https://arduino.esp8266.com/stable/package_esp8266com_index.json"

# Prüfen ob Arduino15 Ordner existiert
if (!(Test-Path "$env:LOCALAPPDATA\Arduino15")) {
    New-Item -ItemType Directory -Path "$env:LOCALAPPDATA\Arduino15"
}

# Prüfen ob preferences.txt existiert
if (!(Test-Path $preferencesPath)) {
    New-Item -ItemType File -Path $preferencesPath
    $content = @()
} else {
    $content = Get-Content $preferencesPath
}

$urlLine = $content | Where-Object { $_ -match "boardsmanager.additional.urls=" }

if ($urlLine) {
    if ($urlLine -notmatch $esp8266URL) {
        $updatedLine = $urlLine + "," + $esp8266URL
        $content = $content -replace [regex]::Escape($urlLine), $updatedLine
    }
} else {
    $content += "boardsmanager.additional.urls=$esp8266URL"
}

$content | Set-Content $preferencesPath

Write-Host "Setup der ArduinoIDE für den Pflanzensensor erfolgreich abgeschlossen!"
Read-Host "Drücken Sie eine beliebige Taste, um fortzufahren..."
