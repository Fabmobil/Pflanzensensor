/**
 * @fileoverview Admin panel functionality
 */

/**
 * Map technical config keys to user-friendly German names
 */
const CONFIG_KEY_NAMES = {
  // Display settings
  'screen_dur': 'Anzeigedauer pro Bildschirm',
  'clock_fmt': 'Uhrzeitformat',
  'show_ip': 'IP-Adresse anzeigen',
  'show_clock': 'Uhrzeit anzeigen',
  'show_flower': 'Blumen-Bild anzeigen',
  'show_fabmobil': 'Fabmobil-Logo anzeigen',

  // General settings
  'device_name': 'Gerätename',
  'admin_pwd': 'Administrator-Passwort',
  'md5_verify': 'MD5-Überprüfung',
  'collectd_enabled': 'InfluxDB/Collectd',
  'file_log': 'Datei-Logging',
  'flower_sens': 'Flower-Status Sensor',

  // WiFi settings
  'ssid1': 'WLAN SSID 1',
  'pwd1': 'WLAN Passwort 1',
  'ssid2': 'WLAN SSID 2',
  'pwd2': 'WLAN Passwort 2',
  'ssid3': 'WLAN SSID 3',
  'pwd3': 'WLAN Passwort 3',

  // Debug settings
  'ram': 'Debug RAM',
  'meas_cycle': 'Debug Messzyklus',
  'sensor': 'Debug Sensor',
  'display': 'Debug Display',
  'websocket': 'Debug WebSocket',

  // Log settings
  'level': 'Log-Level',
  'file_enabled': 'Datei-Logging',

  // LED traffic light
  'mode': 'LED-Ampel Modus',
  'sel_meas': 'LED-Ampel Messung'
};

/**
 * Unified function to set a configuration value
 * @param {string} namespace - The namespace (e.g., "general", "wifi", "display", "debug", "s_ANALOG_1")
 * @param {string} key - The configuration key
 * @param {string|boolean|number} value - The value to set
 * @param {string} type - The type of value ("bool", "int", "uint", "float", "string")
 * @returns {Promise} Promise that resolves when the config is saved
 */
function setConfigValue(namespace, key, value, type) {
  // Convert value to string
  const valueStr = String(value);

  // Create display value for toast message (mask passwords)
  let displayValue = valueStr;
  if (key.includes('pwd') || key.includes('password')) {
    displayValue = '***';
  }

  // Format boolean values in German
  else if (type === 'bool') {
    displayValue = (value === true || value === 'true' || value === '1') ? 'aktiviert' : 'deaktiviert';
  }

  // Format special values with units
  else if (key === 'screen_dur') {
    // The firmware stores screen_dur in milliseconds, but the UI input is in seconds.
    // If value looks like milliseconds (>=1000) convert to seconds for the message.
    const asNumber = Number(value);
    if (!isNaN(asNumber) && asNumber >= 1000) {
      displayValue = (asNumber / 1000) + ' Sekunden';
    } else {
      displayValue = valueStr + ' Sekunden';
    }
  } else if (key === 'clock_fmt') {
    displayValue = valueStr === '24h' ? '24-Stunden' : '12-Stunden';
  }

  // Get user-friendly name for the key
  const friendlyName = CONFIG_KEY_NAMES[key] || key;

  const params = new URLSearchParams({
    namespace: namespace,
    key: key,
    value: valueStr,
    type: type || 'string',
    ajax: '1'
  });

  return fetch('/admin/config/setConfigValue', {
    method: 'POST',
    body: params,
    credentials: 'include',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'X-Requested-With': 'XMLHttpRequest'
    }
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data && data.success) {
      // Show detailed message with friendly name and value
      const message = data.message || `${friendlyName} geändert zu ${displayValue}`;
      showSuccessMessage(message);
      return data;
    } else {
      const errorMsg = (data && (data.error || data.message)) || 'Unbekannter Fehler';
      showErrorMessage('Fehler beim Speichern: ' + errorMsg);
      throw new Error(errorMsg);
    }
  })
  .catch(err => {
    console.error('[admin.js] setConfigValue error:', err);
    if (!err.message.includes('Fehler beim Speichern')) {
      showErrorMessage('Fehler beim Speichern: ' + err.message);
    }
    throw err;
  });
}

function confirmReboot() {
  if(confirm('Gerät wirklich neu starten?')) {
    window.location.href = '/admin/reboot';
  }
  return false;
}

