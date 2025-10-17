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

// Intercept the single upload form for settings/sensors and show notifications
window.addEventListener('load', () => {
  const uploadForm = document.getElementById('upload-config-form');
  if (!uploadForm) return;

  uploadForm.addEventListener('submit', function(evt) {
    evt.preventDefault();
    const input = uploadForm.querySelector('input[type="file"]');
    if (!input || !input.files || input.files.length === 0) {
      showErrorMessage('Bitte wählen Sie eine JSON-Datei zum Hochladen aus.');
      return;
    }

    const file = input.files[0];
  const formData = new FormData();
  formData.append('file', file, file.name);
  // mark as AJAX so the server returns JSON instead of redirecting
  formData.append('ajax', '1');

    // Show a temporary message
    showSuccessMessage('Hochladen gestartet...');

    fetch(uploadForm.action, {
      method: 'POST',
      body: formData,
      credentials: 'include',
      headers: {
        'X-Requested-With': 'XMLHttpRequest',
        'Accept': 'application/json'
      }
    })
    .then(response => {
      // Try to parse JSON; if not JSON, treat as text error
      const ct = response.headers.get('content-type') || '';
      if (ct.includes('application/json')) return response.json();
      return response.text().then(t => { throw new Error(t || 'Ungültige Serverantwort'); });
    })
    .then(data => {
      if (data.success) {
        showSuccessMessage(data.message || 'Datei erfolgreich hochgeladen');
      } else {
        showErrorMessage(data.error || data.message || 'Fehler beim Hochladen');
      }
    })
    .catch(err => {
      console.error('[admin.js] Upload error:', err);
      showErrorMessage('Fehler beim Hochladen: ' + (err.message || err));
    });
  });
});

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
  if (response.status === 401) {
    showErrorMessage('Authentifizierung erforderlich');
    throw new Error('Unauthorized');
  }

  const contentType = response.headers.get('content-type') || '';
  if (!contentType.includes('application/json')) {
    return response.text().then(text => {
      throw new Error('Ungültige Server-Antwort: ' + (text || '<leer>'));
    });
  }

  return response.json();
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
  // Only target the main settings forms which post to /admin/updateSettings
  // or the WiFi form which posts to /admin/updateWiFi (we handle WiFi
  // specially below).
  if (!(actionAttr.includes('/admin/updateSettings') || actionAttr.includes('/admin/updateWiFi'))) return;

    // Hide the visual save button to reduce clutter but keep it for fallback
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

            fetch('/admin/updateSettings/json', {
              method: 'POST',
              body: params,
              credentials: 'include',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
            })
            .then(parseJsonResponse)
            .then(data => {
              if (data.success) {
                showSuccessMessage('Einstellung gespeichert');
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
        } else if (actionAttr.includes('/admin/updateWiFi')) {
          // For WiFi forms, send only the changed field to the WiFi update endpoint
          const params = new URLSearchParams();
          params.append(lastChanged.name, lastChanged.value);

          fetch('/admin/updateWiFi', {
            method: 'POST',
            body: params,
            credentials: 'include',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
          })
            .then(response => {
              const contentType = response.headers.get('content-type') || '';
              if (contentType.includes('application/json')) {
                return parseJsonResponse(response);
              }
              // If server returns non-JSON but status is OK we treat it as success
              if (response.ok) {
                showSuccessMessage('WiFi-Einstellung gespeichert');
                return {};
              }
              // Otherwise try to read body and surface error
              return response.text().then(text => { throw new Error(text || 'Unbekannter Serverfehler'); });
            })
            .then(data => {
              // If JSON was returned, we may show messages accordingly
              if (data && data.success === false) {
                showErrorMessage('Fehler beim Speichern: ' + (data.error || data.message || 'Unbekannter Fehler'));
              }
            })
          .catch(err => {
            console.error('[admin.js] WiFi partial update error:', err);
            showErrorMessage('Fehler beim Speichern: ' + err.message);
          });

          lastChanged = null;
          return;
        }
      }
      // Fallback: submit the whole form (legacy behavior)
      const action = '/admin/updateSettings/json';
      // Fallback: send the whole form as urlencoded (less RAM than JSON
      // stringification on the device and supported by the server).
      const formData = new FormData(form);
      const params = new URLSearchParams();
      for (const pair of formData.entries()) {
        params.append(pair[0], pair[1]);
      }

      fetch(action, {
        method: 'POST',
        body: params,
        credentials: 'include',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
      })
      .then(parseJsonResponse)
      .then(data => {
        if (data.success) {
          showSuccessMessage('Einstellungen automatisch gespeichert');
        } else {
          showErrorMessage('Fehler beim automatischen Speichern: ' + (data.error || data.message || 'Unbekannter Fehler'));
        }
      })
      .catch(err => {
        console.error('[admin.js] Auto-save error:', err);
        showErrorMessage('Fehler beim automatischen Speichern: ' + err.message);
      });
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
  });
}

// Initialize auto-save on load
window.addEventListener('load', () => {
  initConfigAutoSave();
  // If the admin page was loaded after a redirect from an upload, show a
  // notification based on the query param upload=ok|err
  try {
    const params = new URLSearchParams(window.location.search);
    const u = params.get('upload');
    if (u === 'ok') {
      showSuccessMessage('Datei erfolgreich importiert');
      // remove param from URL without reloading
      if (window.history && window.history.replaceState) {
        const url = new URL(window.location);
        url.searchParams.delete('upload');
        window.history.replaceState({}, document.title, url.pathname + url.search);
      }
    } else if (u === 'err') {
      showErrorMessage('Fehler beim Importieren der Datei');
      if (window.history && window.history.replaceState) {
        const url = new URL(window.location);
        url.searchParams.delete('upload');
        window.history.replaceState({}, document.title, url.pathname + url.search);
      }
    }
  } catch (e) {
    // ignore URL parsing issues
  }
});
