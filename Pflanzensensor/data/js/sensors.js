/**
 * @file sensors.js
 * @brief JavaScript for automatic sensor value and countdown updates
 */

let updateFailureCount = 0;
const MAX_UPDATE_FAILURES = 3;
let latestSensorData = {};

function updateSensorValues() {
  console.log('Updating sensor values...');

  return fetch('/getLatestValues')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      updateFailureCount = 0; // Reset on success
      return response.json();
    })
    .then(data => {
      // Update system time offset
      if (data.currentTime) {
        window._serverStartTime = Date.now() - data.currentTime;
      }

      // Update sensor values
      if (data.sensors) {
        latestSensorData = data.sensors; // Store for timer updates
        // Iterate through sensor values instead of treating data.sensors as an array
        Object.entries(data.sensors).forEach(([fieldName, sensorData]) => {
          const sensorBox = document.querySelector(`.sensor-box[data-sensor="${fieldName}"]`);
          if (sensorBox) {
            const valueDisplay = sensorBox.querySelector('.value');
            if (valueDisplay) {
              const unit = sensorBox.dataset.unit || '';
              valueDisplay.textContent = `${parseFloat(sensorData.value).toFixed(1)}${unit ? ' ' + unit : ''}`;
            }

            // Update min/max values if they exist
            if (sensorData.absoluteMin !== undefined || sensorData.absoluteMax !== undefined) {
              let minMaxContainer = sensorBox.querySelector('.min-max-container');
              
              // Create min/max container if it doesn't exist
              if (!minMaxContainer) {
                minMaxContainer = document.createElement('div');
                minMaxContainer.className = 'min-max-container';
                sensorBox.insertBefore(minMaxContainer, sensorBox.querySelector('.details'));
              }
              
              // Clear existing min/max items
              minMaxContainer.innerHTML = '';
              
              // Add min value if available and valid
              if (sensorData.absoluteMin !== undefined && 
                  sensorData.absoluteMin !== Infinity && 
                  sensorData.absoluteMin !== -Infinity && 
                  !isNaN(sensorData.absoluteMin)) {
                const minItem = document.createElement('div');
                minItem.className = 'min-max-item';
                const unit = sensorBox.dataset.unit || '';
                minItem.innerHTML = `<span class='min-max-label'>Min:</span> ${parseFloat(sensorData.absoluteMin).toFixed(1)}${unit ? ' ' + unit : ''}`;
                minMaxContainer.appendChild(minItem);
              }
              
              // Add max value if available and valid
              if (sensorData.absoluteMax !== undefined && 
                  sensorData.absoluteMax !== Infinity && 
                  sensorData.absoluteMax !== -Infinity && 
                  !isNaN(sensorData.absoluteMax)) {
                const maxItem = document.createElement('div');
                maxItem.className = 'min-max-item';
                const unit = sensorBox.dataset.unit || '';
                maxItem.innerHTML = `<span class='min-max-label'>Max:</span> ${parseFloat(sensorData.absoluteMax).toFixed(1)}${unit ? ' ' + unit : ''}`;
                minMaxContainer.appendChild(maxItem);
              }
              
              // Hide container if no valid min/max values
              if (minMaxContainer.children.length === 0) {
                minMaxContainer.style.display = 'none';
              } else {
                minMaxContainer.style.display = 'block';
              }
            }

            // Update status and time
            const statusIndicator = sensorBox.querySelector('.status-indicator');
            if (statusIndicator) {
              const status = translateStatus(sensorData.status || 'unknown');
              let timerText = '';
              if (sensorData.lastMeasurement && sensorData.measurementInterval) {
                const now = Date.now();
                const serverTime = window._serverStartTime ? (now - window._serverStartTime) : now;
                const elapsed = Math.floor((serverTime - sensorData.lastMeasurement) / 1000);
                const intervalSec = Math.floor(sensorData.measurementInterval / 1000);
                timerText = `, ${elapsed}s/${intervalSec}s`;
              } else {
                timerText = ', Keine Messung';
              }
              statusIndicator.textContent = `Status: ${status}${timerText}`;
              statusIndicator.className = `status-indicator status-${(sensorData.status || 'unknown').toLowerCase()}`;
              // Update sensor-box class for color
              const statusClasses = ['status-green', 'status-yellow', 'status-red', 'status-error', 'status-unknown', 'status-warmup'];
              sensorBox.classList.remove(...statusClasses);
              sensorBox.classList.add(`status-${(sensorData.status || 'unknown').toLowerCase()}`);
            }

            // Update last measurement time data attributes
            const lastMeasurementElem = sensorBox.querySelector('.last-measurement');
            if (lastMeasurementElem) {
              lastMeasurementElem.dataset.time = sensorData.lastMeasurement || '0';
              lastMeasurementElem.dataset.interval = sensorData.measurementInterval || '0';
              lastMeasurementElem.dataset.serverTime = window._serverStartTime ? (Date.now() - window._serverStartTime) : Date.now();
            }
          }
        });
      }

      // Update system information
      updateSystemInfo(data.system);
    })
    .catch(error => {
      console.error('Error updating sensors:', error);
      updateFailureCount++;
    });
}

