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
# Prüfe ob platformio cli installiert ist
if ! command -v pio >/dev/null 2>&1; then
    echo "Fehler: 'platformio' wurde nicht gefunden!"
    echo "Bitte installiere PlatformIO Core (CLI) mit:"
    echo "python3 -c \"$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)\""
    exit 1
fi

echo "- Lade Libraries herunter..."

# Erstelle temporäre Verzeichnisse für die Libraries
mkdir -p "$PROJECT_ROOT/temp_libs_2_tage"
mkdir -p "$PROJECT_ROOT/temp_libs_3_tage"

# Lese platformio.ini aus
if [ ! -f "$PROJECT_ROOT/platformio.ini" ]; then
    echo "Fehler: platformio.ini nicht gefunden in $PROJECT_ROOT!"
    exit 1
fi

# Erstelle Zielverzeichnisse
mkdir -p "../2 Tage/Workshopdateien/Arduino/libraries"
mkdir -p "../3 Tage/Workshopdateien/Arduino/libraries"

# Extrahiere Bibliotheken mit verbesserter Methode
while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ $line == *"lib_deps"* ]]; then
        # Entferne alles vor dem = und Leerzeichen
        lib_line="${line#*=}"
        lib_line="$(echo "$lib_line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"

        for target in "2" "3"; do
            target_dir="../${target} Tage/Workshopdateien/Arduino/libraries"

            if [[ ! -z "$lib_line" ]]; then
                echo "Installiere Bibliothek für ${target}-Tage-Workshop: $lib_line"
                # Führe pio install in einer subshell aus, um Fehler abzufangen
                if ! (pio pkg install --no-save "$lib_line" --global-lib --storage-dir "$target_dir"); then
                    echo "Warnung: Installation von $lib_line fehlgeschlagen"
                    continue
                fi
            fi
        done
    fi
done < "$PROJECT_ROOT/platformio.ini"

echo "Libraries wurden erfolgreich heruntergeladen!"
# Aufräumen
rm -rf "$PROJECT_ROOT/temp_libs_2_tage"
rm -rf "$PROJECT_ROOT/temp_libs_3_tage"

# Prüfe ob Arduino15.zip Dateien bereits existieren
check_arduino15_files() {
    local files_exist=false
    for target in "2" "3"; do
        if [ -f "../${target} Tage/Workshopdateien/Arduino15.zip" ]; then
            files_exist=true
            echo "Arduino15.zip für ${target}-Tage-Workshop existiert bereits"
        fi
    done
    return $([ "$files_exist" = true ] && echo 0 || echo 1)
}