function confirmReset() {
  if(confirm('Wirklich alle Einstellungen zurücksetzen?')) {
    window.location.href = '/admin/reset';
  }
  return false;
}

/**
 * Updates the navigation to show active items and handle dropdowns
 */
function initNavigation() {
  const path = window.location.pathname;

  document.querySelectorAll('.nav-link').forEach(link => {
    if (link.getAttribute('href') === path) {
      link.classList.add('active');
      const parentDropdown = link.closest('.nav-item');
      if (parentDropdown) {
        parentDropdown.classList.add('active');
      }
    }
  });
}

// Add to existing window.onload
window.addEventListener('load', () => {
  initNavigation();
  // No table population or fetch logic needed for /admin/sensors or any admin page
});

// Handler for saving admin password explicitly (does not participate in auto-save)
window.addEventListener('load', () => {
  const saveBtn = document.getElementById('save_admin_password');
  if (!saveBtn) return;

  saveBtn.addEventListener('click', function () {
    const pw = document.getElementById('admin_password');
    const pw2 = document.getElementById('admin_password_confirm');
    if (!pw || !pw2) return;
    const v1 = pw.value || '';
    const v2 = pw2.value || '';

    if (v1.length === 0) {
      showErrorMessage('Bitte ein Passwort eingeben');
      return;
    }
    // Enforce length constraints (match server): min 8, max 32
    if (v1.length < 8 || v1.length > 32) {
      showErrorMessage('Passwortlänge muss zwischen 8 und 32 Zeichen liegen');
      return;
    }
    // ASCII-only check: allow characters 0x20..0x7E
    for (let i = 0; i < v1.length; ++i) {
      const code = v1.charCodeAt(i);
      if (code < 0x20 || code > 0x7E) {
        showErrorMessage('Nur ASCII-Zeichen erlaubt');
        return;
      }
    }
    if (v1 !== v2) {
      showErrorMessage('Passwörter stimmen nicht überein');
      return;
    }

    // Visual feedback
    showSuccessMessage('Speichere Passwort...');

    // Use unified setConfigValue method
    setConfigValue('general', 'admin_pwd', v1, 'string')
      .then(() => {
        showSuccessMessage('Passwort erfolgreich geändert');
        // Clear fields after success
        pw.value = '';
        pw2.value = '';
      })
      .catch(err => {
        console.error('[admin.js] Password save error:', err);
        // Error message already shown by setConfigValue
      });
  });
});

// NOTE: Upload form handling removed - configuration now in Preferences (EEPROM)
// OLD REMOVED: Upload form event listener for upload-config-form

// --- Global AJAX Message Functions (shared across admin pages) ---
function showSuccessMessage(message) {
  let messageElement = document.getElementById('ajax-message');
  if (!messageElement) {
    messageElement = document.createElement('div');
    messageElement.id = 'ajax-message';
    document.body.appendChild(messageElement);
  }
  messageElement.textContent = message;
  messageElement.className = 'ajax-message ajax-message-success';
  messageElement.style.opacity = '1';
  setTimeout(() => { messageElement.style.opacity = '0'; }, 3000);
}

function showErrorMessage(message) {
  let messageElement = document.getElementById('ajax-message');
  if (!messageElement) {
    messageElement = document.createElement('div');
    messageElement.id = 'ajax-message';
    document.body.appendChild(messageElement);
  }
  messageElement.textContent = message;
  messageElement.className = 'ajax-message ajax-message-error';
  messageElement.style.opacity = '1';
  setTimeout(() => { messageElement.style.opacity = '0'; }, 5000);
}

/**
 * Helper to parse JSON responses and surface auth / non-JSON errors
 * Used by admin pages to provide consistent error notifications.
 */
