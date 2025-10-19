/**
 * @file admin_sensors.js
 * @brief JavaScript for AJAX updates of sensor settings in admin panel
 */

console.log('[admin_sensors.js] loaded');

// Global object to store initial values for each measurement
const initialMeasurementValues = {};

// Add periodic update of measured sensor values
function updateAdminSensorValues() {
  fetch('/getLatestValues', { credentials: 'include' })
    .then(parseJsonResponse)
    .then(data => {
      if (data.sensors) {
        Object.entries(data.sensors).forEach(([sensorKey, sensorData]) => {
          // Update main measured value
          const readonlyInput = document.querySelector(`input.readonly-value[data-sensor='${sensorKey}']`);
          if (readonlyInput && typeof sensorData.value !== 'undefined') {
            const newValue = parseFloat(sensorData.value).toFixed(1);
            if (readonlyInput.value !== newValue) {
              readonlyInput.value = newValue;
              const card = readonlyInput.closest('.measurement-card');
              if (card) {
                card.classList.remove('flash');
                void card.offsetWidth;
                card.classList.add('flash');
                setTimeout(() => card.classList.remove('flash'), 300);
              }
            }
            // Update threshold bar marker (use processed value)
            const thresholdContainer = document.getElementById('threshold_' + sensorKey);
            const slider = thresholdContainer ? thresholdContainer.querySelector('.threshold-slider') : null;
            if (slider && typeof slider.updateBar === 'function') {
              slider.lastValue = parseFloat(sensorData.value);
              slider.updateBar();
            }
            // --- NEW: Update measured value marker and label on the slider ---
            if (thresholdContainer) {
              const sliderContainer = thresholdContainer.parentElement;
              if (sliderContainer && sliderContainer.classList.contains('threshold-slider-container')) {
                // Update the marker and label if present
                const valueDot = sliderContainer.querySelector('.measured-value-dot');
                const valueLabel = sliderContainer.querySelector('.measured-value-label');
                if (valueDot && valueLabel) {
                  const min = parseFloat(thresholdContainer.getAttribute('data-min')) || 0;
                  const max = parseFloat(thresholdContainer.getAttribute('data-max')) || 100;
                  const percent = ((parseFloat(sensorData.value) - min) / (max - min)) * 100;
                  valueDot.style.left = `${percent}%`;
                  valueLabel.style.left = `${percent}%`;
                  valueLabel.textContent = parseFloat(sensorData.value).toFixed(2);
                }
              }
            }
          }
          // Update raw value in Rohwerte section for analog sensors
          if (typeof sensorData.raw !== 'undefined') {
            // Find the minmax-section for this sensor/measurement
            const minmaxSection = document.querySelector(`.minmax-section input.analog-min-input[data-sensor-id], .minmax-section input.analog-max-input[data-sensor-id]`);
            // Try to find the correct minmax-section by matching sensorKey
            // sensorKey is like 'ANALOG_0', 'ANALOG_1', etc.
            const match = sensorKey.match(/^(.*)_(\d+)$/);
            if (match) {
              const sensorId = match[1];
              const measurementIndex = match[2];
              // Find the correct minmax-section
              const minmax = document.querySelector(`.minmax-section input.analog-min-input[data-sensor-id='${sensorId}'][data-measurement-index='${measurementIndex}']`);
              if (minmax) {
                // The raw value input is the next readonly input after min
                const minmaxRow = minmax.closest('.minmax-section');
                if (minmaxRow) {
                  const rawInput = minmaxRow.querySelector('input.readonly-value');
                  if (rawInput && rawInput.value !== String(sensorData.raw)) {
                    rawInput.value = sensorData.raw;
                  }
                }
              }
            }
          }

          // Update absolute min/max values
          if (typeof sensorData.absoluteMin !== 'undefined' || typeof sensorData.absoluteMax !== 'undefined') {
            console.debug(`[updateAdminSensorValues] Received absolute min/max for ${sensorKey}: min=${sensorData.absoluteMin}, max=${sensorData.absoluteMax}`);
            const match = sensorKey.match(/^(.*)_(\d+)$/);
            if (match) {
              const sensorId = match[1];
              const measurementIndex = parseInt(match[2]);
              updateAbsoluteMinMaxDisplay(sensorId, measurementIndex, sensorData.absoluteMin, sensorData.absoluteMax);
            }
          }

          // Update absolute raw min/max values for analog sensors
          if (typeof sensorData.absoluteRawMin !== 'undefined' || typeof sensorData.absoluteRawMax !== 'undefined') {
            console.debug(`[updateAdminSensorValues] Received absolute raw min/max for ${sensorKey}: min=${sensorData.absoluteRawMin} (type: ${typeof sensorData.absoluteRawMin}), max=${sensorData.absoluteRawMax} (type: ${typeof sensorData.absoluteRawMax})`);
            const match = sensorKey.match(/^(.*)_(\d+)$/);
            if (match) {
              const sensorId = match[1];
              const measurementIndex = parseInt(match[2]);
              console.debug(`[updateAdminSensorValues] Calling updateAbsoluteRawMinMaxDisplay with sensorId=${sensorId}, measurementIndex=${measurementIndex}`);
              updateAbsoluteRawMinMaxDisplay(sensorId, measurementIndex, sensorData.absoluteRawMin, sensorData.absoluteRawMax);
            } else {
              console.warn(`[updateAdminSensorValues] Could not parse sensor key: ${sensorKey}`);
            }
          }
        });
      }
    })
    .catch(err => {
      // Optionally handle error
      // console.error('Error updating admin sensor values:', err);
    });
}

