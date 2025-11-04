/**
 * @fileoverview Admin panel functionality
 */
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

    // Prepare urlencoded body for AJAX partial update
    const params = new URLSearchParams();
    params.append('section', 'system');
    params.append('admin_password', v1);
    params.append('ajax', '1');

    // Visual feedback
    showSuccessMessage('Speichere Passwort...');

    fetch('/admin/updateSettings', {
      method: 'POST',
      body: params,
      credentials: 'include',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-Requested-With': 'XMLHttpRequest' }
    })
    .then(parseJsonResponse)
    .then(data => {
      if (data && data.success) {
        showSuccessMessage('Passwort erfolgreich geändert');
        // Clear fields after success
        pw.value = '';
        pw2.value = '';
      } else {
        showErrorMessage('Fehler beim Speichern: ' + (data && (data.error || data.message) ? (data.error || data.message) : 'Unbekannt'));
      }
    })
    .catch(err => {
      console.error('[admin.js] Password save error:', err);
      showErrorMessage('Fehler beim Speichern: ' + (err.message || err));
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
 * Auto-save config forms on change (debounced).
 * Submits the whole form as application/x-www-form-urlencoded so the server
 * receives the same parameters as a normal form POST (unchecked checkboxes are
 * omitted).
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
    // Keep track of the last changed field so we can do atomic updates when possible
    let lastChanged = null; // { name, value }

    // We perform small urlencoded partial updates for any changed field
    // (section + single field) so the server can handle them uniformly.

    function submitForm() {
      // If lastChanged is set, submit a small urlencoded partial update for
      // the changed field. This unifies the save path for atomic and
      // non-atomic settings and keeps payloads tiny.
      if (lastChanged) {
        // Determine the appropriate partial update endpoint based on form action
        if (actionAttr.includes('/admin/updateSettings')) {
          // submit section + changed field to updateSettings endpoint
          const sectionInput = form.querySelector('input[name="section"]');
          if (sectionInput) {
            const params = new URLSearchParams();
            params.append('section', sectionInput.value);
            params.append(lastChanged.name, lastChanged.value);
            // Mark as AJAX partial update so the server accepts the request even if custom headers are stripped
            params.append('ajax', '1');

            fetch('/admin/updateSettings', {
              method: 'POST',
              body: params,
              credentials: 'include',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-Requested-With': 'XMLHttpRequest' }
            })
            .then(parseJsonResponse)
            .then(data => {
              if (data.success) {
                if (data.changes) {
                  // Strip HTML tags for the toast and show a compact summary
                  const summary = data.changes.replace(/<[^>]+>/g, '').trim();
                  showSuccessMessage('Gespeichert: ' + (summary || 'Änderungen übernommen'));
                } else if (data.message) {
                  showSuccessMessage(data.message);
                } else {
                  showSuccessMessage('Einstellung gespeichert');
                }
              } else {
                showErrorMessage('Fehler beim Speichern: ' + (data.error || data.message || 'Unbekannter Fehler'));
              }
            })
            .catch(err => {
              console.error('[admin.js] Partial update error:', err);
              showErrorMessage('Fehler beim Speichern: ' + err.message);
            });

            lastChanged = null;
            return;
          }
        }
      }
      // Fallback: do nothing — partial updates only
      // Previously we submitted the whole form here; that behavior is removed
      // to enforce partial updates via AJAX from the UI.
      lastChanged = null;
      return;
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
        lastChanged = { name: target.name, value: target.value };
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
