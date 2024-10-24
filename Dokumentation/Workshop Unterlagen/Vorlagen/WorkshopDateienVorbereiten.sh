#!/usr/bin/env bash

# Setze Fehlererkennung
set -e

# Funktion für Fehlerbehandlung
error_handler() {
    echo "Fehler in Zeile $1"
    exit 1
}

trap 'error_handler ${LINENO}' ERR

# Hilfsfunktion zum Prüfen von Abhängigkeiten
check_dependency() {
    local cmd=$1
    local install_msg=$2
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Fehler: '$cmd' wurde nicht gefunden!"
        echo "Bitte installiere $cmd mit:"
        echo "$install_msg"
        exit 1
    fi
}

# Prüfe Abhängigkeiten
check_dependency "zip" "sudo apt-get install zip (Ubuntu/Debian)\nsudo pacman -S zip (Arch Linux)\nsudo dnf install zip (Fedora)"
check_dependency "jq" "sudo apt-get install jq (Ubuntu/Debian)\nsudo pacman -S jq (Arch Linux)\nsudo dnf install jq (Fedora)"
check_dependency "pio" "python3 -c \"\$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)\""

# Bestimme den Pfad zum Projektroot
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Prüfe Projektstruktur
if [ ! -d "$PROJECT_ROOT/Pflanzensensor" ]; then
    echo "Fehler: Pflanzensensor-Verzeichnis nicht gefunden!"
    echo "Bitte stelle sicher, dass das Skript in 'Dokumentation/Workshop Unterlagen/Vorlagen/' liegt."
    exit 1
fi

# Funktion zum Erstellen von Verzeichnissen für beide Workshop-Varianten
create_workshop_dirs() {
    local base_dir=$1
    for days in 2 3; do
        mkdir -p "../$days Tage/Workshopdateien/$base_dir"
    done
}

# Erstelle Basisverzeichnisse
create_workshop_dirs "Arduino"

# Funktion zum Kopieren des Pflanzensensor-Verzeichnisses
copy_pflanzensensor() {
    local target_dir=$1

    rsync -a --exclude 'passwoerter.cpp' "$PROJECT_ROOT/Pflanzensensor/" "$target_dir"

    if [ -f "$PROJECT_ROOT/Pflanzensensor/passwoerter.cpp-Beispiel" ]; then
        cp "$PROJECT_ROOT/Pflanzensensor/passwoerter.cpp-Beispiel" "$target_dir/passwoerter.cpp"
        rm "$target_dir/passwoerter.cpp-Beispiel"
        echo "- passwoerter.cpp-Beispiel wurde zu passwoerter.cpp umbenannt"
    else
        echo "Warnung: passwoerter.cpp-Beispiel nicht gefunden!"
    fi
}

# Kopiere Pflanzensensor und Programmieraufgaben
echo "Kopiere Projektdateien..."
for days in 2 3; do
    copy_pflanzensensor "../$days Tage/Workshopdateien/Arduino/Pflanzensensor"
    cp -r "./Programmieraufgaben $days Tage/"* "../$days Tage/Workshopdateien/Arduino/"
done

# Funktion zum Installieren einer Bibliothek
install_library() {
    local lib_name="$1"
    local target_dir="$2"

    echo "Installiere: $lib_name in $target_dir"
    lib_name=$(echo "$lib_name" | tr -d '"')

    if ! pio pkg install --global --library "$lib_name" --storage-dir "$target_dir"; then
        echo "Warnung: Installation von $lib_name fehlgeschlagen"
        return 1
    fi

    local base_name=$(echo "$lib_name" | sed -E 's/@.*$//' | sed 's#.*/##')
    echo "✓ $base_name erfolgreich installiert in $target_dir"
    return 0
}

# Prüfe und verarbeite platformio.ini
if [ ! -f "$PROJECT_ROOT/platformio.ini" ]; then
    echo "Fehler: platformio.ini nicht gefunden in $PROJECT_ROOT!"
    exit 1
fi

# Installiere Libraries
echo "Installiere Libraries..."
create_workshop_dirs "Arduino/libraries"

while IFS= read -r line || [[ -n "$line" ]]; do
    line=$(echo "$line" | sed 's/^[ \t]*//')
    if [[ $line =~ ^lib_deps[[:space:]]*= ]]; then
        while IFS= read -r lib || [[ -n "$lib" ]]; do
            lib=$(echo "$lib" | sed 's/^[ \t]*//')
            [[ $lib =~ ^\[.*\] ]] && break
            [[ -z "$lib" || $lib == \;* ]] && continue

            if [ ! -z "$lib" ]; then
                for days in 2 3; do
                    target_dir="../$days Tage/Workshopdateien/Arduino/libraries"
                    install_library "$lib" "$target_dir"
                done
            fi
        done
    fi
done < "$PROJECT_ROOT/platformio.ini"

# Kopiere zusätzliche Dateien
echo "Kopiere zusätzliche Dateien..."
for days in 2 3; do
    cp "./KopiereDateien.ps1" "../$days Tage/Workshopdateien/"
    cp "./Handout Pflanzensensorworkshop $days Tage.pdf" "../$days Tage/"
    cp "./Spickzettel C Programmierung.pdf" "../$days Tage/"
done

# VSIX-Datei herunterladen
echo "Lade VSIX-Datei herunter..."
REPO="earlephilhower/arduino-littlefs-upload"
LATEST_RELEASE=$(curl --silent "https://api.github.com/repos/$REPO/releases/latest")
VSIX_URL=$(echo "$LATEST_RELEASE" | jq -r '.assets[] | select(.name | endswith(".vsix")) | .browser_download_url')

if [[ -n "$VSIX_URL" ]]; then
    curl -L -o "arduino-littlefs-upload.vsix" "$VSIX_URL"
    cp "arduino-littlefs-upload.vsix" "../2 Tage/Workshopdateien/"
    mv "arduino-littlefs-upload.vsix" "../3 Tage/Workshopdateien/"
else
    echo "Fehler: Keine VSIX-Datei gefunden!"
    exit 1
fi

# Dateien prüfen und Archive erstellen
echo "Prüfe Dateien und erstelle Archive..."
declare -a paths_to_check=(
    "../2 Tage/Workshopdateien/Arduino/Pflanzensensor"
    "../3 Tage/Workshopdateien/Arduino/Pflanzensensor"
    "../2 Tage/Workshopdateien/Arduino/libraries"
    "../3 Tage/Workshopdateien/Arduino/libraries"
    "../2 Tage/Workshopdateien/KopiereDateien.ps1"
    "../3 Tage/Workshopdateien/KopiereDateien.ps1"
    "../2 Tage/Handout Pflanzensensorworkshop 2 Tage.pdf"
    "../3 Tage/Handout Pflanzensensorworkshop 3 Tage.pdf"
)

for path in "${paths_to_check[@]}"; do
    [ ! -e "$path" ] && { echo "Fehler: $path nicht gefunden!"; exit 1; }
done

# Archive erstellen
for days in 2 3; do
    (cd "../$days Tage/Workshopdateien" && \
     zip -r Arduino.zip Arduino/ && \
     rm -rf Arduino && \
     echo "Arduino.zip für $days-Tage-Workshop erstellt")
done

echo "Workshop-Dateien wurden erfolgreich vorbereitet!"
