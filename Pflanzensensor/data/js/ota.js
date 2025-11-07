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

        // Parse response to check for restore pending
        const result = await response.json().catch(() => ({ success: true }));

        if (result.restorePending) {
            // Update progress to 40% when upload complete
            updateProgress(40);
            showStatus('Filesystem aktualisiert...', 'success');

            // Show restore pending message with countdown in status area
            // Device will reboot twice - first to restore config, then normal boot
            setTimeout(() => {
                updateProgress(70);
                showRestoreCountdown(25, result.message || 'Einstellungen werden nach Neustart wiederhergestellt.');
            }, 500);
        } else {
            // Standard success handling
            showStatus('Update erfolgreich! Gerät startet neu...', 'success');
            updateProgress(100);

            // Warte auf Neustart und Redirect
            setTimeout(() => {
                window.location.href = '/';
            }, 10000);
        }

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

// Show restore countdown in status area (not modal)
function showRestoreCountdown(seconds, message) {
    const statusDiv = document.getElementById('status');
    if (!statusDiv) return;

    let remaining = seconds;

    const updateCountdown = () => {
        statusDiv.className = 'status-success';
        statusDiv.innerHTML = `
            <strong>Einstellungen werden wiederhergestellt</strong><br>
            ${message}<br><br>
            <strong style="font-size:1.2em">Neustart in ${remaining} Sekunden...</strong>
        `;
    };

    updateCountdown();

    const interval = setInterval(() => {
        remaining--;
        if (remaining <= 0) {
            clearInterval(interval);
            statusDiv.innerHTML = 'Lade neu...';
            window.location.href = '/';
            return;
        }
        updateCountdown();
    }, 1000);
}

    // --- Old modal version (kept for compatibility but not used) ---
    function showRestoreModal(seconds, message) {
        // Create modal elements if not present
        let modal = document.getElementById('restore-modal');
        if (!modal) {
            modal = document.createElement('div');
            modal.id = 'restore-modal';
            modal.style.position = 'fixed';
            modal.style.left = '0';
            modal.style.top = '0';
            modal.style.width = '100%';
            modal.style.height = '100%';
            modal.style.background = 'rgba(0,0,0,0.6)';
            modal.style.display = 'flex';
            modal.style.alignItems = 'center';
            modal.style.justifyContent = 'center';
            modal.style.zIndex = 9999;

            const box = document.createElement('div');
            box.id = 'restore-box';
            box.style.background = '#fff';
            box.style.color = '#000';
            box.style.padding = '20px';
            box.style.borderRadius = '8px';
            box.style.maxWidth = '480px';
            box.style.textAlign = 'center';
            box.style.boxShadow = '0 8px 24px rgba(0,0,0,0.3)';

            const h = document.createElement('h3');
            h.textContent = 'Einstellungen werden wiederhergestellt';
            box.appendChild(h);

            const p = document.createElement('p');
            p.id = 'restore-message';
            p.style.whiteSpace = 'pre-wrap';
            box.appendChild(p);

            const counter = document.createElement('div');
            counter.id = 'restore-counter';
            counter.style.fontSize = '20px';
            counter.style.marginTop = '12px';
            box.appendChild(counter);

            modal.appendChild(box);
            document.body.appendChild(modal);
        }

        document.getElementById('restore-message').textContent = message;
        let remaining = seconds;
        const counterEl = document.getElementById('restore-counter');
        counterEl.textContent = `Neustart in ${remaining} Sekunden...`;

        const interval = setInterval(() => {
            remaining--;
            if (remaining <= 0) {
                clearInterval(interval);
                // Remove modal and reload start page
                const m = document.getElementById('restore-modal');
                if (m) m.parentNode.removeChild(m);
                window.location.href = '/';
                return;
            }
            counterEl.textContent = `Neustart in ${remaining} Sekunden...`;
        }, 1000);
    }