document.addEventListener('DOMContentLoaded', function() {
  console.log('[admin_sensors.js] DOMContentLoaded');
  setInterval(updateAdminSensorValues, 5000);

  // Initialize Flower Status Sensor change handler (AJAX)
  const flowerStatusSelect = document.getElementById('flower-status-sensor');
  if (flowerStatusSelect) {
    flowerStatusSelect.addEventListener('change', function() {
      const sensor = this.value;
      console.log('[admin_sensors.js] Flower status sensor changed to:', sensor);

      const formData = new FormData();
      formData.append('sensor', sensor);

      fetch('/admin/sensors/flower_status', {
        method: 'POST',
        body: formData,
        credentials: 'include',
        headers: { 'X-Requested-With': 'XMLHttpRequest' }
      })
      .then(parseJsonResponse)
      .then(data => {
        if (data.success) {
          showSuccessMessage('Blumen-Status Sensor erfolgreich aktualisiert');
        } else {
          showErrorMessage('Fehler beim Speichern: ' + (data.error || 'Unbekannter Fehler'));
        }
      })
      .catch(error => {
        console.error('[admin_sensors.js] Error updating flower status:', error);
        showErrorMessage('Fehler beim Speichern: ' + error.message);
      });
    });
  }

  // Fetch initial config and initialize everything after
  fetch('/admin/getSensorConfig', { credentials: 'include' })
    .then(parseJsonResponse)
    .then(data => {
      if (data.success && data.sensors) {
        storeInitialMeasurementValues(data.sensors);
        // --- DEBUG ---
        console.log('[admin_sensors.js] Fetched config:', data);
        // Initialize threshold sliders
        initializeThresholdSliders(data);
      } else {
        console.warn('[admin_sensors.js] No sensors in config:', data);
      }
      // Initialize all AJAX handlers
      initMeasurementIntervalHandlers();
      initAnalogMinMaxHandlers();
      initAnalogInvertedHandlers();
      initThresholdHandlers();
      initMeasurementNameHandlers();
      initResetMinMaxHandlers();
      initMeasureButtonHandlers();

      // Extend the existing sensors.js functionality for admin page
      extendSensorsJSForAdmin();

      // --- Measure button handler is now always initialized ---
      console.log('[admin_sensors.js] All handlers initialized, including measure buttons');
    });
});

/**
 * Extend the existing sensors.js functionality to work with admin page structure
 */
function extendSensorsJSForAdmin() {
  // Add sensor-box class and data attributes to measurement cards for compatibility with sensors.js
  document.querySelectorAll('.measurement-card').forEach(card => {
    const readonlyInput = card.querySelector('input.readonly-value');
    if (readonlyInput && readonlyInput.dataset.sensor) {
      // Add sensor-box class and data-sensor attribute to the card
      card.classList.add('sensor-box');
      card.dataset.sensor = readonlyInput.dataset.sensor;

      // Add a .value element that sensors.js can update
      const valueDisplay = document.createElement('div');
      valueDisplay.className = 'value';
      valueDisplay.style.display = 'none'; // Hide it since we use readonly input
      card.appendChild(valueDisplay);

      // Add status indicator for sensors.js
      const statusIndicator = document.createElement('div');
      statusIndicator.className = 'status-indicator';
      statusIndicator.style.display = 'none'; // Hide it since we don't need it in admin
      card.appendChild(statusIndicator);
    }
  });

  // Set up a mutation observer to watch for changes to .value elements and update readonly inputs
  const observer = new MutationObserver((mutations) => {
    mutations.forEach((mutation) => {
      if (mutation.type === 'childList' || mutation.type === 'characterData') {
        const target = mutation.target;
        if (target.classList && target.classList.contains('value')) {
          const sensorBox = target.closest('.sensor-box');
          if (sensorBox) {
            const readonlyInput = sensorBox.querySelector('input.readonly-value');
            if (readonlyInput) {
              // Extract numeric value from the .value element text
              const valueText = target.textContent;
              const match = valueText.match(/(\d+\.?\d*)/);
              if (match) {
                const newValue = parseFloat(match[1]).toFixed(1);
                if (readonlyInput.value !== newValue) {
                  readonlyInput.value = newValue;
                  // Flash the measurement card
                  sensorBox.classList.remove('flash');
                  void sensorBox.offsetWidth; // Force reflow
                  sensorBox.classList.add('flash');
                  setTimeout(() => sensorBox.classList.remove('flash'), 300);
                }
              }
            }
          }
        }
      }
    });
  });

  // Start observing all .value elements
  document.querySelectorAll('.value').forEach(valueElement => {
    observer.observe(valueElement, {
      childList: true,
      characterData: true,
      subtree: true
    });
  });
}

function storeInitialMeasurementValues(sensors) {
  // sensors is now an object with sensor IDs as keys
  Object.keys(sensors).forEach(sensorId => {
    const sensor = sensors[sensorId];
    if (!sensor.measurements) return;

    // measurements is now an object with indices as keys
    Object.keys(sensor.measurements).forEach(idx => {
      const m = sensor.measurements[idx];
      initialMeasurementValues[`${sensorId}_${idx}`] = {
        name: m.name,
        enabled: m.enabled,
        thresholds: {
          yellowLow: m.thresholds.yellowLow,
          greenLow: m.thresholds.greenLow,
          greenHigh: m.thresholds.greenHigh,
          yellowHigh: m.thresholds.yellowHigh
        },
        min: m.minmax ? m.minmax.min : undefined,
        max: m.minmax ? m.minmax.max : undefined,
        inverted: m.inverted !== undefined ? m.inverted : false,
        interval: sensor.interval ? sensor.interval / 1000 : undefined, // seconds
        absoluteMin: m.absoluteMin !== undefined ? m.absoluteMin : undefined,
        absoluteMax: m.absoluteMax !== undefined ? m.absoluteMax : undefined,
        absoluteRawMin: m.absoluteRawMin !== undefined ? m.absoluteRawMin : undefined,
        absoluteRawMax: m.absoluteRawMax !== undefined ? m.absoluteRawMax : undefined
      };
    });
  });
}

/**
 * Initialize handlers for measurement interval inputs
 */