function parseJsonResponse(response) {
  if (response && response.status === 401) {
    showErrorMessage('Authentifizierung erforderlich');
    throw new Error('Unauthorized');
  }

  // Safely retrieve content-type from fetch Response or our XHR-like fallback
  var contentType = '';
  try {
    if (response && response.headers && typeof response.headers.get === 'function') {
      contentType = (response.headers.get('content-type') || '').toLowerCase();
    } else if (response && typeof response.getResponseHeader === 'function') {
      contentType = (response.getResponseHeader('content-type') || '').toLowerCase();
    }
  } catch (e) {
    contentType = '';
  }

  // If server didn't advertise JSON, try to detect JSON-ish body and fail with the raw body shown
  if (!contentType || contentType.indexOf('application/json') === -1) {
    if (typeof response.text === 'function') {
      return response.text().then(function (text) {
        var trimmed = (text || '').trim();
        // If the body appears to be JSON, attempt parse to provide a better error message
        if (trimmed && (trimmed[0] === '{' || trimmed[0] === '[')) {
          try {
            return JSON.parse(text);
          } catch (err) {
            console.error('[ADMIN] JSON.parse failed for response with missing/incorrect Content-Type:', err, text);
            showErrorMessage('Ungültige JSON-Antwort vom Server (siehe Konsole)');
            throw new Error('Ungültige Server-Antwort: ' + (text || '<leer>'));
          }
        }
        throw new Error('Ungültige Server-Antwort: ' + (text || '<leer>'));
      });
    }
    return Promise.reject(new Error('Ungültige Server-Antwort (keine body reader)'));
  }

  // Content-Type indicates JSON — parse robustly, but fall back to text and log raw body on parse errors
  if (typeof response.json === 'function') {
    return response.json().catch(function (err) {
      if (typeof response.text === 'function') {
        return response.text().then(function (text) {
          console.error('[ADMIN] Failed to parse JSON response:', err, 'raw response:', text);
          showErrorMessage('Ungültige JSON-Antwort vom Server (siehe Konsole)');
          throw new Error('JSON parse error: ' + (err && err.message ? err.message : err) + ' | raw: ' + (text || '<leer>'));
        });
      }
      throw err;
    });
  }

  // Fallback: read body as text and try JSON.parse
  if (typeof response.text === 'function') {
    return response.text().then(function (text) {
      try {
        return JSON.parse(text);
      } catch (err) {
        console.error('[ADMIN] Failed to parse JSON response (text fallback):', err, 'raw:', text);
        showErrorMessage('Ungültige JSON-Antwort vom Server (siehe Konsole)');
        throw err;
      }
    });
  }

  return Promise.reject(new Error('Keine Möglichkeit, Serverantwort zu lesen'));
}

/**
 * Map form field names to config namespace, key, and type
 * @param {string} fieldName - The HTML form field name
 * @param {string} section - The section from the form (e.g., "debug", "system", "led_traffic_light")
 * @returns {object} Object with namespace, key, and type properties
 */
function mapFieldToConfig(fieldName, section) {
  // Debug section maps to "debug" namespace
  if (section === 'debug') {
    const mapping = {
      'debug_ram': { namespace: 'debug', key: 'ram', type: 'bool' },
      'debug_measurement_cycle': { namespace: 'debug', key: 'meas_cycle', type: 'bool' },
      'debug_sensor': { namespace: 'debug', key: 'sensor', type: 'bool' },
      'debug_display': { namespace: 'debug', key: 'display', type: 'bool' },
      'debug_websocket': { namespace: 'debug', key: 'websocket', type: 'bool' },
      'log_level': { namespace: 'log', key: 'level', type: 'string' },
      'file_logging_enabled': { namespace: 'log', key: 'file_enabled', type: 'bool' }
    };
    return mapping[fieldName] || null;
  }

  // System section maps to "general" namespace
  if (section === 'system') {
    const mapping = {
      'device_name': { namespace: 'general', key: 'device_name', type: 'string' },
      'md5_verification': { namespace: 'general', key: 'md5_verify', type: 'bool' },
      'collectd_enabled': { namespace: 'general', key: 'collectd_enabled', type: 'bool' },
      'admin_password': { namespace: 'general', key: 'admin_pwd', type: 'string' }
    };
    return mapping[fieldName] || null;
  }

  // LED traffic light section maps to "led_traf" namespace
  if (section === 'led_traffic_light') {
    const mapping = {
      'led_traffic_light_mode': { namespace: 'led_traf', key: 'mode', type: 'uint' },
      'led_traffic_light_measurement': { namespace: 'led_traf', key: 'sel_meas', type: 'string' }
    };
    return mapping[fieldName] || null;
  }

  return null;
}

/**
 * Auto-save config forms on change (debounced).
 * Uses the unified setConfigValue method for all updates.
 */
