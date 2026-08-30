// Globale Variablen für Upload-Status
let uploadInProgress = false;
let lastProgress = 0;

// DOM Elemente
const updateForm = document.getElementById('update-form');
const updateFile = document.getElementById('update-file');
const progressContainer = document.getElementById('progress');
const statusContainer = document.getElementById('status');
const md5Input = document.getElementById('md5-input');

// Event Listener für das Formular
updateForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    if (uploadInProgress) {
        showStatus('Upload bereits in Bearbeitung', 'warning');
        return;
    }
    startUpdate();
});

// Hauptfunktion für den Update-Prozess
async function startUpdate() {
    const file = updateFile.files[0];
    if (!file) {
        showStatus('Bitte wähle eine Datei aus', 'error');
        return;
    }

    // Validiere Dateiname und bestimme Update-Typ
    const isFileSystem = file.name.toLowerCase().includes('littlefs');
    const isFirmware = file.name.toLowerCase().includes('firmware');

    if (!isFileSystem && !isFirmware) {
        showStatus('Ungültiger Dateiname - muss "firmware.bin" oder "littlefs.bin" enthalten', 'error');
        return;
    }

    try {
        // Zustand VOR dem Neustart merken - daran erkennt DeviceWait später,
        // ob das Gerät wirklich neu gestartet ist oder nur noch kurz auf der
        // alten Instanz antwortet.
        const baseline = await DeviceWait.getStatus();

        // Setze Update-Flags
        showStatus('Setze Update-Flags...', 'info');

        try {
            const flagsResponse = await fetch('/admin/config/update', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-Requested-With': 'XMLHttpRequest'
                },
                body: JSON.stringify({
                    isFileSystemUpdatePending: isFileSystem,
                    isFirmwareUpdatePending: isFirmware,
                    inUpdateMode: true
                })
            });

            if (!flagsResponse.ok) {
                console.warn('Update flags response not OK, but continuing (device may be rebooting)');
            }
        } catch (flagsError) {
            // Expected: Device reboots immediately after setting flags, so connection may be closed
            console.log('Update flags request failed (expected if device is rebooting):', flagsError.message);
        }

        // Warten, bis das Gerät neu gestartet UND im Update-Modus ist. Die
        // feste Wartezeit von vorher steckt jetzt in settleMs.
        showStatus('Warte, bis Gerät in den Update-Modus wechselt...', 'info');
        const entered = await DeviceWait.waitForDevice({
            // Kein Neustartnachweis: inUpdateMode ist selbst der Zustand, auf
            // den gewartet wird. Beim Dateisystem-Update sichert
            // setUpdateFlags() vorher den Flash und antwortet dabei fast eine
            // Minute nicht - das Gerät ist dann längst neu gestartet, bevor
            // hier der erste Poll läuft.
            requireReboot: false,
            until: s => s.inUpdateMode === true,
            timeoutMs: 90000,
            onProgress: otaProgress(0, 20)
        });
        if (!entered.ok) {
            throw new Error('Gerät ist nicht in den Update-Modus gewechselt (90 s)');
        }

        showStatus('Update-Modus aktiv, bereit für Upload', 'success');
        await new Promise(r => setTimeout(r, 500)); // Kurze Pause vor Upload

        // Starte Upload
        uploadInProgress = true;
        showStatus('Starte Upload...', 'info');
        updateProgress(0);

    const formData = new FormData();
    // Use the same field name as the server form input ('firmware')
    formData.append('firmware', file);

        // Füge MD5 hinzu falls vorhanden
        if (md5Input && md5Input.value) {
            formData.append('md5', md5Input.value);
        }

        // Setze Update-Typ
        if (isFileSystem) {
            formData.append('mode', 'fs');
        }

        // If this is a filesystem image, include the mode as a query
        // parameter so the server can detect it immediately during the
        // streaming upload (some servers don't expose multipart form
        // fields until after the file is processed).
        const uploadUrl = isFileSystem ? '/update?mode=fs' : '/update';

        // Create AbortController for timeout handling (5 minutes for large filesystem images)
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 300000); // 5 minutes

        // Ob der Upload geklappt hat, entscheidet NICHT die Antwort auf den
        // Upload: das Gerät setzt sich unmittelbar nach dem Schreiben selbst
        // zurück ("Sofortiger Reset wird erzwungen"), und die Antwort geht
        // dabei oft unterwegs verloren. Am Gerät ließ sich das direkt
        // nachlesen - "Update erfolgreich: 744256 Bytes", danach ein sauberer
        // Reset, während der Browser auf eine Antwort wartete, die nie kam.
        // Früher wurde daraus eine Fehlermeldung, obwohl das Update lief.
        //
        // Entschieden wird stattdessen danach, ob das Gerät wieder hochkommt
        // und den Update-Modus verlassen hat. Ist der Upload wirklich
        // gescheitert, bleibt das Gerät im Update-Modus und die Wartefrist
        // läuft ab - dann ist es ein echter Fehler.
        let result = null;
        try {
            const response = await fetch(uploadUrl, {
                method: 'POST',
                body: formData,
                signal: controller.signal
            });
            clearTimeout(timeoutId);
            if (response.ok) {
                result = await response.json().catch(() => ({ success: true }));
            } else {
                console.log('Upload-Antwort war ' + response.status + ' - Gerät wird trotzdem geprüft');
            }
        } catch (uploadError) {
            clearTimeout(timeoutId);
            if (uploadError.name === 'AbortError') {
                throw new Error('Upload-Timeout (5 Minuten überschritten)');
            }
            console.log('Verbindung beim Upload abgerissen (Gerät setzt sich vermutlich gerade zurück):',
                        uploadError.message);
        }
        result = result || {};

        {
            // Nach dem Upload startet das Gerät neu - beim Dateisystem-Update
            // sogar zweimal (erst der Restore-Boot, dann der Normalboot).
            // Gewartet wird deshalb nicht auf eine Uhr, sondern darauf, dass
            // das Gerät wieder antwortet UND den Update-Modus verlassen hat.
            // Das ist das eigentliche Fertig-Signal: es weist auch ein Gerät
            // zurück, das schon antwortet, aber noch im Update-Modus steht -
            // und deckt den doppelten Neustart damit konstruktiv ab statt
            // zufällig über eine passend gewählte Wartezeit.
            if (result.restorePending) {
                showStatus(result.message || 'Dateisystem aktualisiert, Einstellungen werden wiederhergestellt...', 'info');
            } else {
                showStatus('Upload abgeschlossen, warte auf Neustart...', 'info');
            }
            updateProgress(70);

            const done = await DeviceWait.waitForDevice({
                // Hier ist der Neustart Pflicht: das Gerät wird gerade neu
                // beschrieben und geht dabei sicher weg. Der Basiswert ist der
                // Zustand aus dem Update-Modus - ohne ihn würde schon ein
                // einsekündiger Netzaussetzer als "wieder da" durchgehen und
                // ein fehlgeschlagenes Update als Erfolg gemeldet.
                baseline: entered.status,
                until: s => !s.inUpdateMode,
                timeoutMs: isFileSystem ? 180000 : 120000,
                onProgress: otaProgress(70, 99)
            });

            if (done.ok) {
                updateProgress(100);
                showStatus('Update abgeschlossen, Gerät ist wieder da.', 'success');
                window.location.href = '/';
            } else {
                showStatus('Das Gerät hat sich nicht zurückgemeldet und den Update-Modus '
                         + 'nicht verlassen - das Update ist vermutlich fehlgeschlagen. '
                         + 'Prüfe die auf dem Display angezeigte IP-Adresse; das Gerät '
                         + 'bleibt über /admin/update erreichbar, um es erneut zu '
                         + 'versuchen.', 'error');
            }
        }

    } catch (error) {
        showStatus('Fehler: ' + error.message, 'error');
        console.error('Update error:', error);
        uploadInProgress = false;
    }
}