function initMeasurementIntervalHandlers() {
  document.querySelectorAll('.measurement-interval-input').forEach(input => {
    let timeout;
    input.addEventListener('input', function() {
      clearTimeout(timeout);
      const sensorId = this.dataset.sensorId;
      const interval = parseInt(this.value);

      // Debounce the update
      timeout = setTimeout(() => {
        if (interval >= 10 && interval <= 3600) {
          updateMeasurementInterval(sensorId, interval);
        }
      }, 1000);
    });
  });
}

/**
 * Initialize handlers for analog sensor min/max inputs
 */
function initAnalogMinMaxHandlers() {
  // Use event delegation to handle dynamically created elements
  document.addEventListener('input', function(event) {
    if (event.target.classList.contains('analog-min-input') || event.target.classList.contains('analog-max-input')) {
      const input = event.target;
      const sensorId = input.dataset.sensorId;
      const measurementIndex = parseInt(input.dataset.measurementIndex);

      // Clear any existing timeout for this sensor/measurement combination
      const timeoutKey = `analog_minmax_${sensorId}_${measurementIndex}`;
      if (window.analogMinMaxTimeouts && window.analogMinMaxTimeouts[timeoutKey]) {
        clearTimeout(window.analogMinMaxTimeouts[timeoutKey]);
      }

      // Get both min and max values
      const minInput = document.querySelector(`.analog-min-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`);
      const maxInput = document.querySelector(`.analog-max-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`);

      if (minInput && maxInput) {
        const minValue = parseFloat(minInput.value);
        const maxValue = parseFloat(maxInput.value);

        // Initialize timeout storage if it doesn't exist
        if (!window.analogMinMaxTimeouts) {
          window.analogMinMaxTimeouts = {};
        }

        // Debounce the update
        window.analogMinMaxTimeouts[timeoutKey] = setTimeout(() => {
          if (!isNaN(minValue) && !isNaN(maxValue) && minValue < maxValue) {
            console.log(`[initAnalogMinMaxHandlers] Updating analog min/max for ${sensorId}_${measurementIndex}: min=${minValue}, max=${maxValue}`);
            updateAnalogMinMax(sensorId, measurementIndex, minValue, maxValue);
          } else {
            console.warn(`[initAnalogMinMaxHandlers] Invalid values for ${sensorId}_${measurementIndex}: min=${minValue}, max=${maxValue}`);
          }
          // Clean up timeout reference
          delete window.analogMinMaxTimeouts[timeoutKey];
        }, 1000);
      }
    }
  });
}

/**
 * Initialize handlers for analog sensor inverted checkbox
 */
function initAnalogInvertedHandlers() {
  document.querySelectorAll('.analog-inverted-checkbox').forEach(checkbox => {
    checkbox.addEventListener('change', function() {
      const sensorId = this.dataset.sensorId;
      const measurementIndex = parseInt(this.dataset.measurementIndex);
      const inverted = this.checked;

      updateAnalogInverted(sensorId, measurementIndex, inverted);
    });
  });
}

/**
 * Initialize handlers for threshold inputs
 */
function initThresholdHandlers() {
  document.querySelectorAll('.threshold-input').forEach(input => {
    let timeout;
    input.addEventListener('input', function() {
      clearTimeout(timeout);

      // Find the sensor ID and measurement index from the input name
      const name = this.name;
      const match = name.match(/^(.+)_(\d+)_(.+)$/);
      if (match) {
        const sensorId = match[1];
        const measurementIndex = parseInt(match[2]);
        const thresholdType = match[3];

        // Get all threshold values for this measurement
        const yellowLowInput = document.querySelector(`input[name="${sensorId}_${measurementIndex}_yellowLow"]`);
        const greenLowInput = document.querySelector(`input[name="${sensorId}_${measurementIndex}_greenLow"]`);
        const greenHighInput = document.querySelector(`input[name="${sensorId}_${measurementIndex}_greenHigh"]`);
        const yellowHighInput = document.querySelector(`input[name="${sensorId}_${measurementIndex}_yellowHigh"]`);

        if (yellowLowInput && greenLowInput && greenHighInput && yellowHighInput) {
          const thresholds = [
            parseFloat(yellowLowInput.value),
            parseFloat(greenLowInput.value),
            parseFloat(greenHighInput.value),
            parseFloat(yellowHighInput.value)
          ];

          // Debounce the update
          timeout = setTimeout(() => {
            if (thresholds.every(t => !isNaN(t))) {
              updateThresholds(sensorId, measurementIndex, thresholds);
            }
          }, 1000);
        }
      }
    });
  });
}

/**
 * Initialize handlers for measurement name inputs
 */
function initMeasurementNameHandlers() {
  document.querySelectorAll('.measurement-name').forEach(input => {
    let timeout;
    input.addEventListener('input', function() {
      clearTimeout(timeout);

      // Find the sensor ID and measurement index from the input name
      const name = this.name;
      const match = name.match(/^name_(.+)_(\d+)$/);
      if (match) {
        const sensorId = match[1];
        const measurementIndex = parseInt(match[2]);

        // Debounce the update
        timeout = setTimeout(() => {
          if (this.value.trim()) {
            updateMeasurementName(sensorId, measurementIndex, this.value.trim());
          }
        }, 1000);
        console.log('[DEBUG] Name input change event fired for', sensorId, measurementIndex, 'value:', this.value);
      }
    });
  });
}

/**
 * Update absolute min/max values display for a specific sensor measurement
 */
