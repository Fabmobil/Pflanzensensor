Sensoreinstellungsseite — Erklärung (deutsch, kompakt)

Diese Datei beschreibt die Sensoreinstellungsseite des Pflanzensensors. Sie erklärt die sichtbaren Bereiche, Formularelemente und ihr Verhalten, damit Konfiguration und Kalibrierung korrekt durchgeführt werden können.

1) Gesicht der Blume (Auswahl)
- Zweck: Wähle den Sensor, der das "Gesicht der Blume" auf der Startseite steuert (visuelle Anzeige).
- Eingabe: Dropdown mit verfügbaren Sensoren. Auswahl wirkt sofort nach dem Speichern der Einstellungen.

2) LED-Ampel Einstellungen
- LED-Ampel Modus: Auswahlfeld mit mehreren Modi (z. B. komplette Anzeige, nur ausgewählte Messung, aus).
- Ausgewählte Messung: Dropdown, welches die spezifische Messung auswählt, die im Ampelmodus gezeigt werden soll.
- Hinweise: Bei Änderung gilt die Einstellung nach Speichern; die LED-Anzeige wird entsprechend der aktuellen Messwerte aktualisiert.

3) Sensor-Konfiguration (pro Sensorblock)
Jeder Sensorblock enthält die wichtigsten Konfigurationsfelder:
- Messintervall: Anzahl Sekunden zwischen automatischen Messungen. Direkter "Messen"-Button löst eine sofortige Messung aus.
- Sensorname: Freier Text zur leichteren Identifikation.
- Skala invertieren: Checkbox. Bei aktiver Invertierung werden hohe Rohwerte als niedrige Prozentwerte interpretiert (nützlich für bestimmte Sensorcharakteristiken).

3.1) Letzter Messwert & Min/Max
- Letzter Messwert: Anzeige des zuletzt gemessenen Wertes und evtl. Fehlercode.
- Min / Max Felder: Angezeigte berechnete minimal/maximal-Werte basierend auf Messhistorie; "Zurücksetzen" entfernt die aktuellen Min/Max-Werte.

3.2) Schwellwerte
- Gelb min / Grün min / Grün max / Gelb max: Prozentuale Schwellenwerte zur Einordnung in Ampelfarben. Werden für Anzeige (Ampel, Startseite) und Alarmierung verwendet.
- UI: Schwellenwerte können per Eingabe oder Slider gesetzt werden; die Visualisierung zeigt die aktuelle Position relativ zu den Grenzwerten.

3.3) Rohwerte Berechnungslimits
- Min / Letzter / Max: Rohwerte in Sensoreinheit (z. B. ADC-Werte). Diese Limits begrenzen die Umrechnung zu Prozentwerten.
- Autokalibrierung aktivieren: Wenn aktiv, passt das Gerät die Min/Max automatisch an beobachtete Rohwerte an (vorsichtig verwenden).

3.4) Rohwerte Extremesswerte
- Min / Max: Härtere Grenzen für Rohwerte; werden zur Validierung und Filterung extremer Messwerte genutzt.

4) Mehrere Sensoren: Jeder sensorrelevante Abschnitt wiederholt die oben genannten Felder für unterschiedliche physische oder virtuelle Sensoren (z. B. Lichtstärke, Bodenfeuchte).

5) Bedienungs- und Fehlerhinweise
- Speicher- und Validierungsfehler: Felder zeigen Fehlerstatus an (z. B. ungültige Werte, fehlende Berechtigungen).
- Fehlercodes: Bei Messfehlern wird ein Fehlercode angezeigt; Logging mittels Debug-Einstellungen hilft bei der Ursachenanalyse.
- RAM/Performance: Häufige Messintervalle und aktives Debugging erhöhen Speicher- und CPU-Last.

6) Empfehlungen
- Vor dem Aktivieren von Autokalibrierung: Erst manuell Kalibrieren und prüfen, dass Rohwerte innerhalb erwarteter Bereiche liegen.
- Schwellenwerte: Zuerst konservative Werte setzen, beobachten und dann feinjustieren.
- Backup: Konfiguration immer exportieren bevor größere Änderungen (z. B. Reset, Firmware-Update).

7) Kurze Checkliste für typische Aufgaben
- Sensor hinzufügen/umbenennen: Sensorname eintragen → Speichern → optional messen.
- Schwellen anpassen: Werte ändern → Speichern → reale Messung beobachten.
- Autokalibrierung testen: Aktivieren → mehrere Messungen durchführen → prüfen ob Min/Max plausibel sind → ggf. deaktivieren.

---

Wenn du möchtest, füge ich gern Beispiel-Screenshots, typische JSON-Export-Beispiele oder eine Schritt-für-Schritt-Anleitung für das Kalibrieren eines Bodensensors hinzu.
Admin-Seite — Kurzbeschreibung

Diese Seite ermöglicht die Konfiguration und Verwaltung des Geräts über das Webinterface. Kurz und bündig sind hier die wichtigsten Bereiche und Funktionen erklärt.

Systemeinstellungen
- Gerätename: Anzeigen und ändern.
- MD5-Prüfung für Updates: Optional aktivieren, damit hochgeladene Firmware vor der Installation geprüft wird.
- Administrator-Passwort: Neues Passwort setzen und speichern.

Systemaktionen
- Einstellungen zurücksetzen: Setzt alle Konfigurationen auf Werkseinstellungen zurück.
- Neustart durchführen: Gerät neu starten.
- Einstellungen herunterladen: Aktuelle Konfigurationsdateien vom Gerät herunterladen.
- Sensordaten herunterladen: Gespeicherte Messdaten herunterladen.
- Einstellungen oder Sensordaten hochladen: JSON-Datei auswählen und hochladen ("Browse...").

WiFi Einstellungen
- SSID 1–3 und zugehörige Passwörter: Mehrere Netzwerke zur Verfügbarkeit hinterlegen.
- Änderungen speichern: Speichert die WiFi-Konfiguration und versucht, sich mit den angegebenen Netzwerken zu verbinden.

System Information
- Freier Heap, Heap-Fragmentierung, Max. Block-Größe: RAM-Statistiken.
- Laufzeit: Wie lange das Gerät bereits läuft.
- WiFi SSID, Signal, IP-Adresse, MAC-Adresse: Netzwerkstatus.
- Dateisystem Gesamt/Belegt/Frei: LittleFS-Statistiken.

Debug-Einstellungen
- Logs in Datei speichern: Aktiviert persistenten Log-Speicher.
- Debug-Optionen (RAM, Messzyklus, Sensor, Display, WebSocket): Feinkörnige Log-Kanäle ein-/ausschalten.
- Log Level: Wählt das gewünschte Logging-Level (z. B. DEBUG, INFO, WARN, ERROR).

Navigation & Footer
- Seitenlinks: START, LOGS, ADMIN, sowie Unterseiten: Einstellungen, Sensoren, Display, Update.
- Footer zeigt Firmware-Version und Build-Datum.

Hinweise
- Änderungen an kritischen Einstellungen (z. B. Firmware-Update, Zurücksetzen) können einen Neustart oder Verlust von Konfigurationen bewirken.
- Bei WiFi-Problemen bitte SSID/Passwort prüfen oder Gerät im AP-Modus neu verbinden.

Kurzfassung
Die Admin-Seite bündelt alle Konfigurations- und Wartungsfunktionen: Geräte- und WiFi-Einstellungen, Systemoperationen, Statusinformationen und detaillierte Debug-Optionen. Ideal für lokale Wartung und Remote-Diagnose.
