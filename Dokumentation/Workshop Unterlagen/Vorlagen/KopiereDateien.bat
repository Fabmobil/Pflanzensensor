@echo off
setlocal enabledelayedexpansion

REM Ermittle den aktuellen Benutzernamen
set "USERNAME=%USERNAME%"

REM Erstelle das Plugin-Verzeichnis falls es nicht existiert
if not exist "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins" (
    mkdir "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins"
    if errorlevel 1 (
        echo Fehler beim Erstellen des Plugin-Verzeichnisses
        pause
        exit /b 1
    )
)

REM Lösche alte Versionen des Plugins
if exist "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins\arduino-littlefs-upload-*.vsix" (
    del /F "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins\arduino-littlefs-upload-*.vsix"
    if errorlevel 1 (
        echo Fehler beim Löschen der alten Plugin-Version
        pause
        exit /b 1
    )
)

REM Kopiere die neue VSIX-Datei
for %%F in ("Plugin\arduino-littlefs-upload-*.vsix") do (
    copy "%%F" "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins\"
    if errorlevel 1 (
        echo Fehler beim Kopieren der VSIX-Datei
        pause
        exit /b 1
    )
)

REM Lösche das bestehende Arduino-Verzeichnis
if exist "C:\Benutzer\%USERNAME%\Dokumente\Arduino" (
    rmdir /s /q "C:\Benutzer\%USERNAME%\Dokumente\Arduino"
    if errorlevel 1 (
        echo Fehler beim Löschen des Arduino-Verzeichnisses
        pause
        exit /b 1
    )
)

REM Erstelle ein temporäres Verzeichnis
set "TEMP_DIR=%TEMP%\arduino_temp"
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"
if errorlevel 1 (
    echo Fehler beim Erstellen des temporären Verzeichnisses
    pause
    exit /b 1
)

REM Entpacke zunächst in temporäres Verzeichnis
tar -xf Arduino.zip -C "%TEMP_DIR%"
if errorlevel 1 (
    echo Fehler beim Entpacken der Arduino.zip
    rmdir /s /q "%TEMP_DIR%"
    pause
    exit /b 1
)

REM Erstelle das Zielverzeichnis
mkdir "C:\Benutzer\%USERNAME%\Dokumente\Arduino"
if errorlevel 1 (
    echo Fehler beim Erstellen des Arduino-Verzeichnisses
    rmdir /s /q "%TEMP_DIR%"
    pause
    exit /b 1
)

REM Verschiebe den Inhalt des Arduino-Ordners ins Zielverzeichnis
xcopy "%TEMP_DIR%\Arduino\*" "C:\Benutzer\%USERNAME%\Dokumente\Arduino\" /E /I /Y
if errorlevel 1 (
    echo Fehler beim Verschieben der Dateien
    rmdir /s /q "%TEMP_DIR%"
    pause
    exit /b 1
)

REM Räume auf
rmdir /s /q "%TEMP_DIR%"

echo Setup erfolgreich abgeschlossen!
pause