function updateAbsoluteMinMaxDisplay(sensorId, measurementIndex, absoluteMin, absoluteMax) {
  console.debug(`[updateAbsoluteMinMaxDisplay] Updating ${sensorId}_${measurementIndex}: min=${absoluteMin}, max=${absoluteMax}`);

  // Find the absolute min input
  const minInput = document.querySelector(`.absolute-min-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`);
  if (minInput) {
    if (absoluteMin !== undefined && absoluteMin !== Infinity) {
      minInput.value = absoluteMin.toFixed(2);
      console.debug(`[updateAbsoluteMinMaxDisplay] Updated min input to ${absoluteMin.toFixed(2)}`);
    } else {
      minInput.value = '--';
      console.debug(`[updateAbsoluteMinMaxDisplay] Reset min input to --`);
    }
  } else {
    console.warn(`[updateAbsoluteMinMaxDisplay] Min input not found for ${sensorId}_${measurementIndex}`);
  }

  // Find the absolute max input
  const maxInput = document.querySelector(`.absolute-max-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`);
  if (maxInput) {
    if (absoluteMax !== undefined && absoluteMax !== -Infinity) {
      maxInput.value = absoluteMax.toFixed(2);
      console.debug(`[updateAbsoluteMinMaxDisplay] Updated max input to ${absoluteMax.toFixed(2)}`);
    } else {
      maxInput.value = '--';
      console.debug(`[updateAbsoluteMinMaxDisplay] Reset max input to --`);
    }
  } else {
    console.warn(`[updateAbsoluteMinMaxDisplay] Max input not found for ${sensorId}_${measurementIndex}`);
  }
}

/**
 * Reset absolute min/max values via AJAX
 */
function resetAbsoluteMinMax(sensorId, measurementIndex) {
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());

  fetch('/admin/reset_absolute_minmax', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Absolute Min/Max Werte zurückgesetzt');
      // Update the display to show "--" for reset values
      updateAbsoluteMinMaxDisplay(sensorId, measurementIndex, Infinity, -Infinity);
    } else {
      showErrorMessage('Fehler beim Zurücksetzen der Min/Max Werte: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error resetting absolute min/max values:', error);
    showErrorMessage('Netzwerkfehler beim Zurücksetzen der Min/Max Werte');
  });
}

/**
 * Update absolute raw min/max values display for a specific sensor measurement
 */
function updateAbsoluteRawMinMaxDisplay(sensorId, measurementIndex, absoluteRawMin, absoluteRawMax) {
  console.debug(`[updateAbsoluteRawMinMaxDisplay] Updating ${sensorId}_${measurementIndex}: min=${absoluteRawMin}, max=${absoluteRawMax}`);

  // Convert string values to numbers for comparison
  const rawMinNum = parseInt(absoluteRawMin);
  const rawMaxNum = parseInt(absoluteRawMax);

  // Debug: Show what selectors we're looking for
  const minSelector = `.absolute-raw-min-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`;
  const maxSelector = `.absolute-raw-max-input[data-sensor-id="${sensorId}"][data-measurement-index="${measurementIndex}"]`;
  console.debug(`[updateAbsoluteRawMinMaxDisplay] Looking for min selector: ${minSelector}`);
  console.debug(`[updateAbsoluteRawMinMaxDisplay] Looking for max selector: ${maxSelector}`);

  // Find the absolute raw min input
  const minInput = document.querySelector(minSelector);
  if (minInput) {
    if (absoluteRawMin !== undefined && !isNaN(rawMinNum) && rawMinNum !== 2147483647) { // INT_MAX
      minInput.value = absoluteRawMin;
      console.debug(`[updateAbsoluteRawMinMaxDisplay] Updated raw min input to ${absoluteRawMin}`);
    } else {
      minInput.value = '--';
      console.debug(`[updateAbsoluteRawMinMaxDisplay] Reset raw min input to --`);
    }
  } else {
    console.warn(`[updateAbsoluteRawMinMaxDisplay] Raw min input not found for ${sensorId}_${measurementIndex}`);
  }

  // Find the absolute raw max input
  const maxInput = document.querySelector(maxSelector);
  if (maxInput) {
    if (absoluteRawMax !== undefined && !isNaN(rawMaxNum) && rawMaxNum !== -2147483648) { // INT_MIN
      maxInput.value = absoluteRawMax;
      console.debug(`[updateAbsoluteRawMinMaxDisplay] Updated raw max input to ${absoluteRawMax}`);
    } else {
      maxInput.value = '--';
      console.debug(`[updateAbsoluteRawMinMaxDisplay] Reset raw max input to --`);
    }
  } else {
    console.warn(`[updateAbsoluteRawMinMaxDisplay] Raw max input not found for ${sensorId}_${measurementIndex}`);
  }
}

/**
 * Reset absolute raw min/max values via AJAX
 */
function resetAbsoluteRawMinMax(sensorId, measurementIndex) {
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());

  fetch('/admin/reset_absolute_raw_minmax', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Absolute Raw Min/Max Werte zurückgesetzt');
      // Update the display to show "--" for reset values
      updateAbsoluteRawMinMaxDisplay(sensorId, measurementIndex, 2147483647, -2147483648);
    } else {
      showErrorMessage('Fehler beim Zurücksetzen der Raw Min/Max Werte: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error resetting absolute raw min/max values:', error);
    showErrorMessage('Netzwerkfehler beim Zurücksetzen der Raw Min/Max Werte');
  });
}

/**
 * Initialize handlers for reset min/max buttons
 */
function initResetMinMaxHandlers() {
  document.querySelectorAll('.reset-minmax-button').forEach(button => {
    button.addEventListener('click', function() {
      const sensorId = this.dataset.sensorId;
      const measurementIndex = parseInt(this.dataset.measurementIndex);
      resetAbsoluteMinMax(sensorId, measurementIndex);
    });
  });

  // Initialize handlers for reset raw min/max buttons
  document.querySelectorAll('.reset-raw-minmax-button').forEach(button => {
    button.addEventListener('click', function() {
      const sensorId = this.dataset.sensorId;
      const measurementIndex = parseInt(this.dataset.measurementIndex);
      resetAbsoluteRawMinMax(sensorId, measurementIndex);
    });
  });
}

/**
 * Initialize handlers for measure buttons
 */