function updateSensorBox(sensorBox, sensor, currentTime) {
  // Update value display
  const valueDisplay = sensorBox.querySelector('.value');
  if (valueDisplay && Array.isArray(sensor.values) && sensor.values.length > 0) {
    const value = parseFloat(sensor.values[0]);
    if (!isNaN(value)) {
      const unit = sensorBox.dataset.unit || '';
      const newValue = `${value.toFixed(1)}${unit ? ' ' + unit : ''}`;
      const oldValue = valueDisplay.textContent;

      if (oldValue !== newValue) {
        valueDisplay.textContent = newValue;
        flashElement(sensorBox);
      }
    }
  }

  // Update status
  const statusIndicator = sensorBox.querySelector('.status-indicator');
  if (statusIndicator) {
    updateSensorStatus(statusIndicator, sensor, sensorBox, currentTime);
  }

  // Update last measurement time
  const lastMeasurement = sensorBox.querySelector('.last-measurement');
  if (lastMeasurement) {
    lastMeasurement.dataset.time = sensor.lastMeasurement || '0';
    lastMeasurement.dataset.interval = sensor.measurementInterval || '0';
    lastMeasurement.dataset.serverTime = currentTime || '0';
  }
}

function updateSensorStatus(statusIndicator, sensor, sensorBox, currentTime) {
  const status = sensor.status || 'unknown';
  const translatedStatus = translateStatus(status);

  // Calculate time since last measurement
  const lastMeasurementSeconds = Math.floor(sensor.lastMeasurement / 1000);
  const currentTimeSeconds = Math.floor(currentTime / 1000);
  const secondsSinceLastMeasurement = currentTimeSeconds - lastMeasurementSeconds;
  const intervalSeconds = Math.floor(sensor.measurementInterval / 1000);

  const newStatusText = `Status: ${translatedStatus}, ${secondsSinceLastMeasurement}s/${intervalSeconds}s`;
  const oldStatusText = statusIndicator.textContent;

  if (oldStatusText !== newStatusText) {
    statusIndicator.textContent = newStatusText;
    flashElement(statusIndicator);
  }

  // Update status classes
  const statusClasses = ['status-green', 'status-yellow', 'status-red', 'status-error', 'status-unknown', 'status-warmup'];
  sensorBox.classList.remove(...statusClasses);
  statusIndicator.classList.remove(...statusClasses);

  const statusClass = `status-${status.toLowerCase()}`;
  sensorBox.classList.add(statusClass);
  statusIndicator.classList.add(statusClass);
}

function updateSystemInfo(systemInfo) {
  if (!systemInfo) return;

  const updateInfoBox = (id, newText) => {
    const element = document.getElementById(id);
    if (element && element.textContent !== newText) {
      element.textContent = newText;
      flashElement(element.closest('.info-box'));
    }
  };

  updateInfoBox('free-heap', `🧮 Freier HEAP: ${systemInfo.freeHeap} bytes`);
  updateInfoBox('heap-fragmentation', `📊 Heap Fragmentierung: ${systemInfo.heapFragmentation}%`);
  updateInfoBox('reboot-count', `🔄 Restarts: ${systemInfo.rebootCount}`);
}

