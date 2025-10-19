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
        // Setze Update-Flags
        showStatus('Setze Update-Flags...', 'info');
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
            throw new Error(`Failed to set update flags: ${flagsResponse.statusText}`);
        }

    // Warte bis Gerät im Update-Modus ist (poll /status). Wenn das Gerät
    // etwas länger zum Umbau in den Update-Modus braucht, vermeiden wir
    // so ein vorzeitiges Abbrechen des Uploads. Erhöhe Timeout auf 30s.
    showStatus('Warte, bis Gerät in den Update-Modus wechselt...', 'info');
    const waitOk = await waitForUpdateMode(60000, 500);
        if (!waitOk) {
            throw new Error('Timeout waiting for device to enter update mode');
        }

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
        const response = await fetch(uploadUrl, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            throw new Error(`Upload failed: ${response.statusText}`);
        }

        showStatus('Update erfolgreich! Gerät startet neu...', 'success');
        updateProgress(100);

        // Warte auf Neustart und Redirect
        setTimeout(() => {
            window.location.href = '/';
        }, 10000);

    } catch (error) {
        showStatus('Fehler: ' + error.message, 'error');
        console.error('Update error:', error);
        uploadInProgress = false;
    }
}

// Poll /status until device reports inUpdateMode or timeout.
// timeoutMs: maximum time to wait, intervalMs: poll interval
async function waitForUpdateMode(timeoutMs = 15000, intervalMs = 500) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        try {
            const resp = await fetch('/status');
            if (resp.ok) {
                const json = await resp.json().catch(() => null);
                if (json && json.inUpdateMode) return true;
            }
        } catch (e) {
            // ignore network errors while waiting
        }
        await new Promise(r => setTimeout(r, intervalMs));
    }
    return false;
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