function initMeasureButtonHandlers() {
  document.querySelectorAll('.measure-button').forEach(button => {
    button.addEventListener('click', function() {
      const sensorId = this.dataset.sensor;
      console.log('[admin_sensors.js] Trigger measurement for sensor:', sensorId);

      const formData = new FormData();
      formData.append('sensor_id', sensorId);

      fetch('/trigger_measurement', {
        method: 'POST',
        headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams({ sensor_id: sensorId, measurement_index: measurementIndex })
      })
      .then(parseJsonResponse)
      .then(data => {
        if (data.success) {
          console.log('[admin_sensors.js] Measurement triggered successfully');
          // Optionally show feedback
        } else {
          console.error('[admin_sensors.js] Failed to trigger measurement:', data.error || data.message);
          alert('Fehler beim Auslösen der Messung: ' + (data.error || data.message || 'Unbekannter Fehler'));
        }
      })
      .catch(error => {
        console.error('[admin_sensors.js] Error triggering measurement:', error);
        alert('Fehler beim Auslösen der Messung: ' + error.message);
      });
    });
  });
}

/**
 * Update measurement interval via AJAX
 */
function updateMeasurementInterval(sensorId, interval) {
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('interval', interval.toString());

  fetch('/admin/measurement_interval', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId, interval: interval })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Messungsintervall erfolgreich auf ' + interval + ' Sekunden aktualisiert');
    } else {
      showErrorMessage('Fehler beim Aktualisieren des Messungsintervalls: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error updating measurement interval:', error);
    showErrorMessage('Netzwerkfehler beim Aktualisieren des Messungsintervalls');
  });
}

/**
 * Update analog sensor min/max values via AJAX
 */
function updateAnalogMinMax(sensorId, measurementIndex, minValue, maxValue) {
  console.log(`[updateAnalogMinMax] Sending request for ${sensorId}_${measurementIndex}: min=${minValue}, max=${maxValue}`);

  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());
  formData.append('min', minValue.toString());
  formData.append('max', maxValue.toString());

  fetch('/admin/analog_minmax', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId, measurement: idx, min: min, max: max })
  })
  .then(response => {
    console.log(`[updateAnalogMinMax] Response status: ${response.status}`);
    return parseJsonResponse(response);
  })
  .then(data => {
    console.log(`[updateAnalogMinMax] Response data:`, data);
    if (data.success) {
      showSuccessMessage('Min/Max Werte erfolgreich aktualisiert');
    } else {
      showErrorMessage('Fehler beim Aktualisieren der Min/Max Werte: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error updating analog min/max values:', error);
    showErrorMessage('Netzwerkfehler beim Aktualisieren der Min/Max Werte');
  });
}

/**
 * Update analog sensor inverted flag via AJAX
 */
function updateAnalogInverted(sensorId, measurementIndex, inverted) {
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());
  formData.append('inverted', inverted.toString());

  fetch('/admin/analog_inverted', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId, measurement: idx, inverted: inverted ? 1 : 0 })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Invertierung erfolgreich ' + (inverted ? 'aktiviert' : 'deaktiviert'));
    } else {
      showErrorMessage('Fehler beim Aktualisieren der Invertierung: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error updating analog inverted flag:', error);
    showErrorMessage('Netzwerkfehler beim Aktualisieren der Invertierung');
  });
}

/**
 * Update thresholds via AJAX
 */
function updateThresholds(sensorId, measurementIndex, thresholds) {
  console.log('[DEBUG] updateThresholds called for', sensorId, measurementIndex, thresholds);
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());
  formData.append('thresholds', thresholds.join(','));

  fetch('/admin/thresholds', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId, measurement: idx, yellowLow: yellowLow, greenLow: greenLow, greenHigh: greenHigh, yellowHigh: yellowHigh })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Schwellwerte erfolgreich aktualisiert');
    } else {
      showErrorMessage('Fehler beim Aktualisieren der Schwellwerte: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error updating thresholds:', error);
    showErrorMessage('Netzwerkfehler beim Aktualisieren der Schwellwerte');
  });
}

/**
 * Update measurement name via AJAX
 */
function updateMeasurementName(sensorId, measurementIndex, name) {
  console.log('[DEBUG] updateMeasurementName called for', sensorId, measurementIndex, name);
  const formData = new FormData();
  formData.append('sensor_id', sensorId);
  formData.append('measurement_index', measurementIndex.toString());
  formData.append('name', name);

  fetch('/admin/measurement_name', {
    method: 'POST',
    headers: { 'X-Requested-With': 'XMLHttpRequest', 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ sensor: sensorId, measurement: idx, name: name })
  })
  .then(parseJsonResponse)
  .then(data => {
    if (data.success) {
      showSuccessMessage('Messwert-Name erfolgreich aktualisiert');
    } else {
      showErrorMessage('Fehler beim Aktualisieren des Namens: ' + (data.error || 'Unbekannter Fehler'));
    }
  })
  .catch(error => {
    console.error('Error updating measurement name:', error);
    showErrorMessage('Netzwerkfehler beim Aktualisieren des Namens');
  });
}

// --- Debounce logic for backend updates ---
const debounceTimers = {};
const debounceCallbacks = {};

function debounceThresholdChange(input, sensorId, idx) {
  const key = sensorId + '_' + idx;
  if (debounceTimers[key]) clearTimeout(debounceTimers[key]);
  debounceCallbacks[key] = () => {
    console.log('[DEBUG] Dispatching debounced change event for', sensorId, idx, 'value:', input.value);
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };
  debounceTimers[key] = setTimeout(() => {
    debounceCallbacks[key]();
    delete debounceTimers[key];
    delete debounceCallbacks[key];
  }, 2000);
}

function flushDebounceThresholdChange(input, sensorId, idx) {
  const key = sensorId + '_' + idx;
  if (debounceTimers[key]) {
    clearTimeout(debounceTimers[key]);
    if (debounceCallbacks[key]) debounceCallbacks[key]();
    delete debounceTimers[key];
    delete debounceCallbacks[key];
  }
}