function initConfigAutoSave() {
  document.querySelectorAll('form.config-form').forEach(form => {
    const actionAttr = form.getAttribute('action') || '';
  // Only target the main settings forms which post to /admin/updateSettings.
  // WiFi form (action '/admin/updateWiFi') should not auto-save — it will be
  // handled by an explicit Save button below.
  if (!actionAttr.includes('/admin/updateSettings')) return;

    // Hide the visual save button to reduce clutter. The form remains in the DOM for accessibility,
    // but it must be submitted via JavaScript (AJAX) and direct HTML submissions are not supported.
    const submitButtons = form.querySelectorAll('button[type="submit"], input[type="submit"]');
    submitButtons.forEach(btn => { btn.style.display = 'none'; });

    let timer = null;
    const debounceMs = 1000; // 1s debounce
    // Keep track of the last changed field so we can do atomic updates
    let lastChanged = null; // { name, value, type }

    function submitForm() {
      if (!lastChanged) return;

      const sectionInput = form.querySelector('input[name="section"]');
      if (!sectionInput) {
        console.warn('[admin.js] No section input found in form');
        lastChanged = null;
        return;
      }

      const section = sectionInput.value;
      const configMapping = mapFieldToConfig(lastChanged.name, section);

      if (!configMapping) {
        console.warn('[admin.js] No config mapping for field:', lastChanged.name, 'in section:', section);
        lastChanged = null;
        return;
      }

      // Use unified setConfigValue method
      setConfigValue(configMapping.namespace, configMapping.key, lastChanged.value, configMapping.type)
        .then(() => {
          // Success message already shown by setConfigValue
        })
        .catch(err => {
          // Error already logged and shown by setConfigValue
        });

      lastChanged = null;
    }

    function scheduleSubmit() {
      if (timer) clearTimeout(timer);
      timer = setTimeout(() => {
        timer = null;
        submitForm();
      }, debounceMs);
    }

    // Listen for input/select/textarea changes inside the form
    form.addEventListener('input', function(evt) {
      // For text/number inputs use debounce
      const target = evt.target;
      if (target && target.name) {
        lastChanged = {
          name: target.name,
          value: target.value,
          type: target.type
        };
      }
      scheduleSubmit();
    });

    form.addEventListener('change', function(evt) {
      const target = evt.target;
      if (target && target.name) {
        // For checkboxes, value should reflect checked state
        if (target.type === 'checkbox') {
          lastChanged = { name: target.name, value: target.checked ? 'true' : 'false' };
        } else {
          lastChanged = { name: target.name, value: target.value };
        }
      }
      // For checkboxes/selects etc. also debounce
      scheduleSubmit();
    });

    // Intercept explicit form submission (e.g., Enter key) and submit whole form via AJAX
    form.addEventListener('submit', function(evt) {
      evt.preventDefault();
      const action = actionAttr || form.getAttribute('action') || window.location.pathname;
      const params = new URLSearchParams(new FormData(form));
      params.append('ajax', '1');

      fetch(action, {
        method: 'POST',
        body: params,
        credentials: 'include',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-Requested-With': 'XMLHttpRequest' }
      })
      .then(parseJsonResponse)
      .then(data => {
        if (data.success) {
          if (data.changes) {
            const summary = data.changes.replace(/<[^>]+>/g, '').trim();
            showSuccessMessage('Gespeichert: ' + (summary || 'Änderungen übernommen'));
          } else if (data.message) {
            showSuccessMessage(data.message);
          } else {
            showSuccessMessage('Einstellungen gespeichert');
          }
        } else {
          showErrorMessage('Fehler beim Speichern: ' + (data.error || data.message || 'Unbekannter Fehler'));
        }
      })
      .catch(err => {
        console.error('[admin.js] Full form submit error:', err);
        showErrorMessage('Fehler beim Speichern: ' + err.message);
      });
    });
  });
}