// Hilfsfunktionen für die UI
function showStatus(message, type = 'info') {
    const statusDiv = document.getElementById('status');
    if (statusDiv) {
        statusDiv.className = 'status-' + type;
        statusDiv.textContent = message;
    }
    console.log(`${type.toUpperCase()}: ${message}`);
}

function updateProgress(percent) {
    const progressDiv = document.getElementById('progress');
    if (progressDiv) {
        progressDiv.style.width = percent + '%';
        progressDiv.textContent = percent + '%';
    }
}

// Event Listener für den Datei-Input
updateFile.addEventListener('change', function(e) {
    const file = e.target.files[0];
    if (!file) return;

    // Aktualisiere UI basierend auf Dateityp
    const isFileSystem = file.name.toLowerCase().includes('littlefs');
    const isFirmware = file.name.toLowerCase().includes('firmware');

    if (!isFileSystem && !isFirmware) {
        showStatus('Warnung: Unerwarteter Dateiname', 'warning');
    } else {
        showStatus(
            `${isFileSystem ? 'Filesystem' : 'Firmware'} Update ausgewählt: ${file.name} (${formatBytes(file.size)})`,
            'info'
        );
    }
});

// Hilfsfunktion zum Formatieren der Dateigröße
function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(decimals)) + ' ' + sizes[i];
}

// Event Listener für XHR Upload Progress
if (window.XMLHttpRequest) {
    const oldXHR = window.XMLHttpRequest;
    function newXHR() {
        const xhr = new oldXHR();
        xhr.addEventListener('progress', function(e) {
            if (e.lengthComputable && uploadInProgress) {
                const percentComplete = (e.loaded / e.total) * 100;
                if (percentComplete !== lastProgress) {
                    lastProgress = percentComplete;
                    updateProgress(Math.round(percentComplete));
                }
            }
        });
        return xhr;
    }
    window.XMLHttpRequest = newXHR;
}

/**
 * Bildet die Phasen von DeviceWait.waitForDevice auf die vorhandene
 * Fortschrittsanzeige der OTA-Seite ab (#status und #progress).
 * @param {number} from Balkenstand zu Beginn des Wartens
 * @param {number} to   Balkenstand am Ende der Wartefrist
 */
function otaProgress(from, to) {
    return function (p) {
        showStatus(DeviceWait.phaseText(p), p.phase === 'ready' ? 'success' : 'info');
        const frac = Math.min(p.elapsedMs / p.timeoutMs, 1);
        updateProgress(from + Math.round(frac * (to - from)));
    };
}