// --- Fixed Interactive Threshold Slider Rendering ---
function renderInteractiveThresholdSlider({ container, min, max, thresholds, measuredValue, inputRefs, sensorId }) {
  container.innerHTML = '';
  container.classList.add('threshold-slider-container');

  console.log(`[renderInteractiveThresholdSlider] ${sensorId}: min=${min}, max=${max}, thresholds=[${thresholds.join(', ')}], measured=${measuredValue}`);

  // Validate inputs
  if (max <= min) {
    console.error(`[renderInteractiveThresholdSlider] Invalid range: min=${min}, max=${max}`);
    return;
  }

  // Helper to get position in % - FIXED CALCULATION
  function pos(val) {
    if (val < min) {
      console.warn(`[renderInteractiveThresholdSlider] Value ${val} is below min ${min}, clamping to 0%`);
      return 0;
    }
    if (val > max) {
      console.warn(`[renderInteractiveThresholdSlider] Value ${val} is above max ${max}, clamping to 100%`);
      return 100;
    }
    const percentage = ((val - min) / (max - min)) * 100;
    console.debug(`[renderInteractiveThresholdSlider] pos(${val}) = ${percentage.toFixed(1)}% (range: ${min}-${max})`);
    return percentage;
  }

  function clamp(val, idx) {
    if (idx > 0) val = Math.max(val, thresholds[idx - 1] + 1);
    if (idx < thresholds.length - 1) val = Math.min(val, thresholds[idx + 1] - 1);
    return Math.max(min, Math.min(max, val));
  }

  // Helper to get the correct gradient class for each threshold (transition)
  function getLabelClass(idx) {
    if (idx === 0) return 'threshold-label-yellowlow'; // red to yellow
    if (idx === 1) return 'threshold-label-greenlow'; // yellow to green
    if (idx === 2) return 'threshold-label-greenhigh'; // green to yellow
    if (idx === 3) return 'threshold-label-yellowhigh'; // yellow to red
    return '';
  }

  // Validate thresholds are in ascending order
  for (let i = 1; i < thresholds.length; i++) {
    if (thresholds[i] <= thresholds[i-1]) {
      console.error(`[renderInteractiveThresholdSlider] Thresholds not in ascending order: ${thresholds.join(', ')}`);
      return;
    }
  }

  // Dynamic color stops for the bar
  const stops = [
    { color: '#e53935', pct: 0 }, // min
    { color: '#e53935', pct: pos(thresholds[0]) }, // yellowLow
    { color: '#fbc02d', pct: pos(thresholds[0]) },
    { color: '#fbc02d', pct: pos(thresholds[1]) }, // greenLow
    { color: '#43a047', pct: pos(thresholds[1]) },
    { color: '#43a047', pct: pos(thresholds[2]) }, // greenHigh
    { color: '#fbc02d', pct: pos(thresholds[2]) },
    { color: '#fbc02d', pct: pos(thresholds[3]) }, // yellowHigh
    { color: '#e53935', pct: pos(thresholds[3]) },
    { color: '#e53935', pct: 100 }
  ];

  const bar = document.createElement('div');
  bar.className = 'threshold-gradient-bar';
  bar.style.background = `linear-gradient(90deg, ${stops.map(s => `${s.color} ${s.pct}%`).join(', ')})`;
  container.appendChild(bar);

  // Store handles and labels for later updates
  const handles = [];
  const labels = [];

  for (let idx = 0; idx < 4; idx++) {
    const val = thresholds[idx];
    const percentage = pos(val);

    console.debug(`[renderInteractiveThresholdSlider] Threshold ${idx}: value=${val}, position=${percentage}%`);

    // Handle (dot)
    const handle = document.createElement('div');
    handle.className = `threshold-handle`;
    handle.style.left = `${percentage}%`;
    handle.style.touchAction = 'none';
    handle.style.cursor = 'pointer';
    container.appendChild(handle);
    handles.push(handle);

    // Label (sign)
    const label = document.createElement('div');
    label.className = `threshold-label ${getLabelClass(idx)}`;
    label.textContent = val;
    label.style.left = `${percentage}%`;
    label.style.cursor = 'pointer';
    label.style.pointerEvents = 'auto'; // allow dragging
    container.appendChild(label);
    labels.push(label);

    // --- Drag logic (shared for handle and label) ---
    let dragging = false;
    function onMove(e) {
      if (!dragging) return;
      const rect = container.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      let percent = (clientX - rect.left) / rect.width;
      percent = Math.max(0, Math.min(1, percent));
      let newVal = min + percent * (max - min);
      newVal = Math.round(newVal);
      newVal = clamp(newVal, idx);
      thresholds[idx] = newVal;

      const newPercentage = pos(newVal);
      handle.style.left = `${newPercentage}%`;
      label.style.left = `${newPercentage}%`;
      label.textContent = newVal;
      // Remove all gradient classes and add the correct one
      label.className = `threshold-label ${getLabelClass(idx)}`;

      if (inputRefs && inputRefs[idx]) {
        inputRefs[idx].value = newVal;
        // --- NEW: Dispatch input and change events to trigger backend update ---
        inputRefs[idx].dispatchEvent(new Event('input', { bubbles: true }));
        inputRefs[idx].dispatchEvent(new Event('change', { bubbles: true }));
        debounceThresholdChange(inputRefs[idx], sensorId, idx);
      }

      // Update bar gradient dynamically
      const newStops = [
        { color: '#e53935', pct: 0 },
        { color: '#e53935', pct: pos(thresholds[0]) },
        { color: '#fbc02d', pct: pos(thresholds[0]) },
        { color: '#fbc02d', pct: pos(thresholds[1]) },
        { color: '#43a047', pct: pos(thresholds[1]) },
        { color: '#43a047', pct: pos(thresholds[2]) },
        { color: '#fbc02d', pct: pos(thresholds[2]) },
        { color: '#fbc02d', pct: pos(thresholds[3]) },
        { color: '#e53935', pct: pos(thresholds[3]) },
        { color: '#e53935', pct: 100 }
      ];
      bar.style.background = `linear-gradient(90deg, ${newStops.map(s => `${s.color} ${s.pct}%`).join(', ')})`;
    }

    function onUp() {
      dragging = false;
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('touchmove', onMove);
      document.removeEventListener('mouseup', onUp);
      document.removeEventListener('touchend', onUp);
      // On drag end, flush the debounced change event for backend update
      if (inputRefs && inputRefs[idx]) {
        flushDebounceThresholdChange(inputRefs[idx], sensorId, idx);
      }
    }

    // Attach drag logic to both handle and label
    [handle, label].forEach(el => {
      el.addEventListener('mousedown', e => {
        e.preventDefault(); dragging = true;
        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup', onUp);
      });
      el.addEventListener('touchstart', e => {
        e.preventDefault(); dragging = true;
        document.addEventListener('touchmove', onMove);
        document.addEventListener('touchend', onUp);
      });
    });
  }

  // --- FIXED: Measured value marker and label update ---
  if (typeof measuredValue === 'number' && !isNaN(measuredValue)) {
    const measuredPercentage = pos(measuredValue);
    console.debug(`[renderInteractiveThresholdSlider] Measured value ${measuredValue} at ${measuredPercentage}%`);

    // Create the dot ON the bar (not above or below)
    const marker = document.createElement('div');
    marker.className = 'measured-value-dot';
    marker.style.left = `${measuredPercentage}%`;
    container.appendChild(marker);

    // Create the label BELOW the bar
    const valueLabel = document.createElement('div');
    valueLabel.className = 'measured-value-label';
    valueLabel.textContent = measuredValue.toFixed(2);
    valueLabel.style.left = `${measuredPercentage}%`;
    container.appendChild(valueLabel);
  }

  // --- Absolute min/max markers ---
  // Get absolute min/max values from the sensor configuration
  const sensorConfig = initialMeasurementValues[`${sensorId}_${inputRefs ? inputRefs.length - 1 : 0}`];
  if (sensorConfig) {
    if (typeof sensorConfig.absoluteMin !== 'undefined' && sensorConfig.absoluteMin !== Infinity) {
      const absMinPercentage = pos(sensorConfig.absoluteMin);
      const absMinMarker = document.createElement('div');
      absMinMarker.className = 'absolute-min-marker';
      absMinMarker.style.left = `${absMinPercentage}%`;
      absMinMarker.title = `Min: ${sensorConfig.absoluteMin.toFixed(2)}`;
      container.appendChild(absMinMarker);

      const absMinLabel = document.createElement('div');
      absMinLabel.className = 'absolute-min-label';
      absMinLabel.textContent = `Min: ${sensorConfig.absoluteMin.toFixed(2)}`;
      absMinLabel.style.left = `${absMinPercentage}%`;
      container.appendChild(absMinLabel);
    }

    if (typeof sensorConfig.absoluteMax !== 'undefined' && sensorConfig.absoluteMax !== -Infinity) {
      const absMaxPercentage = pos(sensorConfig.absoluteMax);
      const absMaxMarker = document.createElement('div');
      absMaxMarker.className = 'absolute-max-marker';
      absMaxMarker.style.left = `${absMaxPercentage}%`;
      absMaxMarker.title = `Max: ${sensorConfig.absoluteMax.toFixed(2)}`;
      container.appendChild(absMaxMarker);

      const absMaxLabel = document.createElement('div');
      absMaxLabel.className = 'absolute-max-label';
      absMaxLabel.textContent = `Max: ${sensorConfig.absoluteMax.toFixed(2)}`;
      absMaxLabel.style.left = `${absMaxPercentage}%`;
      container.appendChild(absMaxLabel);
    }
  }

  // --- Input sync logic ---
  if (inputRefs) {
    inputRefs.forEach((input, idx) => {
      if (input) {
        input.addEventListener('input', function() {
          const val = parseFloat(input.value);
          if (!isNaN(val)) {
            thresholds[idx] = clamp(val, idx);
            const newPercentage = pos(thresholds[idx]);
            handles[idx].style.left = `${newPercentage}%`;
            labels[idx].style.left = `${newPercentage}%`;
            labels[idx].textContent = thresholds[idx];
            // Remove all gradient classes and add the correct one
            labels[idx].className = `threshold-label ${getLabelClass(idx)}`;

            // Update bar gradient dynamically
            const newStops = [
              { color: '#e53935', pct: 0 },
              { color: '#e53935', pct: pos(thresholds[0]) },
              { color: '#fbc02d', pct: pos(thresholds[0]) },
              { color: '#fbc02d', pct: pos(thresholds[1]) },
              { color: '#43a047', pct: pos(thresholds[1]) },
              { color: '#43a047', pct: pos(thresholds[2]) },
              { color: '#fbc02d', pct: pos(thresholds[2]) },
              { color: '#fbc02d', pct: pos(thresholds[3]) },
              { color: '#e53935', pct: pos(thresholds[3]) },
              { color: '#e53935', pct: 100 }
            ];
            bar.style.background = `linear-gradient(90deg, ${newStops.map(s => `${s.color} ${s.pct}%`).join(', ')})`;
            debounceThresholdChange(input, sensorId, idx);
          }
        });
      }
    });
  }
}