function flashElement(element) {
  if (!element) return;

  // Remove any existing flash animation
  element.classList.remove('flash');

  // Force a reflow
  void element.offsetWidth;

  // Add flash class
  element.classList.add('flash');

  // Remove flash class after animation completes
  setTimeout(() => {
    element.classList.remove('flash');
  }, 300); // Match this to your CSS animation duration
}

function millis() {
  const serverStartTime = window._serverStartTime || Date.now();
  return Date.now() - serverStartTime;
}

function translateStatus(status) {
  if (!status) return 'Unbekannt';

  const translations = {
    'green': 'OK',
    'yellow': 'Warnung',
    'red': 'Kritisch',
    'error': 'Fehler',
    'warmup': 'Aufwärmen',
    'unknown': 'Unbekannt'
  };
  return translations[status.toLowerCase()] || status;
}

function updateStatusTimers() {
  Object.entries(latestSensorData).forEach(([fieldName, sensorData]) => {
    const sensorBox = document.querySelector(`.sensor-box[data-sensor="${fieldName}"]`);
    if (sensorBox) {
      const statusIndicator = sensorBox.querySelector('.status-indicator');
      if (statusIndicator) {
        const status = translateStatus(sensorData.status || 'unknown');
        let timerText = '';
        if (sensorData.lastMeasurement && sensorData.measurementInterval) {
          const now = Date.now();
          const serverTime = window._serverStartTime ? (now - window._serverStartTime) : now;
          const elapsed = Math.floor((serverTime - sensorData.lastMeasurement) / 1000);
          const intervalSec = Math.floor(sensorData.measurementInterval / 1000);
          timerText = `, ${elapsed}s/${intervalSec}s`;
        } else {
          timerText = ', Keine Messung';
        }
        statusIndicator.textContent = `Status: ${status}${timerText}`;
        statusIndicator.className = `status-indicator status-${(sensorData.status || 'unknown').toLowerCase()}`;
        // Update sensor-box class for color
        const statusClasses = ['status-green', 'status-yellow', 'status-red', 'status-error', 'status-unknown', 'status-warmup'];
        sensorBox.classList.remove(...statusClasses);
        sensorBox.classList.add(`status-${(sensorData.status || 'unknown').toLowerCase()}`);
      }
    }
  });
}

function showGlobalMessage(message, type = 'info') {
  let messageElement = document.getElementById('global-feedback-message');
  if (!messageElement) {
    messageElement = document.createElement('div');
    messageElement.id = 'global-feedback-message';
    messageElement.style.cssText = `
      position: fixed;
      top: 20px;
      right: 20px;
      min-width: 220px;
      max-width: 400px;
      padding: 12px 18px;
      border-radius: 6px;
      color: #fff;
      font-weight: bold;
      z-index: 2000;
      opacity: 0;
      transition: opacity 0.3s ease;
      box-shadow: 0 2px 8px rgba(0,0,0,0.12);
      font-size: 1.08em;
      pointer-events: none;
    `;
    document.body.appendChild(messageElement);
  }
  messageElement.textContent = message;
  if (type === 'success') {
    messageElement.style.backgroundColor = '#4CAF50';
  } else if (type === 'error') {
    messageElement.style.backgroundColor = '#f44336';
  } else {
    messageElement.style.backgroundColor = '#1976d2';
  }
  messageElement.style.opacity = '1';
  setTimeout(() => {
    messageElement.style.opacity = '0';
  }, 3000);
}