download_esp8266_files() {
    # Prüfe ob jq installiert ist
    if ! command -v jq >/dev/null 2>&1; then
        echo "Fehler: 'jq' wurde nicht gefunden!"
        echo "Bitte installiere jq mit:"
        echo "sudo apt-get install jq (Ubuntu/Debian)"
        echo "sudo pacman -S jq (Arch Linux)"
        echo "sudo dnf install jq (Fedora)"
        exit 1
    fi

    echo "Lade ESP8266 Board-Dateien herunter..."

    # Erstelle temporäres Verzeichnis für Arduino15
    TEMP_ARDUINO15="$PROJECT_ROOT/temp_arduino15"
    mkdir -p "$TEMP_ARDUINO15/packages"

    # Lade package index herunter
    echo "- Lade package_esp8266com_index.json..."
    if ! curl -# -o "$TEMP_ARDUINO15/package_esp8266com_index.json" "http://arduino.esp8266.com/stable/package_esp8266com_index.json"; then
        echo "Fehler: Download des package index fehlgeschlagen!"
        exit 1
    fi

    if [ ! -f "$TEMP_ARDUINO15/package_esp8266com_index.json" ]; then
        echo "Fehler: package_esp8266com_index.json konnte nicht heruntergeladen werden!"
        exit 1
    fi

    # Erstelle Verzeichnisstruktur
    mkdir -p "$TEMP_ARDUINO15/packages/esp8266/hardware/esp8266"
    mkdir -p "$TEMP_ARDUINO15/packages/esp8266/tools"

    # Kopiere die package_esp8266com_index.json in das Arduino15-Verzeichnis
    mkdir -p "$TEMP_ARDUINO15/package_index"
    cp "$TEMP_ARDUINO15/package_esp8266com_index.json" "$TEMP_ARDUINO15/package_index/"

    echo "- Extrahiere URLs aus JSON..."

    # Extrahiere Plattform-URLs (nur die neueste Version)
    PACKAGE_URLS=$(jq -r '.packages[].platforms | sort_by(.version) | reverse | .[0].url' "$TEMP_ARDUINO15/package_esp8266com_index.json")

    # Extrahiere Tool-URLs (nur für das aktuelle System)
    HOST_OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    HOST_ARCH=$(uname -m)
    if [[ "$HOST_ARCH" == "x86_64" ]]; then
        HOST_ARCH="x86_64"
    elif [[ "$HOST_ARCH" == "i686" || "$HOST_ARCH" == "i386" ]]; then
        HOST_ARCH="i686"
    elif [[ "$HOST_ARCH" == "aarch64" || "$HOST_ARCH" == "armv8" ]]; then
        HOST_ARCH="aarch64"
    elif [[ "$HOST_ARCH" == "armv7l" ]]; then
        HOST_ARCH="arm"
    fi

    TOOLS_URLS=$(jq -r --arg os "$HOST_OS" --arg arch "$HOST_ARCH" \
        '.packages[].tools[] | select(.systems[].host | contains($os) and contains($arch)) | .systems[] | select(.host | contains($os) and contains($arch)) | .url' \
        "$TEMP_ARDUINO15/package_esp8266com_index.json")

    if [ -z "$PACKAGE_URLS" ] || [ -z "$TOOLS_URLS" ]; then
        echo "Fehler: Keine URLs in package_esp8266com_index.json gefunden!"
        exit 1
    fi

    echo "- Lade ESP8266 Packages herunter..."
    echo "$PACKAGE_URLS" | while read -r url; do
        if [ -n "$url" ]; then
            filename=$(basename "$url")
            version=$(echo "$filename" | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' || echo "unknown")
            target_dir="$TEMP_ARDUINO15/packages/esp8266/hardware/esp8266/$version"

            echo "  Lade $filename herunter..."
            mkdir -p "$target_dir"
            if curl -# -L "$url" -o "$target_dir.zip"; then
                # Füge -o Flag hinzu um Dateien ohne Nachfrage zu überschreiben
                unzip -o -q "$target_dir.zip" -d "$target_dir"
                rm "$target_dir.zip"
            else
                echo "Warnung: Download von $filename fehlgeschlagen"
            fi
        fi
    done

    echo "- Lade ESP8266 Tools herunter..."
    echo "$TOOLS_URLS" | while read -r url; do
        if [ -n "$url" ]; then
            filename=$(basename "$url")
            tool_name=$(echo "$filename" | grep -o '^[^0-9]*' || echo "unknown")
            version=$(echo "$filename" | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' || echo "unknown")
            target_dir="$TEMP_ARDUINO15/packages/esp8266/tools/$tool_name/$version"

            echo "  Lade $filename herunter..."
            mkdir -p "$target_dir"

            # Prüfe Dateiendung und wähle entsprechende Entpackmethode
            if curl -# -L "$url" -o "$target_dir.archive"; then
                if [[ "$filename" == *.tar.gz ]] || [[ "$filename" == *.tgz ]]; then
                    # Prüfe ob es eine gültige tar.gz Datei ist
                    if gzip -t "$target_dir.archive" 2>/dev/null; then
                        tar -xzf "$target_dir.archive" -C "$target_dir"
                    else
                        echo "Warnung: $filename ist keine gültige tar.gz Datei, versuche als tar-Datei"
                        tar -xf "$target_dir.archive" -C "$target_dir" || echo "Warnung: Entpacken von $filename fehlgeschlagen"
                    fi
                elif [[ "$filename" == *.zip ]]; then
                    unzip -q "$target_dir.archive" -d "$target_dir" || echo "Warnung: Entpacken von $filename fehlgeschlagen"
                else
                    echo "Warnung: Unbekanntes Archivformat für $filename"
                    cp "$target_dir.archive" "$target_dir/"
                fi
                rm "$target_dir.archive"
            else
                echo "Warnung: Download von $filename fehlgeschlagen"
            fi
        fi
    done

    # Erstelle ZIP-Archive für beide Workshop-Varianten
for target in "2" "3"; do
    echo "Erstelle Arduino15.zip für ${target}-Tage-Workshop..."
    # Stelle sicher, dass das Zielverzeichnis existiert
    mkdir -p "../${target} Tage/Workshopdateien"
    # Wechsle ins temporäre Verzeichnis und erstelle das ZIP
    (cd "$TEMP_ARDUINO15" && pwd && zip -r "../Dokumentation/Workshop Unterlagen/${target} Tage/Workshopdateien/Arduino15.zip" .)
done

# Aufräumen
rm -rf "$TEMP_ARDUINO15"

echo "Arduino15 Board-Dateien wurden erfolgreich heruntergeladen und gepackt!"
}

echo "Prüfe auf vorhandene ESP8266 Board-Dateien..."
if check_arduino15_files; then
    read -p "Arduino15.zip Dateien existieren bereits. Neu herunterladen? (j/N) " response
    case $response in
        [jJ]* )
            download_esp8266_files
            ;;
        * )
            echo "Überspringe Download der ESP8266 Board-Dateien..."
            ;;
    esac