// --- Fixed Threshold Slider Initialization ---
function initializeThresholdSliders(sensorConfigData) {
  if (!sensorConfigData || !sensorConfigData.sensors) {
    console.warn('[admin_sensors.js] initializeThresholdSliders: No sensors found in config', sensorConfigData);
    return;
  }
  console.log('[admin_sensors.js] Initializing threshold sliders for sensors:', Object.keys(sensorConfigData.sensors));

  Object.values(sensorConfigData.sensors).forEach(sensor => {
    if (!sensor.measurements) {
      console.warn('[admin_sensors.js] Sensor has no measurements:', sensor);
      return;
    }

    sensor.measurements.forEach((meas, i) => {
      let min, max;
      const name = (meas.name || '').toLowerCase();
      const id = (sensor.id + '_' + i).toLowerCase();
      const unit = (meas.unit || '').toLowerCase();

      // Get threshold values to calculate dynamic range
      const thresholds = [
        meas.thresholds.yellowLow,
        meas.thresholds.greenLow,
        meas.thresholds.greenHigh,
        meas.thresholds.yellowHigh
      ];

      const minThreshold = Math.min(...thresholds);
      const maxThreshold = Math.max(...thresholds);
      const thresholdRange = maxThreshold - minThreshold;

      // Determine scale type and set appropriate min/max
      if (name.includes('hum') || name.includes('%') || id.includes('hum') || unit.includes('%')) {
        // Percentage scales: always 0-100%
        min = 0;
        max = 100;
        console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Percentage scale 0-100%`);
      } else {
        // Non-percentage scales: 20% buffer around threshold range
        const buffer = thresholdRange * 0.2;
        min = Math.floor(minThreshold - buffer);
        max = Math.ceil(maxThreshold + buffer);

        // Special handling for temperature scales
        if (name.includes('temp') || name.includes('°c') || id.includes('temp')) {
          // Ensure temperature scale makes sense (don't go too low)
          min = Math.max(min, -40);
          max = Math.max(max, 50); // Ensure reasonable max
          console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Temperature scale ${min}-${max}°C`);
        } else if (name.includes('co2') || name.includes('ppm') || id.includes('co2')) {
          // CO2 should never go below 0
          min = Math.max(min, 0);
          console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: CO2 scale ${min}-${max} ppm`);
        } else if (name.includes('pm') || name.includes('partikel') || id.includes('pm')) {
          // Particulate matter should never go below 0
          min = Math.max(min, 0);
          console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: PM scale ${min}-${max}`);
        } else {
          console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Generic scale ${min}-${max}`);
        }
      }

      // Override with explicit min/max from config if present
      if (typeof meas.min !== 'undefined') {
        min = meas.min;
        console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Using config min: ${min}`);
      }
      if (typeof meas.max !== 'undefined') {
        max = meas.max;
        console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Using config max: ${max}`);
      }

      // Include absolute min/max values in the range calculation if they extend beyond current range
      if (typeof meas.absoluteMin !== 'undefined' && meas.absoluteMin !== Infinity) {
        min = Math.min(min, meas.absoluteMin);
        console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Extended min to absolute min: ${min}`);
      }
      if (typeof meas.absoluteMax !== 'undefined' && meas.absoluteMax !== -Infinity) {
        max = Math.max(max, meas.absoluteMax);
        console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Extended max to absolute max: ${max}`);
      }

      // Validate that thresholds fit within the calculated range
      if (minThreshold < min || maxThreshold > max) {
        console.warn(`[initializeThresholdSliders] ${sensor.id}_${i}: Thresholds (${minThreshold}-${maxThreshold}) don't fit in scale (${min}-${max}). Adjusting scale.`);
        min = Math.min(min, minThreshold - 5);
        max = Math.max(max, maxThreshold + 5);
      }

      const inputNames = ['yellowLow', 'greenLow', 'greenHigh', 'yellowHigh'];
      const inputRefs = inputNames.map((thName) =>
        document.querySelector(`input[name='${sensor.id}_${i}_${thName}']`)
      );
      const container = document.getElementById('threshold_' + sensor.id + '_' + i);

      if (container) {
        // Get the measured value from the parent container's data attribute
        const sliderContainer = container.parentElement;
        let measuredValue = null;
        if (sliderContainer && sliderContainer.classList.contains('threshold-slider-container')) {
          const lastValueAttr = sliderContainer.getAttribute('data-last-value');
          if (lastValueAttr !== null && !isNaN(parseFloat(lastValueAttr))) {
            measuredValue = parseFloat(lastValueAttr);
            console.debug(`[initializeThresholdSliders] ${sensor.id}_${i}: Using measured value: ${measuredValue}`);
          }
        }

        // Store min/max as data attributes for update functions
        container.setAttribute('data-min', min);
        container.setAttribute('data-max', max);

        console.log(`[initializeThresholdSliders] ${sensor.id}_${i}: Final scale ${min}-${max}, thresholds [${thresholds.join(', ')}], measured: ${measuredValue}`);

        renderInteractiveThresholdSlider({
          container: container,
          min: min,
          max: max,
          thresholds: thresholds,
          measuredValue: measuredValue,
          inputRefs: inputRefs,
          sensorId: sensor.id
        });
      } else {
        console.warn('[admin_sensors.js] No container found for slider:', 'threshold_' + sensor.id + '_' + i);
      }
    });
  });
}

