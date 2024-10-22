#!/usr/bin/env bash

# Setze Fehlererkennung
set -e

# Funktion für Fehlerbehandlung
error_handler() {
    echo "Fehler in Zeile $1"
    exit 1
}

trap 'error_handler ${LINENO}' ERR

# Prüfe ob zip installiert ist
if ! command -v zip >/dev/null 2>&1; then
    echo "Fehler: 'zip' wurde nicht gefunden!"
    echo "Bitte installiere zip mit:"
    echo "sudo apt-get install zip (Ubuntu/Debian)"
    echo "sudo pacman -S zip (Arch Linux)"
    echo "sudo dnf install zip (Fedora)"
    exit 1
fi

# Bestimme den Pfad zum Projektroot (3 Verzeichnisse nach oben von der Skriptposition)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Prüfe ob wir uns im richtigen Verzeichnis befinden
if [ ! -d "$PROJECT_ROOT/Pflanzensensor" ]; then
    echo "Fehler: Pflanzensensor-Verzeichnis nicht gefunden!"
    echo "Bitte stelle sicher, dass das Skript in 'Dokumentation/Workshop Unterlagen/Vorlagen/' liegt."
    exit 1
fi

# Erstelle die Zielverzeichnisse
echo "Erstelle Zielverzeichnisse..."
mkdir -p "../2 Tage/Workshopdateien/Arduino"
mkdir -p "../3 Tage/Workshopdateien/Arduino"
mkdir -p "../2 Tage"
mkdir -p "../3 Tage"

echo "Kopiere Dateien..."

# Funktion zum Kopieren des Pflanzensensor-Verzeichnisses mit spezieller Behandlung der passwoerter.cpp
copy_pflanzensensor() {
    local target_dir=$1
    
    # Kopiere zuerst alles außer passwoerter.cpp
    rsync -a --exclude 'passwoerter.cpp' "$PROJECT_ROOT/Pflanzensensor/" "$target_dir"
    
    # Kopiere und benenne passwoerter.cpp-Beispiel um
    if [ -f "$PROJECT_ROOT/Pflanzensensor/passwoerter.cpp-Beispiel" ]; then
        cp "$PROJECT_ROOT/Pflanzensensor/passwoerter.cpp-Beispiel" "$target_dir/passwoerter.cpp"
        rm "$target_dir/passwoerter.cpp-Beispiel"
        echo "- passwoerter.cpp-Beispiel wurde zu passwoerter.cpp umbenannt"
    else
        echo "Warnung: passwoerter.cpp-Beispiel nicht gefunden!"
    fi
}

# Kopiere Pflanzensensor mit spezieller Behandlung
echo "- Kopiere Pflanzensensor..."
copy_pflanzensensor "../2 Tage/Workshopdateien/Arduino/Pflanzensensor"
copy_pflanzensensor "../3 Tage/Workshopdateien/Arduino/Pflanzensensor"

# Kopiere Programmieraufgaben
echo "- Kopiere Programmieraufgaben..."
cp -r "./Programmieraufgaben 2 Tage/"* "../2 Tage/Workshopdateien/Arduino/"
cp -r "./Programmieraufgaben 3 Tage/"* "../3 Tage/Workshopdateien/Arduino/"

# Kopiere libraries
echo "- Kopiere Libraries..."
cp -r "./libraries" "../2 Tage/Workshopdateien/Arduino/"
cp -r "./libraries" "../3 Tage/Workshopdateien/Arduino/"

# Kopiere KopiereDateien.bat
echo "- Kopiere KopiereDateien.bat..."
cp "./KopiereDateien.bat" "../2 Tage/Workshopdateien/"
cp "./KopiereDateien.bat" "../3 Tage/Workshopdateien/"

# Kopiere Handouts und Spickzettel
echo "- Kopiere Handouts und Spickzettel..."
cp "./Handout Pflanzensensorworkshop 2 Tage.pdf" "../2 Tage/"
cp "./Spickzettel C Programmierung.pdf" "../2 Tage/"
cp "./Handout Pflanzensensorworkshop 3 Tage.pdf" "../3 Tage/"
cp "./Spickzettel C Programmierung.pdf" "../3 Tage/"

# Kopiere VSIX-Datei
echo "- Kopiere VSIX-Datei..."
cp "./arduino-littlefs-upload"*.vsix "../2 Tage/Workshopdateien/"
cp "./arduino-littlefs-upload"*.vsix "../3 Tage/Workshopdateien/"

# Prüfe ob alle Dateien erfolgreich kopiert wurden
echo "Prüfe die kopierten Dateien..."

# Array mit Prüfpfaden
declare -a paths_to_check=(
    "../2 Tage/Workshopdateien/Arduino/Pflanzensensor"
    "../3 Tage/Workshopdateien/Arduino/Pflanzensensor"
    "../2 Tage/Workshopdateien/Arduino/libraries"
    "../3 Tage/Workshopdateien/Arduino/libraries"
    "../2 Tage/Workshopdateien/KopiereDateien.bat"
    "../3 Tage/Workshopdateien/KopiereDateien.bat"
    "../2 Tage/Handout Pflanzensensorworkshop 2 Tage.pdf"
    "../3 Tage/Handout Pflanzensensorworkshop 3 Tage.pdf"
)

# Prüfe jeden Pfad
for path in "${paths_to_check[@]}"; do
    if [ ! -e "$path" ]; then
        echo "Fehler: $path wurde nicht korrekt kopiert!"
        exit 1
    fi
done

echo "Alle Dateien wurden erfolgreich kopiert!"

# Funktion zum Archivieren und Aufräumen
create_archive() {
    local days=$1
    cd "../$days Tage/Workshopdateien"
    
    echo "Erstelle ZIP-Archiv für $days-Tage-Workshop..."
    zip -r Arduino.zip Arduino/
    
    if [ $? -eq 0 ]; then
        echo "- Arduino.zip für $days-Tage-Workshop erstellt"
        rm -rf Arduino
    else
        echo "Fehler beim Erstellen von Arduino.zip für $days-Tage-Workshop!"
        exit 1
    fi
    cd "$SCRIPT_DIR"
}

# Erstelle Archive für beide Workshops
create_archive "2"
create_archive "3"

echo "Workshop-Dateien wurden erfolgreich vorbereitet!"
echo "Die Arduino-Verzeichnisse wurden in ZIP-Archive gepackt und die Originalverzeichnisse wurden gelöscht."

