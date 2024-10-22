@echo off
setlocal enabledelayedexpansion

REM Ermittle den aktuellen Benutzernamen
set "USERNAME=%USERNAME%"

REM Erstelle das Plugin-Verzeichnis falls es nicht existiert
if not exist "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins" (
    mkdir "C:\Benutzer\%USERNAME%\.arduinoIDE\plugins"
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
for %%F in ("arduino-littlefs-upload-*.vsix") do (
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

REM Kopiere das neue Arduino-Verzeichnis
xcopy "Arduino" "C:\Benutzer\%USERNAME%\Dokumente\Arduino\" /E /I /Y
if errorlevel 1 (
    echo Fehler beim Kopieren des Arduino-Verzeichnisses
    pause
    exit /b 1
)

echo Setup erfolgreich abgeschlossen!
pause