// NOTE: showSuccessMessage / showErrorMessage are provided globally in admin.js

// --- Measured value sign and blue dot update logic for all measurements ---
function observeAllMeasuredValues(sensorId, nMeasurements, min, max) {
  for (let i = 0; i < nMeasurements; i++) {
    const inputSelector = `input[data-sensor='${sensorId}_${i}']`;
    const markerSelector = `#threshold_${sensorId}_${i}`;
    const input = document.querySelector(inputSelector);
    const markerContainer = document.querySelector(markerSelector)?.parentElement;
    if (!input || !markerContainer) continue;
    const valueDot = markerContainer.querySelector('.measured-value-dot');
    const valueLabel = markerContainer.querySelector('.measured-value-label');
    if (!valueDot || !valueLabel) continue;
    const update = () => {
      const val = parseFloat(input.value);
      if (!isNaN(val)) {
        const percent = ((val - min) / (max - min)) * 100;
        valueDot.style.left = `${percent}%`;
        valueLabel.style.left = `${percent}%`;
        valueLabel.textContent = val.toFixed(2);
      }
    };
    input.addEventListener('input', update);
    input.addEventListener('change', update);
    const observer = new MutationObserver(update);
    observer.observe(input, { attributes: true, childList: true, subtree: true, characterData: true });
  }
}

// --- Ensure backend update handler is attached to all threshold input fields ---
document.addEventListener('DOMContentLoaded', function() {
  document.querySelectorAll('.threshold-input').forEach(input => {
    input.addEventListener('change', function(e) {
      console.log('[DEBUG] Threshold input change event fired for', input.name, 'value:', input.value);
      // Existing backend update logic here (e.g., updateThresholds call)
    });
  });
});