// Initialize auto-save on load
window.addEventListener('load', () => {
  initConfigAutoSave();
    // Attach explicit Save handler for WiFi settings (if present)
    const wifiSaveBtn = document.getElementById('save_wifi_settings');
    if (wifiSaveBtn) {
      wifiSaveBtn.addEventListener('click', function () {
        // Find the closest form (the WiFi settings form)
        const form = document.querySelector('form.config-form[action="/admin/updateWiFi"]');
        if (!form) return;
        const params = new URLSearchParams(new FormData(form));
        params.append('ajax', '1');

        showSuccessMessage('Speichere WiFi Einstellungen...');

        fetch('/admin/updateWiFi', {
          method: 'POST',
          body: params,
          credentials: 'include',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-Requested-With': 'XMLHttpRequest' }
        })
        .then(parseJsonResponse)
        .then(data => {
          if (data && data.success) {
            if (data.changes) {
              const summary = data.changes.replace(/<[^>]+>/g, '').trim();
              showSuccessMessage('Gespeichert: ' + (summary || 'Änderungen übernommen'));
            } else if (data.message) {
              showSuccessMessage(data.message);
            } else {
              showSuccessMessage('WiFi Einstellungen gespeichert');
            }
          } else {
            showErrorMessage('Fehler beim Speichern: ' + (data && (data.error || data.message) ? (data.error || data.message) : 'Unbekannt'));
          }
        })
        .catch(err => {
          console.error('[admin.js] WiFi save error:', err);
          showErrorMessage('Fehler beim Speichern: ' + (err.message || err));
        });
      });
    }
  // No legacy redirect handling required: uploads return JSON now.
});

// --- Reboot modal for config upload ---
function showRebootModal(seconds, message) {
    // Create modal elements if not present
    let modal = document.getElementById('reboot-modal');
    if (!modal) {
        modal = document.createElement('div');
        modal.id = 'reboot-modal';
        modal.style.position = 'fixed';
        modal.style.left = '0';
        modal.style.top = '0';
        modal.style.width = '100%';
        modal.style.height = '100%';
        modal.style.background = 'rgba(0,0,0,0.6)';
        modal.style.display = 'flex';
        modal.style.alignItems = 'center';
        modal.style.justifyContent = 'center';
        modal.style.zIndex = '9999';

        const box = document.createElement('div');
        box.id = 'reboot-box';
        box.style.background = '#fff';
        box.style.color = '#000';
        box.style.padding = '30px';
        box.style.borderRadius = '8px';
        box.style.maxWidth = '480px';
        box.style.textAlign = 'center';
        box.style.boxShadow = '0 8px 24px rgba(0,0,0,0.3)';

        const h = document.createElement('h3');
        h.textContent = 'Konfiguration wird angewendet';
        h.style.marginTop = '0';
        box.appendChild(h);

        const p = document.createElement('p');
        p.id = 'reboot-message';
        p.style.whiteSpace = 'pre-wrap';
        p.style.marginBottom = '20px';
        box.appendChild(p);

        const counter = document.createElement('div');
        counter.id = 'reboot-counter';
        counter.style.fontSize = '20px';
        counter.style.fontWeight = 'bold';
        counter.style.marginTop = '12px';
        box.appendChild(counter);

        modal.appendChild(box);
        document.body.appendChild(modal);
    }

    document.getElementById('reboot-message').textContent = message;
    let remaining = seconds;
    const counterEl = document.getElementById('reboot-counter');
    counterEl.textContent = `Neustart in ${remaining} Sekunden...`;

    const interval = setInterval(() => {
        remaining--;
        if (remaining <= 0) {
            clearInterval(interval);
            // Remove modal and reload admin page
            const m = document.getElementById('reboot-modal');
            if (m) m.parentNode.removeChild(m);
            window.location.href = '/admin';
            return;
        }
        counterEl.textContent = `Neustart in ${remaining} Sekunden...`;
    }, 1000);
}

// --- Config upload handler (auto-initialized) ---
(function initConfigUpload() {
    const fileInput = document.getElementById('configFile');
    const form = document.getElementById('uploadConfigForm');

    if (!fileInput || !form) return; // Elements not on this page

    fileInput.addEventListener('change', function(e) {
        if (e.target.files.length > 0) {
            if (confirm('Konfiguration hochladen und anwenden?')) {
                const formData = new FormData(form);
                fetch(form.action, {method: 'POST', body: formData})
                    .then(r => {
                        if (!r.ok) throw new Error('HTTP ' + r.status);
                        return r.json();
                    })
                    .then(data => {
                        if (data.success && data.rebootPending) {
                            showRebootModal(20, data.message || 'Konfiguration wird angewendet...');
                        } else {
                            alert('Fehler: ' + (data.message || 'Unbekannter Fehler'));
                        }
                    })
                    .catch(err => {
                        alert('Netzwerkfehler: ' + err.message);
                    });
            } else {
                e.target.value = ''; // Clear selection if cancelled
            }
        }
    });
})();
