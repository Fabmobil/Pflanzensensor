#!/usr/bin/env bash

# Absoluten Pfad zum rsync-Befehl verwenden
RSYNC="rsync"  # für NixOS

# Zielpfad auf dem NAS
ZIEL="fabmobil-nas-root:/daten/fabmobil/Vorlagen/Pflanzensensor"

# Sicherstellen, dass das Verzeichnis existiert:
echo "Erstelle Verzeichnisse auf dem NAS..."
ssh fabmobil-nas-root "mkdir -p /daten/fabmobil/Vorlagen/Pflanzensensor/\"2 Tage\" /daten/fabmobil/Vorlagen/Pflanzensensor/\"3 Tage\"" || {
    echo "Fehler beim Erstellen der Verzeichnisse auf dem NAS"
    exit 1
}


# Kopieren mit vollem Pfad zu rsync
echo "Starte Kopiervorgang..."
"$RSYNC" -avz "../2 Tage/" "$ZIEL/2 Tage/" || {
    echo "Fehler beim Kopieren von '../2 Tage'"
    exit 1
}

"$RSYNC" -avz "../3 Tage/" "$ZIEL/3 Tage/" || {
    echo "Fehler beim Kopieren von '../3 Tage'"
    exit 1
}

# SSH-Verbindung zum NAS aufbauen und Schreibschutz setzen
ssh fabmobil-nas-root "chmod -R a-w /daten/fabmobil/Vorlagen/Pflanzensensor" || {
    echo "Fehler beim Setzen des Schreibschutzes"
    exit 1
}

echo "Backup und Schreibschutz abgeschlossen"
