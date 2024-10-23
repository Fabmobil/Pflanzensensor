#!/usr/bin/env bash

# Zielpfad auf dem NAS
ZIEL="fabmobil-nas-root:/daten/fabmobil/Vorlagen/Pflanzensensor/"

# Kopieren der Verzeichnisse mit rsync
# -a für Archive-Modus (erhält Berechtigungen etc.)
# -v für verbose Output
# -z für Komprimierung während der Übertragung
rsync -avz "../2 Tage" "$ZIEL"
rsync -avz "../3 Tage" "$ZIEL"

# SSH-Verbindung zum NAS aufbauen und Schreibschutz setzen
# -R für rekursiv
ssh fabmobil-nas-root "chmod -R a-w /daten/fabmobil/Vorlagen/Pflanzensensor"

echo "Backup und Schreibschutz abgeschlossen"
