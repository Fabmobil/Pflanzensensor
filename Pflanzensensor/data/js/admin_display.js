/**
 * @file admin_display.js
 * @brief Display configuration page AJAX handlers
 */

document.addEventListener('DOMContentLoaded', function() {
  console.log('[admin_display.js] DOMContentLoaded');

  // Initialize all handlers
  initScreenDurationHandler();
  initClockFormatHandler();
  initDisplayToggleHandlers();
  initMeasurementDisplayHandlers();
});

/**
 * Helper to parse JSON responses and surface auth / non-JSON errors
 */
function parseJsonResponse(response) {
  if (response.status === 401) {
    // Authentication required - show a clear message
    showErrorMessage('Authentifizierung erforderlich');
    // Throw to jump to catch block in the fetch chain
    throw new Error('Unauthorized');
  }

  const contentType = response.headers.get('content-type') || '';
  if (!contentType.includes('application/json')) {
    // Try to include server response text in the error message for diagnostics
    return response.text().then(text => {
      throw new Error('Ungültige Server-Antwort: ' + (text || '<leer>'));
    });
  }

  return response.json();
}

/**
 * Initialize screen duration input handler
 */
function initScreenDurationHandler() {
  const input = document.querySelector('.screen-duration-input');
  if (!input) return;

  let timeout;
  input.addEventListener('input', function() {
    clearTimeout(timeout);
    timeout = setTimeout(() => {
      const duration = parseInt(this.value);
      // User-facing input is in seconds. Convert to milliseconds before storing
      // because the firmware expects screen_dur in milliseconds.
      if (duration >= 1 && duration <= 60) {
        const durationMs = duration * 1000;
        updateScreenDuration(durationMs);
      }
    }, 1000); // 1 second debounce
  });
  console.log('[admin_display.js] Screen duration handler initialized');
}

/**
 * Update screen duration via AJAX
 */
function updateScreenDuration(duration) {
  // duration passed in is milliseconds
  setConfigValue('display', 'screen_dur', duration, 'uint')
    .then(() => {
      console.log('[admin_display.js] Screen duration updated');
    })
    .catch(error => {
      console.error('[admin_display.js] Error updating screen duration:', error);
    });
}

/**
 * Initialize clock format select handler
 */
function initClockFormatHandler() {
  const select = document.querySelector('.clock-format-select');
  if (!select) return;

  select.addEventListener('change', function() {
    updateClockFormat(this.value);
  });
  console.log('[admin_display.js] Clock format handler initialized');
}

/**
 * Update clock format via AJAX
 */
function updateClockFormat(format) {
  setConfigValue('display', 'clock_fmt', format, 'string')
    .then(() => {
      console.log('[admin_display.js] Clock format updated');
    })
    .catch(error => {
      console.error('[admin_display.js] Error updating clock format:', error);
    });
}

/**
 * Initialize display toggle checkboxes (IP, clock, flower, fabmobil)
 */
function initDisplayToggleHandlers() {
  const checkboxes = [
    { selector: '.show-ip-checkbox', setting: 'show_ip' },
    { selector: '.show-clock-checkbox', setting: 'show_clock' },
    { selector: '.show-flower-checkbox', setting: 'show_flower' },
    { selector: '.show-fabmobil-checkbox', setting: 'show_fabmobil' }
  ];

  checkboxes.forEach(({ selector, setting }) => {
    const checkbox = document.querySelector(selector);
    if (checkbox) {
      checkbox.addEventListener('change', function() {
        updateDisplayToggle(setting, this.checked);
      });
    }
  });
  console.log('[admin_display.js] Display toggle handlers initialized');
}

/**
 * Update display toggle setting via AJAX
 */
function updateDisplayToggle(setting, enabled) {
  // Map setting names to config keys
  const keyMap = {
    'show_ip': 'show_ip',
    'show_clock': 'show_clock',
    'show_flower': 'show_flower',
    'show_fabmobil': 'show_fabmobil'
  };

  const key = keyMap[setting];
  if (!key) {
    console.error('[admin_display.js] Unknown setting:', setting);
    return;
  }

  setConfigValue('display', key, enabled, 'bool')
    .then(() => {
      console.log('[admin_display.js] Display toggle updated:', setting, enabled);
    })
    .catch(error => {
      console.error('[admin_display.js] Error updating display toggle:', error);
    });
}

/**
 * Initialize measurement display checkboxes
 */
function initMeasurementDisplayHandlers() {
  // Individual measurements
  document.querySelectorAll('.measurement-display-checkbox').forEach(checkbox => {
    checkbox.addEventListener('change', function() {
      const sensorId = this.dataset.sensorId;
      const measurementIndex = this.dataset.measurementIndex;
      updateMeasurementDisplay(sensorId, this.checked, measurementIndex);
    });
  });

  // Whole sensors
  document.querySelectorAll('.sensor-display-checkbox').forEach(checkbox => {
    checkbox.addEventListener('change', function() {
      const sensorId = this.dataset.sensorId;
      // For single-measurement sensors, explicitly send measurement_index=0
      // so the server toggles the measurement display flag instead of
      // toggling all measurements.
      updateMeasurementDisplay(sensorId, this.checked, '0');
    });
  });
  console.log('[admin_display.js] Measurement display handlers initialized');
}

/**
 * Update measurement display setting via AJAX
 */
function updateMeasurementDisplay(sensorId, enabled, measurementIndex) {
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('enabled', enabled.toString());
  if (measurementIndex !== null) {
    formData.append('measurement_index', measurementIndex);
  }

  // Build URLSearchParams body; include measurement_index when provided so
  // the server can act on the single measurement instead of toggling all.
  const bodyParams = { measurement: sensorId, enabled: enabled };
  if (measurementIndex !== null) {
    bodyParams.measurement_index = measurementIndex;
  }

  // Prepare user-friendly message
  const state = enabled ? 'aktiviert' : 'deaktiviert';
  const successMsg = `Displayanzeige von ${sensorId} ${state}`;

  fetch('/admin/display/measurement_toggle', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    credentials: 'include',
    body: new URLSearchParams(Object.assign({}, bodyParams, { ajax: '1' }))
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage(successMsg);
    } else {
      showErrorMessage('Fehler beim Aktualisieren: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('[admin_display.js] Error updating measurement display:', error);
    showErrorMessage('Fehler beim Aktualisieren: ' + error.message);
  });
}