function triggerMeasurement(sensorId, button) {
  showGlobalMessage('Messung angefordert...', 'info');
  button && (button.disabled = true);
  fetch('/trigger_measurement', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: `sensor_id=${encodeURIComponent(sensorId)}`
  })
    .then(async response => {
      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`Measurement failed: ${response.status} ${response.statusText} - ${errorText}`);
      }
      return response.json();
    })
    .then(data => {
      if (data.success) {
        showGlobalMessage('Messung erfolgreich angefordert!', 'success');
      } else {
        showGlobalMessage('Fehler bei der Messung', 'error');
        throw new Error(data.error || 'Unknown error');
      }
    })
    .catch(error => {
      showGlobalMessage('Fehler bei der Messung: ' + error.message, 'error');
      alert('Fehler bei der Messung: ' + error.message);
    })
    .finally(() => {
      button && setTimeout(() => { button.disabled = false; }, 5000);
    });
}

// --- Setup for Both Pages ---
function setupMeasurementTriggers() {
  console.log('[sensors.js] Setting up measurement triggers...');
  const buttons = document.querySelectorAll('.measure-button');
  console.log('[sensors.js] Found', buttons.length, 'measure buttons');
  // For standalone measure buttons (startpage and admin page)
  buttons.forEach(button => {
    // If button is inside a form, skip (handled below)
    if (button.closest('form.measure-form')) return;
    const sensorId = button.dataset.sensor;
    button.addEventListener('click', e => {
      e.preventDefault();
      console.log('[sensors.js] Measure button clicked:', sensorId);
      triggerMeasurement(sensorId, button);
    });
  });
  // For forms (admin page)
  document.querySelectorAll('form.measure-form').forEach(form => {
    form.addEventListener('submit', e => {
      e.preventDefault();
      const button = form.querySelector('.measure-button');
      const sensorIdInput = form.querySelector('input[name="sensor_id"]');
      if (!sensorIdInput) return;
      const sensorId = sensorIdInput.value;
      triggerMeasurement(sensorId, button);
    });
  });
}

// Add this function before DOMContentLoaded or at the top-level
function updateLastMeasurementTime(elem, sensorId) {
  // Get data attributes
  const lastTime = parseInt(elem.dataset.time || '0', 10);
  const interval = parseInt(elem.dataset.interval || '0', 10);
  const serverTime = parseInt(elem.dataset.serverTime || '0', 10);

  if (lastTime > 0 && serverTime > 0) {
    // Calculate the time when the measurement actually happened (in ms)
    const measurementTimestamp = Date.now() - (serverTime - lastTime);
    // Calculate elapsed seconds since last measurement
    const elapsed = Math.floor((Date.now() - measurementTimestamp) / 1000);
    const intervalSec = Math.floor(interval / 1000);
    elem.textContent = `Letzte Messung: vor ${elapsed}s / Intervall: ${intervalSec}s`;
  } else {
    elem.textContent = 'Letzte Messung: Keine Messung';
  }
}

// Initialize updates
document.addEventListener('DOMContentLoaded', () => {
  console.log('[sensors.js] DOMContentLoaded');
  setupMeasurementTriggers();

  if (document.querySelector('.sensor-box') || document.querySelector('.info-container')) {
    console.log('Found sensors or info container, starting updates');

    // Initial update with retry
    const tryInitialUpdate = () => {
      updateSensorValues().catch(error => {
        console.error('Initial update failed, retrying in 5s:', error);
        setTimeout(tryInitialUpdate, 5000);
      });
    };
    tryInitialUpdate();

    // Set up regular updates
    window._updateInterval = setInterval(updateSensorValues, 10000);

    // Update measurement times every second
    const timeUpdateInterval = setInterval(() => {
      document.querySelectorAll('.last-measurement').forEach(elem => {
        const sensorBox = elem.closest('.sensor-box');
        const sensorId = sensorBox ? sensorBox.dataset.sensor : 'unknown';
        updateLastMeasurementTime(elem, sensorId);
      });
    }, 1000);

    // Update status timers every second
    setInterval(updateStatusTimers, 1000);

    // Cleanup intervals when page is unloaded
    window.addEventListener('unload', () => {
      clearInterval(window._updateInterval);
      clearInterval(timeUpdateInterval);
    });
  } else {
    console.log('No sensors or info container found');
  }
});
window.setupMeasurementTriggers = setupMeasurementTriggers;
window.triggerMeasurement = triggerMeasurement;