else
    download_esp8266_files
fi

# Kopiere KopiereDateien.bat
echo "- Kopiere KopiereDateien.bat..."
cp "./KopiereDateien.ps1" "../2 Tage/Workshopdateien/"
cp "./KopiereDateien.ps1" "../3 Tage/Workshopdateien/"

# Kopiere Handouts und Spickzettel
echo "- Kopiere Handouts und Spickzettel..."
cp "./Handout Pflanzensensorworkshop 2 Tage.pdf" "../2 Tage/"
cp "./Spickzettel C Programmierung.pdf" "../2 Tage/"
cp "./Handout Pflanzensensorworkshop 3 Tage.pdf" "../3 Tage/"
cp "./Spickzettel C Programmierung.pdf" "../3 Tage/"

# Kopiere VSIX-Datei

# Repository-Informationen
REPO="earlephilhower/arduino-littlefs-upload"

# Hole die neueste Release-Information über die GitHub API
LATEST_RELEASE=$(curl --silent "https://api.github.com/repos/$REPO/releases/latest")

# Extrahiere die URL der .vsix Datei
VSIX_URL=$(echo "$LATEST_RELEASE" | jq -r '.assets[] | select(.name | endswith(".vsix")) | .browser_download_url')

# Überprüfen, ob eine URL gefunden wurde
if [[ -n "$VSIX_URL" ]]; then
  echo "Neueste .vsix Datei gefunden: $VSIX_URL"
  # Lade die .vsix Datei herunter
  curl -L -o arduino-littlefs-upload.vsix "$VSIX_URL"
  echo "Download abgeschlossen: arduino-littlefs-upload.vsix"
else
  echo "Keine .vsix Datei gefunden!"
  exit 1
fi
echo "- Kopiere VSIX-Datei..."
cp "./arduino-littlefs-upload.vsix" "../2 Tage/Workshopdateien/"
mv "./arduino-littlefs-upload.vsix" "../3 Tage/Workshopdateien/"

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
