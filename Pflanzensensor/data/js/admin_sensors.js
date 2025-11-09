/*
 * admin_sensors.js (ES5-compatible refactor)
 * - avoids ES6+ syntax (no class, no arrow functions, no template strings)
 * - uses fetch when available, falls back to XMLHttpRequest
 * - sends server-expected parameter names (sensor_id, measurement_index, min, max, enabled, thresholds CSV)
 * - robustly handles measurements as array or object
 * - exposes window.AdminSensors for testing
 */

// ------------------------------
// LOGGING
// ------------------------------
var LOG_LEVELS = { DEBUG: 0, INFO: 1, WARN: 2, ERROR: 3 };
var Logger = (function () {
  var level = (typeof window !== 'undefined' && typeof window.ADMIN_JS_LOG_LEVEL !== 'undefined') ? window.ADMIN_JS_LOG_LEVEL : LOG_LEVELS.DEBUG;

  function debug() {
    if (level <= LOG_LEVELS.DEBUG && typeof console !== 'undefined' && console.debug) {
      console.debug.apply(console, ['[ADMIN]'].concat(Array.prototype.slice.call(arguments)));
    }
  }
  function info() {
    if (level <= LOG_LEVELS.INFO && typeof console !== 'undefined' && console.log) {
      console.log.apply(console, ['[ADMIN]'].concat(Array.prototype.slice.call(arguments)));
    }
  }
  function warn() {
    if (level <= LOG_LEVELS.WARN && typeof console !== 'undefined' && console.warn) {
      console.warn.apply(console, ['[ADMIN]'].concat(Array.prototype.slice.call(arguments)));
    }
  }
  function error() {
    if (typeof console !== 'undefined' && console.error) {
      console.error.apply(console, ['[ADMIN]'].concat(Array.prototype.slice.call(arguments)));
    }
  }

  // expose level and functions
  var api = { debug: debug, info: info, warn: warn, error: error };
  if (typeof window !== 'undefined') {
    window.ADMIN_JS_LOG_LEVELS = LOG_LEVELS;
    try { window.ADMIN_JS_LOG_LEVEL = window.ADMIN_JS_LOG_LEVEL; } catch (e) { /* ignore */ }
    window.Logger = api;
  }
  return api;
}());

// ------------------------------
// UTILITIES
// ------------------------------
var Debouncer = (function () {
  var timers = {};
  return {
    debounce: function (key, cb, delay) {
      delay = (typeof delay === 'number') ? delay : 1000;
      if (timers[key]) clearTimeout(timers[key]);
      timers[key] = setTimeout(function () {
        try { cb(); } catch (e) { Logger.error('Debounce callback error', e); }
        delete timers[key];
      }, delay);
    },
    flush: function (key) {
      if (timers[key]) {
        clearTimeout(timers[key]);
        delete timers[key];
      }
    },
    flushAll: function () {
      var k;
      for (k in timers) if (timers.hasOwnProperty(k)) this.flush(k);
    }
  };
}());

var DOMUtils = (function () {
  function toArray(nodeList) {
    try { return Array.prototype.slice.call(nodeList); }
    catch (e) { // old IE fallback
      var arr = [];
      for (var i = 0; i < nodeList.length; i++) arr.push(nodeList[i]);
      return arr;
    }
  }

  function flashElement(el) {
    if (!el) return;
    try {
      el.classList.remove('flash');
      void el.offsetWidth; // force reflow
      el.classList.add('flash');
      setTimeout(function () { el.classList.remove('flash'); }, 300);
    } catch (e) { /* ignore classList errors on very old browsers */ }
  }

  function qs(sel, ctx) { ctx = ctx || document; return ctx.querySelector(sel); }
  function qsa(sel, ctx) { ctx = ctx || document; return toArray(ctx.querySelectorAll(sel)); }

  function elementMatches(el, selector) {
    if (!el) return false;
    var fn = el.matches || el.msMatchesSelector || el.webkitMatchesSelector || el.mozMatchesSelector || el.oMatchesSelector;
    if (fn) return fn.call(el, selector);
    // very old fallback: try comparing nodes from querySelectorAll
    var nodes = document.querySelectorAll(selector);
    for (var i = 0; i < nodes.length; i++) if (nodes[i] === el) return true;
    return false;
  }

  return { flashElement: flashElement, querySelector: qs, querySelectorAll: qsa, elementMatches: elementMatches };
}());

// ------------------------------
// API helper: fetch with XHR fallback and URL-encoding utilities
// ------------------------------
var API = (function () {
  function buildUrlEncoded(obj) {
    if (!obj) return '';
    if (typeof URLSearchParams !== 'undefined') {
      var p = new URLSearchParams();
      for (var k in obj) if (obj.hasOwnProperty(k)) p.append(k, obj[k]);
      return p.toString();
    }
    var parts = [];
    for (var key in obj) if (obj.hasOwnProperty(key)) {
      var v = obj[key];
      parts.push(encodeURIComponent(key) + '=' + encodeURIComponent(v));
    }
    return parts.join('&');
  }

  function post(url, data) {
    // Returns a Promise that resolves to fetch Response-like object or rejects on network error
    var useFetch = (typeof fetch === 'function');
    var headers = { 'X-Requested-With': 'XMLHttpRequest' };

    if (data instanceof FormData) {
      if (!data.get('ajax')) data.append('ajax', '1');
      if (useFetch) return fetch(url, { method: 'POST', credentials: 'include', body: data, headers: headers });
      // XHR with FormData
      return new Promise(function (resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', url);
        xhr.withCredentials = true;
        // DON'T set Content-Type for FormData
        for (var h in headers) if (headers.hasOwnProperty(h)) try { xhr.setRequestHeader(h, headers[h]); } catch (e) { }
        xhr.onload = function () {
          // Provide a minimal Response-like object for parseJsonResponse compatibility
          resolve({
            status: xhr.status,
            headers: { get: function (name) { try { return xhr.getResponseHeader(name); } catch (e) { return null; } } },
            text: function () { return Promise.resolve(xhr.responseText); },
            json: function () { try { return Promise.resolve(JSON.parse(xhr.responseText)); } catch (e) { return Promise.reject(e); } }
          });
        };
        xhr.onerror = function () { reject(new Error('Network error')); };
        xhr.send(data);
      });
    }

    // For objects or URLSearchParams, convert to urlencoded string
    var bodyString = null;
    if (typeof URLSearchParams !== 'undefined' && (data instanceof URLSearchParams)) {
      if (!data.has('ajax')) data.append('ajax', '1');
      bodyString = data.toString();
    } else if (typeof data === 'object' && data !== null) {
      // plain object
      if (!data.hasOwnProperty('ajax')) data.ajax = '1';
      if (typeof URLSearchParams !== 'undefined') {
        var params = new URLSearchParams();
        for (var k2 in data) if (data.hasOwnProperty(k2)) params.append(k2, data[k2]);
        bodyString = params.toString();
      } else {
        bodyString = buildUrlEncoded(data);
      }
    } else if (typeof data === 'string') {
      bodyString = data;
    }

    headers['Content-Type'] = 'application/x-www-form-urlencoded';

    if (useFetch) {
      return fetch(url, { method: 'POST', credentials: 'include', headers: headers, body: bodyString });
    }

    // XHR fallback returning Promise
    return new Promise(function (resolve, reject) {
      var xhr2 = new XMLHttpRequest();
      xhr2.open('POST', url);
      xhr2.withCredentials = true;
      for (var hh in headers) if (headers.hasOwnProperty(hh)) try { xhr2.setRequestHeader(hh, headers[hh]); } catch (e) { }
      xhr2.onload = function () {
        resolve({
          status: xhr2.status,
          headers: { get: function (name) { try { return xhr2.getResponseHeader(name); } catch (e) { return null; } } },
          text: function () { return Promise.resolve(xhr2.responseText); },
          json: function () { try { return Promise.resolve(JSON.parse(xhr2.responseText)); } catch (e) { return Promise.reject(e); } }
        });
      };
      xhr2.onerror = function () { reject(new Error('Network error')); };
      xhr2.send(bodyString);
    });
  }

  function request(endpoint, params, successMsg) {
    return post(endpoint, params).then(function (response) {
      return parseJsonResponse(response).then(function (data) {
        if (data && data.success) {
          if (successMsg && typeof showSuccessMessage === 'function') showSuccessMessage(successMsg);
        } else if (data) {
          if (typeof showErrorMessage === 'function') showErrorMessage('Fehler: ' + (data.error || 'Unbekannter Fehler'));
        }
        return data;
      });
    }).catch(function (err) {
      Logger.error('API error for ' + endpoint + ':', err);
      if (typeof showErrorMessage === 'function') showErrorMessage('Netzwerkfehler beim Speichern');
      throw err;
    });
  }

  return { post: post, request: request };
}());

// ------------------------------
// STATE
// ------------------------------
var AppState = (function () {
  var state = { sensors: {}, initialConfig: {} };
  return {
    setInitialConfig: function (config) { state.initialConfig = config; Logger.debug('Initial config set'); },
    getInitialValue: function (sensorId, measurementIndex, key) {
      var sensorKey = sensorId + '_' + measurementIndex;
      var s = state.initialConfig[sensorKey];
      return s ? s[key] : undefined;
    },
    setSensorValue: function (sensorKey, updates) { state.sensors[sensorKey] = state.sensors[sensorKey] || {}; for (var k in updates) if (updates.hasOwnProperty(k)) state.sensors[sensorKey][k] = updates[k]; },
    getSensorValue: function (sensorKey) { return state.sensors[sensorKey]; },
    _internal: state
  };
}());

// ------------------------------
// DISPLAY UPDATER
// ------------------------------
var DisplayUpdater = (function () {
  function updateAbsoluteMinMax(sensorId, measurementIndex, absoluteMin, absoluteMax) {
    var minInput = DOMUtils.querySelector('.absolute-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    if (minInput) {
      if (typeof absoluteMin !== 'undefined' && absoluteMin !== Infinity) minInput.value = parseFloat(absoluteMin).toFixed(2); else minInput.value = '--';
    }
    var maxInput = DOMUtils.querySelector('.absolute-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    if (maxInput) {
      if (typeof absoluteMax !== 'undefined' && absoluteMax !== -Infinity) maxInput.value = parseFloat(absoluteMax).toFixed(2); else maxInput.value = '--';
    }
    updateSliderAbsoluteMarkers(sensorId, measurementIndex, absoluteMin, absoluteMax);
  }

  function updateSliderAbsoluteMarkers(sensorId, measurementIndex, absoluteMin, absoluteMax) {
    var sliderContainer = DOMUtils.querySelector(' #threshold_' + sensorId + '_' + measurementIndex);
    if (!sliderContainer) return;
    var rangeMin = parseFloat(sliderContainer.getAttribute('data-min')) || 0;
    var rangeMax = parseFloat(sliderContainer.getAttribute('data-max')) || 100;
    if (!isFinite(rangeMin)) rangeMin = 0;
    if (!isFinite(rangeMax) || rangeMax <= rangeMin) rangeMax = rangeMin + 100;
    function toPercent(v) {
      if (!isFinite(v)) return null;
      var p = ((v - rangeMin) / (rangeMax - rangeMin)) * 100;
      if (isNaN(p)) return null;
      return Math.max(0, Math.min(100, p));
    }
    updateMarker(sliderContainer, sensorId, measurementIndex, 'min', absoluteMin, toPercent, Infinity);
    updateMarker(sliderContainer, sensorId, measurementIndex, 'max', absoluteMax, toPercent, -Infinity);
  }

  function updateMarker(container, sensorId, measurementIndex, type, value, toPercent, invalidValue) {
    var exists = (typeof value !== 'undefined' && value !== invalidValue);
    var val = exists ? parseFloat(value) : null;
    var markerClass = 'absolute-' + type + '-marker';
    var labelClass = 'absolute-' + type + '-label';
    var marker = container.querySelector('.' + markerClass + '[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    var label = container.querySelector('.' + labelClass + '[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    if (exists && !isNaN(val)) {
      var pct = toPercent(val);
      var icon = (type === 'min') ? '🔽' : '🔼';
      if (!marker) {
        marker = document.createElement('div');
        marker.className = markerClass;
        marker.setAttribute('data-sensor-id', sensorId);
        marker.setAttribute('data-measurement-index', measurementIndex);
        container.appendChild(marker);
      }
      if (!label) {
        label = document.createElement('div');
        label.className = labelClass;
        label.setAttribute('data-sensor-id', sensorId);
        label.setAttribute('data-measurement-index', measurementIndex);
        container.appendChild(label);
      }
      if (pct !== null) {
        marker.style.left = pct + '%';
        label.style.left = pct + '%';
      }
      marker.title = icon + val.toFixed(2);
      label.textContent = icon + val.toFixed(2);
    } else {
      if (marker && marker.parentNode) marker.parentNode.removeChild(marker);
      if (label && label.parentNode) label.parentNode.removeChild(label);
    }
  }

  function updateAbsoluteRawMinMax(sensorId, measurementIndex, absoluteRawMin, absoluteRawMax) {
    var rawMinNum = parseInt(absoluteRawMin, 10);
    var rawMaxNum = parseInt(absoluteRawMax, 10);
    var minInput = DOMUtils.querySelector('.absolute-raw-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    if (minInput) {
      if (typeof absoluteRawMin !== 'undefined' && !isNaN(rawMinNum) && rawMinNum !== 2147483647) minInput.value = absoluteRawMin; else minInput.value = '--';
    }
    var maxInput = DOMUtils.querySelector('.absolute-raw-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    if (maxInput) {
      if (typeof absoluteRawMax !== 'undefined' && !isNaN(rawMaxNum) && rawMaxNum !== -2147483648) maxInput.value = absoluteRawMax; else maxInput.value = '--';
    }
  }

  function updateAnalogCalculationMinMax(sensorId, measurementIndex, minValue, maxValue) {
    var minInput = DOMUtils.querySelector('.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    var maxInput = DOMUtils.querySelector('.analog-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
    try {
      var floatMin = (typeof minValue !== 'undefined' && minValue !== null) ? Number(minValue) : null;
      var floatMax = (typeof maxValue !== 'undefined' && maxValue !== null) ? Number(maxValue) : null;
      var isAnalog = sensorId && sensorId.toLowerCase().indexOf('analog') === 0;
      var container = DOMUtils.querySelector('#threshold_' + sensorId + '_' + measurementIndex);

      if (isAnalog) {
        // Always render the slider as 0..100% range
        if (container) {
          container.setAttribute('data-min', '0');
          container.setAttribute('data-max', '100');
        }

        // If autocal checkbox exists and is unchecked, the user is in manual
        // mode — do not overwrite the manual min/max inputs from periodic updates.
        var autocalCheckbox = DOMUtils.querySelector('.analog-autocal-checkbox[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
        if (autocalCheckbox && autocalCheckbox.checked === false) {
          return;
        }

        // Autocal is enabled (or no checkbox present): apply device-sent calc limits
        if (minInput && maxInput && floatMin !== null && floatMax !== null && !isNaN(floatMin) && !isNaN(floatMax) && floatMax > floatMin) {
          // Update raw numeric inputs (ADC/raw values) so the UI reflects runtime autocal
          minInput.value = Math.round(floatMin);
          maxInput.value = Math.round(floatMax);
        }
      } else {
        // Non-analog sensors: treat min/max as numeric range for the slider
        if (minInput && maxInput && floatMin !== null && floatMax !== null && !isNaN(floatMin) && !isNaN(floatMax) && floatMax > floatMin) {
          minInput.value = Math.round(floatMin);
          maxInput.value = Math.round(floatMax);
          if (container) {
            container.setAttribute('data-min', String(floatMin));
            container.setAttribute('data-max', String(floatMax));
          }
        }
      }
    } catch (e) { Logger.error('updateAnalogCalculationMinMax failed', e); }
  }

  function updateCalibrationMode(sensorId, measurementIndex, calibrationMode) {
    // Update autocal checkbox state and enable/disable the analog min/max inputs
    var selector = '.analog-autocal-checkbox[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]';
    var checkbox = DOMUtils.querySelector(selector);
    var shouldBeChecked = !!calibrationMode;
    if (checkbox) {
      try {
        if (checkbox.checked !== shouldBeChecked) checkbox.checked = shouldBeChecked;
      } catch (e) { Logger.debug('Failed to update autocal checkbox checked state', e); }
    }

    // Enable or disable the manual raw min/max inputs immediately so the user
    // can edit them after disabling autocal without reloading the page.
    try {
      var minInput = DOMUtils.querySelector('.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      var maxInput = DOMUtils.querySelector('.analog-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      if (minInput) {
        minInput.disabled = shouldBeChecked;
        if (shouldBeChecked) {
          minInput.classList.add('readonly-value');
        } else {
          // only remove readonly-value if it wasn't originally readonly (we assume it's not read-only in template)
          minInput.classList.remove('readonly-value');
        }
      }
      if (maxInput) {
        maxInput.disabled = shouldBeChecked;
        if (shouldBeChecked) {
          maxInput.classList.add('readonly-value');
        } else {
          maxInput.classList.remove('readonly-value');
        }
      }
      // Also update any visible slider input refs created by ThresholdSlider
      var sliderContainer = DOMUtils.querySelector('#threshold_' + sensorId + '_' + measurementIndex);
      if (sliderContainer) {
        // If the slider created inputRefs and they exist in DOM, enable/disable them too
        var sliderInputs = sliderContainer.querySelectorAll('input.threshold-input, input[type="number"]');
        for (var i = 0; i < sliderInputs.length; i++) {
          try {
            sliderInputs[i].disabled = shouldBeChecked;
            if (shouldBeChecked) {
              sliderInputs[i].classList.add('readonly-value');
            } else {
              sliderInputs[i].classList.remove('readonly-value');
            }
          } catch (e) { /* ignore */ }
        }
      }
    } catch (e) { Logger.error('Failed to update calibration mode UI controls', e); }
  }

  return {
    updateAbsoluteMinMax: updateAbsoluteMinMax,
    updateAbsoluteRawMinMax: updateAbsoluteRawMinMax,
    updateAnalogCalculationMinMax: updateAnalogCalculationMinMax,
    updateCalibrationMode: updateCalibrationMode
  };
}());

// ------------------------------
// SENSOR CONFIG API (server param names verified from C++ handlers)
// ------------------------------
var SensorConfigAPI = (function () {
  function updateFlowerStatus(sensor) {
    // Use unified setConfigValue method
    return setConfigValue('general', 'flower_sens', sensor, 'string');
  }

  function updateMeasurementInterval(sensorId, interval) {
    return API.request('/admin/measurement_interval', { sensor_id: sensorId, interval: '' + interval }, 'Messungsintervall erfolgreich auf ' + interval + ' Sekunden aktualisiert');
  }

  function updateAnalogMinMax(sensorId, measurementIndex, minValue, maxValue) {
    return API.request('/admin/analog_minmax', { sensor_id: sensorId, measurement_index: '' + measurementIndex, min: '' + minValue, max: '' + maxValue }, 'Min/Max Werte erfolgreich aktualisiert');
  }

  function updateAnalogInverted(sensorId, measurementIndex, inverted) {
    // Use unified setConfigValue method
    var namespace = 's_' + sensorId;
    var key = 'm' + measurementIndex + '_inv';
    return setConfigValue(namespace, key, inverted ? 'true' : 'false', 'bool');
  }

  function updateAnalogAutocal(sensorId, measurementIndex, enabled) {
    if (typeof showInfoMessage === 'function') showInfoMessage('Autokalibrierung wird aktualisiert...');
    return API.request('/admin/analog_autocal', { sensor_id: sensorId, measurement_index: '' + measurementIndex, enabled: enabled ? 'true' : 'false' }, 'Autokalibrierung ' + (enabled ? 'aktiviert' : 'deaktiviert'));
  }

  function updateAnalogAutocalDuration(sensorId, measurementIndex, durationSeconds) {
    if (typeof showInfoMessage === 'function') showInfoMessage('Autokalibrierungsdauer wird gespeichert...');
    return API.request('/admin/analog_autocal_duration', { sensor_id: sensorId, measurement_index: '' + measurementIndex, duration: '' + durationSeconds }, 'Autokalibrierungsdauer aktualisiert');
  }

  function updateThresholds(sensorId, measurementIndex, thresholds) {
    // Server expects a single CSV string 'thresholds' with 4 values
    var csv = [thresholds[0], thresholds[1], thresholds[2], thresholds[3]].join(',');
    return API.request('/admin/thresholds', { sensor_id: sensorId, measurement_index: '' + measurementIndex, thresholds: csv }, 'Schwellwerte erfolgreich aktualisiert');
  }

  function updateMeasurementName(sensorId, measurementIndex, name) {
    return API.request('/admin/measurement_name', { sensor_id: sensorId, measurement_index: '' + measurementIndex, name: name }, 'Sensorname erfolgreich aktualisiert');
  }

  function resetAbsoluteMinMax(sensorId, measurementIndex) {
    return API.request('/admin/reset_absolute_minmax', { sensor_id: sensorId, measurement_index: '' + measurementIndex }, 'Absolute Min/Max Werte zurückgesetzt').then(function (data) {
      if (data && data.success) DisplayUpdater.updateAbsoluteMinMax(sensorId, measurementIndex, Infinity, -Infinity);
      return data;
    });
  }

  function resetAbsoluteRawMinMax(sensorId, measurementIndex) {
    return API.request('/admin/reset_absolute_raw_minmax', { sensor_id: sensorId, measurement_index: '' + measurementIndex }, 'Absolute Raw Min/Max Werte zurückgesetzt').then(function (data) {
      if (data && data.success) DisplayUpdater.updateAbsoluteRawMinMax(sensorId, measurementIndex, 2147483647, -2147483648);
      return data;
    });
  }


  function triggerMeasurement(sensorId, measurementIndex) {
    var params = { sensor_id: sensorId };
    if (typeof measurementIndex !== 'undefined' && measurementIndex !== null && !isNaN(measurementIndex)) params.measurement_index = '' + measurementIndex;
    return API.request('/trigger_measurement', params, null).then(function (data) {
      if (data && !data.success) {
        var err = data.error || data.message || 'Unbekannter Fehler';
        Logger.error('Failed to trigger measurement:', err);
        alert('Fehler beim Auslösen der Messung: ' + err);
      }
      return data;
    });
  }

  return {
    updateFlowerStatus: updateFlowerStatus,
    updateMeasurementInterval: updateMeasurementInterval,
    updateAnalogMinMax: updateAnalogMinMax,
    updateAnalogInverted: updateAnalogInverted,
    updateAnalogAutocal: updateAnalogAutocal,
  updateAnalogAutocalDuration: updateAnalogAutocalDuration,
    updateThresholds: updateThresholds,
    updateMeasurementName: updateMeasurementName,
    resetAbsoluteMinMax: resetAbsoluteMinMax,
    resetAbsoluteRawMinMax: resetAbsoluteRawMinMax,
    triggerMeasurement: triggerMeasurement
  };
}());

// Hook DOM change for autocal duration selects
document.addEventListener('change', function (e) {
  var t = e.target;
  try {
    if (t && t.classList && t.classList.contains('analog-autocal-duration')) {
      var sensorId = t.getAttribute('data-sensor-id');
      var idx = parseInt(t.getAttribute('data-measurement-index'), 10);
      var val = parseInt(t.value, 10);
      if (!isNaN(idx) && sensorId) {
        SensorConfigAPI.updateAnalogAutocalDuration(sensorId, idx, val).then(function (data) {
          if (data && data.success) {
            if (typeof showInfoMessage === 'function') showInfoMessage('Autocal Dauer gespeichert');
          } else {
            alert('Fehler beim Speichern der Autocal Dauer');
          }
        });
      }
    }

    // Live UI update when autocal checkbox toggled
    if (t && t.classList && t.classList.contains('analog-autocal-checkbox')) {
      try {
        var sensorIdCb = t.getAttribute('data-sensor-id');
        var measurementIndexCb = t.getAttribute('data-measurement-index');
        var enabled = !!t.checked;

        // Toggle autocal duration section visibility
        var sel = DOMUtils.querySelector('.analog-autocal-duration[data-sensor-id="' + sensorIdCb + '"][data-measurement-index="' + measurementIndexCb + '"]');
        if (sel) {
          // find the nearest container with class 'autocal-duration-section'
          var container = sel;
          var depth = 0;
          while (container && depth < 6 && !(container.classList && container.classList.contains('autocal-duration-section'))) {
            container = container.parentElement;
            depth++;
          }
          if (container && container.style) container.style.display = enabled ? '' : 'none';
          try { sel.disabled = !enabled; } catch (e) {}
        }

        // Enable/disable min/max inputs
        var minInput = DOMUtils.querySelector('.analog-min-input[data-sensor-id="' + sensorIdCb + '"][data-measurement-index="' + measurementIndexCb + '"]');
        var maxInput = DOMUtils.querySelector('.analog-max-input[data-sensor-id="' + sensorIdCb + '"][data-measurement-index="' + measurementIndexCb + '"]');
        if (minInput) {
          if (enabled) { minInput.classList.remove('readonly-value'); minInput.removeAttribute('disabled'); } else { minInput.classList.add('readonly-value'); minInput.setAttribute('disabled', 'disabled'); }
        }
        if (maxInput) {
          if (enabled) { maxInput.classList.remove('readonly-value'); maxInput.removeAttribute('disabled'); } else { maxInput.classList.add('readonly-value'); maxInput.setAttribute('disabled', 'disabled'); }
        }

        // Let DisplayUpdater handle other UI synchronization
        try { if (typeof DisplayUpdater !== 'undefined' && DisplayUpdater.updateCalibrationMode) DisplayUpdater.updateCalibrationMode(sensorIdCb, measurementIndexCb, enabled); } catch (e) {}
      } catch (err2) { Logger.debug('autocal-checkbox handler failed', err2); }
    }
  } catch (err) { Logger.debug('autocal-duration handler failed', err); }
});

// ------------------------------
// THRESHOLD SLIDER (constructor/prototype for ES5)
// ------------------------------
function ThresholdSlider(container, config) {
  this.container = container;
  this.config = config || {};
  this.thresholds = (config.thresholds || []).slice(0);
  this.handles = [];
  this.labels = [];
  this.init();
}

ThresholdSlider.prototype.init = function () { this.render(); };

ThresholdSlider.prototype.render = function () {
  var container = this.container, cfg = this.config;
  container.innerHTML = '';
  // Note: Do NOT add 'threshold-slider-container' class here!
  // The container already has the correct 'threshold-container' class from HTML.
  // The parent element has 'threshold-slider-container'.
  if (cfg.max <= cfg.min) { Logger.error('Invalid slider range:', cfg); return; }
  if (!this.validateThresholds()) return;
  this.renderGradientBar();
  this.renderHandlesAndLabels();
  this.renderMeasuredValue();
  this.renderAbsoluteMarkers();
  this.attachInputSync();
};

ThresholdSlider.prototype.validateThresholds = function () {
  for (var i = 1; i < this.thresholds.length; i++) { if (this.thresholds[i] <= this.thresholds[i - 1]) { Logger.error('Thresholds not in ascending order:', this.thresholds); return false; } }
  return true;
};

ThresholdSlider.prototype.renderGradientBar = function () {
  this.bar = document.createElement('div');
  this.bar.className = 'threshold-gradient-bar';
  this.updateGradient();
  this.container.appendChild(this.bar);
};

ThresholdSlider.prototype.updateGradient = function () {
  var stops = [
    { color: '#e53935', pct: 0 },
    { color: '#e53935', pct: this.toPercent(this.thresholds[0]) },
    { color: '#fbc02d', pct: this.toPercent(this.thresholds[0]) },
    { color: '#fbc02d', pct: this.toPercent(this.thresholds[1]) },
    { color: '#43a047', pct: this.toPercent(this.thresholds[1]) },
    { color: '#43a047', pct: this.toPercent(this.thresholds[2]) },
    { color: '#fbc02d', pct: this.toPercent(this.thresholds[2]) },
    { color: '#fbc02d', pct: this.toPercent(this.thresholds[3]) },
    { color: '#e53935', pct: this.toPercent(this.thresholds[3]) },
    { color: '#e53935', pct: 100 }
  ];
  this.bar.style.background = 'linear-gradient(90deg, ' + stops.map(function (s) { return s.color + ' ' + s.pct + '%'; }).join(', ') + ')';
};

ThresholdSlider.prototype.createHandle = function (percentage) {
  var handle = document.createElement('div');
  handle.className = 'threshold-handle';
  handle.style.left = percentage + '%';
  handle.style.touchAction = 'none';
  handle.style.cursor = 'pointer';
  return handle;
};

ThresholdSlider.prototype.createLabel = function (val, percentage, className) {
  var label = document.createElement('div');
  label.className = 'threshold-label ' + className;
  label.textContent = val;
  label.style.left = percentage + '%';
  label.style.cursor = 'pointer';
  label.style.pointerEvents = 'auto';
  return label;
};

ThresholdSlider.prototype.renderHandlesAndLabels = function () {
  var labelClasses = ['threshold-label-yellowlow', 'threshold-label-greenlow', 'threshold-label-greenhigh', 'threshold-label-yellowhigh'];
  for (var idx = 0; idx < this.thresholds.length; idx++) {
    var val = this.thresholds[idx];
    var percentage = this.toPercent(val);
    var handle = this.createHandle(percentage);
    var label = this.createLabel(val, percentage, labelClasses[idx]);
    this.handles.push(handle); this.labels.push(label);
    this.container.appendChild(handle); this.container.appendChild(label);
    this.attachDragHandlers(handle, label, idx);
  }
};

ThresholdSlider.prototype.attachDragHandlers = function (handle, label, idx) {
  var self = this;
  var dragging = false;
  function onMove(e) {
    if (!dragging) return;
    var rect = self.container.getBoundingClientRect();
    var clientX = (e.touches && e.touches.length) ? e.touches[0].clientX : e.clientX;
    var percent = (clientX - rect.left) / rect.width;
    percent = Math.max(0, Math.min(1, percent));
    var newVal = Math.round(self.config.min + percent * (self.config.max - self.config.min));
    newVal = self.clamp(newVal, idx);
    self.updateThreshold(idx, newVal);
  }
  function onUp() { if (!dragging) return; dragging = false; document.removeEventListener('mousemove', onMove); document.removeEventListener('touchmove', onMove); document.removeEventListener('mouseup', onUp); document.removeEventListener('touchend', onUp); if (self.config.inputRefs && self.config.inputRefs[idx]) self.flushDebounce(idx); }
  function onDown(e) { e.preventDefault(); dragging = true; document.addEventListener('mousemove', onMove); document.addEventListener('touchmove', onMove); document.addEventListener('mouseup', onUp); document.addEventListener('touchend', onUp); }
  handle.addEventListener('mousedown', onDown); handle.addEventListener('touchstart', onDown); label.addEventListener('mousedown', onDown); label.addEventListener('touchstart', onDown);
};

ThresholdSlider.prototype.updateThreshold = function (idx, newVal) {
  this.thresholds[idx] = newVal;
  var newPercentage = this.toPercent(newVal);
  this.handles[idx].style.left = newPercentage + '%';
  this.labels[idx].style.left = newPercentage + '%';
  this.labels[idx].textContent = newVal;
  var labelClasses = ['threshold-label-yellowlow','threshold-label-greenlow','threshold-label-greenhigh','threshold-label-yellowhigh'];
  this.labels[idx].className = 'threshold-label ' + labelClasses[idx];
  if (this.config.inputRefs && this.config.inputRefs[idx]) {
    var input = this.config.inputRefs[idx];
    input.value = newVal;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
    this.debounceBackendUpdate(idx);
  }
  this.updateGradient();
};

ThresholdSlider.prototype.debounceBackendUpdate = function (idx) {
  var key = this.config.sensorId + '_' + idx;
  var input = this.config.inputRefs ? this.config.inputRefs[idx] : null;
  Debouncer.debounce(key, function () { if (input) input.dispatchEvent(new Event('change', { bubbles: true })); }, 2000);
};

ThresholdSlider.prototype.flushDebounce = function (idx) {
  var key = this.config.sensorId + '_' + idx;
  Debouncer.flush(key);
  var input = this.config.inputRefs ? this.config.inputRefs[idx] : null;
  if (input) input.dispatchEvent(new Event('change', { bubbles: true }));
};

ThresholdSlider.prototype.attachInputSync = function () {
  var self = this;
  if (!this.config.inputRefs) return;
  this.config.inputRefs.forEach(function (input, idx) {
    if (!input) return;
    input.addEventListener('input', function () {
      var val = parseFloat(input.value);
      if (!isNaN(val)) {
        self.thresholds[idx] = self.clamp(val, idx);
        var newPercentage = self.toPercent(self.thresholds[idx]);
        self.handles[idx].style.left = newPercentage + '%';
        self.labels[idx].style.left = newPercentage + '%';
        self.labels[idx].textContent = self.thresholds[idx];
        var labelClasses = ['threshold-label-yellowlow','threshold-label-greenlow','threshold-label-greenhigh','threshold-label-yellowhigh'];
        self.labels[idx].className = 'threshold-label ' + labelClasses[idx];
        self.updateGradient();
        self.debounceBackendUpdate(idx);
      }
    });
  });
};

ThresholdSlider.prototype.renderMeasuredValue = function () {
  // Always render the threshold bar, and always create the measured value marker and label elements
  var marker = document.createElement('div'); marker.className = 'measured-value-dot';
  var label = document.createElement('div'); label.className = 'measured-value-label';
  if (typeof this.config.measuredValue === 'number' && !isNaN(this.config.measuredValue)) {
    var percentage = this.toPercent(this.config.measuredValue);
    marker.style.left = percentage + '%';
    label.textContent = this.config.measuredValue.toFixed(2);
    label.style.left = percentage + '%';
  } else {
    // No measured value yet; hide marker and label visually
    marker.style.display = 'none';
    label.style.display = 'none';
  }
  this.container.appendChild(marker);
  this.container.appendChild(label);
};

ThresholdSlider.prototype.renderAbsoluteMarkers = function () {
  var sensorKey = this.config.sensorId + '_' + (this.config.measurementIndex || 0);
  var sensorConfig = AppState._internal.initialConfig ? AppState._internal.initialConfig[sensorKey] : null;
  if (!sensorConfig) { // also try AppState.getInitialValue
    sensorConfig = AppState.getInitialValue ? { absoluteMin: AppState.getInitialValue(this.config.sensorId, this.config.measurementIndex || 0, 'absoluteMin'), absoluteMax: AppState.getInitialValue(this.config.sensorId, this.config.measurementIndex || 0, 'absoluteMax') } : null;
  }
  if (!sensorConfig) return;
  if (typeof sensorConfig.absoluteMin !== 'undefined' && sensorConfig.absoluteMin !== Infinity) this.createAbsoluteMarker('min', sensorConfig.absoluteMin, '🔽');
  if (typeof sensorConfig.absoluteMax !== 'undefined' && sensorConfig.absoluteMax !== -Infinity) this.createAbsoluteMarker('max', sensorConfig.absoluteMax, '🔼');
};

ThresholdSlider.prototype.createAbsoluteMarker = function (type, value, icon) {
  var percentage = this.toPercent(value);
  var measurementIndex = this.config.measurementIndex || 0;
  var marker = document.createElement('div'); marker.className = 'absolute-' + type + '-marker'; marker.setAttribute('data-sensor-id', this.config.sensorId); marker.setAttribute('data-measurement-index', measurementIndex); marker.style.left = percentage + '%'; marker.title = icon + value.toFixed(2); this.container.appendChild(marker);
  var label = document.createElement('div'); label.className = 'absolute-' + type + '-label'; label.setAttribute('data-sensor-id', this.config.sensorId); label.setAttribute('data-measurement-index', measurementIndex); label.textContent = icon + value.toFixed(2); label.style.left = percentage + '%'; this.container.appendChild(label);
};

ThresholdSlider.prototype.toPercent = function (value) {
  if (value < this.config.min) { Logger.warn('Value ' + value + ' below min ' + this.config.min + ', clamping to 0%'); return 0; }
  if (value > this.config.max) { Logger.warn('Value ' + value + ' above max ' + this.config.max + ', clamping to 100%'); return 100; }
  return ((value - this.config.min) / (this.config.max - this.config.min)) * 100;
};

ThresholdSlider.prototype.clamp = function (val, idx) {
  if (idx > 0) val = Math.max(val, this.thresholds[idx - 1] + 1);
  if (idx < this.thresholds.length - 1) val = Math.min(val, this.thresholds[idx + 1] - 1);
  return Math.max(this.config.min, Math.min(this.config.max, val));
};

// ------------------------------
// THRESHOLD SLIDER INITIALIZER
// ------------------------------
var ThresholdSliderInitializer = (function () {
  function initialize(sensorConfigData) {
    if (!sensorConfigData || !sensorConfigData.sensors) { Logger.warn('No sensors found in config'); return; }
    var sensors = sensorConfigData.sensors;
    for (var sid in sensors) if (sensors.hasOwnProperty(sid)) {
      var sensor = sensors[sid];
      if (!sensor.measurements) { Logger.warn('Sensor has no measurements:', sensor.id); continue; }
      var measurements = (Object.prototype.toString.call(sensor.measurements) === '[object Array]') ? sensor.measurements : (function (m) { var arr = []; for (var k in m) if (m.hasOwnProperty(k)) arr.push(m[k]); return arr; })(sensor.measurements);
      for (var i = 0; i < measurements.length; i++) initializeSingleSlider(sensor, measurements[i], i);
    }
  }

  function initializeSingleSlider(sensor, meas, index) {
    var range = calculateRange(sensor, meas, index);
    var min = range.min, max = range.max;
    var thresholds = [meas.thresholds.yellowLow, meas.thresholds.greenLow, meas.thresholds.greenHigh, meas.thresholds.yellowHigh];
    var inputNames = ['yellowLow', 'greenLow', 'greenHigh', 'yellowHigh'];
    var inputRefs = inputNames.map(function (th) { return document.querySelector("input[name='" + sensor.id + '_' + index + '_' + th + "']"); });
    var container = document.getElementById('threshold_' + sensor.id + '_' + index);
    if (!container) { Logger.warn('No container found for slider: threshold_' + sensor.id + '_' + index); return; }
    var sliderContainer = container.parentElement;
    var measuredValue = null;
    // Try to get measured value if available, otherwise leave as null
    if (sliderContainer && sliderContainer.classList && sliderContainer.classList.contains('threshold-slider-container')) {
      var lastValueAttr = sliderContainer.getAttribute('data-last-value');
      if (lastValueAttr !== null && !isNaN(parseFloat(lastValueAttr))) measuredValue = parseFloat(lastValueAttr);
    }
    // Always render the threshold slider, even if measuredValue is null
    container.setAttribute('data-min', min);
    container.setAttribute('data-max', max);
    Logger.debug('Initializing slider ' + sensor.id + '_' + index + ': range ' + min + '-' + max + ', thresholds [' + thresholds.join(',') + ']');
    new ThresholdSlider(container, { min: min, max: max, thresholds: thresholds, measuredValue: measuredValue, inputRefs: inputRefs, sensorId: sensor.id, measurementIndex: index });
  }

  function calculateRange(sensor, meas, index) {
    var name = (meas.name || '').toLowerCase();
    var id = (sensor.id + '_' + index).toLowerCase();
    var unit = (meas.unit || '').toLowerCase();
    var thresholds = [meas.thresholds.yellowLow, meas.thresholds.greenLow, meas.thresholds.greenHigh, meas.thresholds.yellowHigh];
    var minThreshold = Math.min.apply(null, thresholds); var maxThreshold = Math.max.apply(null, thresholds); var thresholdRange = maxThreshold - minThreshold;
    var min, max;
    var isAnalog = (sensor.id && sensor.id.toLowerCase().indexOf('analog') === 0);

    // For analog sensors, always use 0..100 scale (relative values, not raw values)
    if (isAnalog) {
      min = 0; max = 100;
      Logger.debug(sensor.id + '_' + index + ': Analog sensor, fixed scale 0-100%');
    } else if (name.indexOf('hum') !== -1 || name.indexOf('%') !== -1 || id.indexOf('hum') !== -1 || unit.indexOf('%') !== -1) {
      min = 0; max = 100;
      Logger.debug(sensor.id + '_' + index + ': Percentage scale 0-100%');
    } else {
      var buffer = thresholdRange * 0.2;
      min = Math.floor(minThreshold - buffer);
      max = Math.ceil(maxThreshold + buffer);
      if (name.indexOf('temp') !== -1 || name.indexOf('°c') !== -1 || id.indexOf('temp') !== -1) {
        min = Math.max(min, -40); max = Math.max(max, 50);
        Logger.debug(sensor.id + '_' + index + ': Temperature scale ' + min + '-' + max + '°C');
      } else if (name.indexOf('co2') !== -1 || name.indexOf('ppm') !== -1 || id.indexOf('co2') !== -1) {
        min = Math.max(min, 0);
        Logger.debug(sensor.id + '_' + index + ': CO2 scale ' + min + '-' + max + ' ppm');
      } else if (name.indexOf('pm') !== -1 || name.indexOf('partikel') !== -1 || id.indexOf('pm') !== -1) {
        min = Math.max(min, 0);
        Logger.debug(sensor.id + '_' + index + ': PM scale ' + min + '-' + max);
      } else {
        Logger.debug(sensor.id + '_' + index + ': Generic scale ' + min + '-' + max);
      }

      // For non-analog sensors, allow minmax override if specified
      if (meas.minmax && typeof meas.minmax.min !== 'undefined') {
        min = meas.minmax.min;
        Logger.debug(sensor.id + '_' + index + ': Using config minmax.min: ' + min);
      }
      if (meas.minmax && typeof meas.minmax.max !== 'undefined') {
        max = meas.minmax.max;
        Logger.debug(sensor.id + '_' + index + ': Using config minmax.max: ' + max);
      }
    }
    // Note: For analog sensors, we deliberately ignore meas.minmax here.
    // The minmax values are the RAW sensor values used for percentage calculation,
    // but the threshold slider always displays the RELATIVE 0-100% scale.
  // Do NOT extend slider range to absolute min/max; only use calculated min/max for threshold bar
    if (minThreshold < min || maxThreshold > max) { Logger.warn(sensor.id + '_' + index + ': Thresholds (' + minThreshold + '-' + maxThreshold + ') don\'t fit in scale (' + min + '-' + max + '). Adjusting.'); min = Math.min(min, minThreshold - 5); max = Math.max(max, maxThreshold + 5); }
    return { min: min, max: max };
  }

  return { initialize: initialize, initializeSingleSlider: initializeSingleSlider, calculateRange: calculateRange };
}());

// ------------------------------
// SENSOR UPDATER (polling)
// ------------------------------
var SensorUpdater = (function () {
  var updateInterval = 5000, timerId = null;
  function start() { update(); timerId = setInterval(update, updateInterval); Logger.info('Sensor updater started'); }
  function stop() { if (timerId) { clearInterval(timerId); timerId = null; } }

  function update() {
    if (typeof fetch === 'function') {
      fetch('/getLatestValues', { credentials: 'include' }).then(function (r) { return parseJsonResponse(r); }).then(function (data) { if (data && data.sensors) handleLatestValues(data.sensors); }).catch(function (err) { Logger.debug('Sensor update failed:', err); });
    } else {
      // XHR fallback
      var xhr = new XMLHttpRequest(); xhr.open('GET', '/getLatestValues', true); xhr.withCredentials = true; xhr.onload = function () { if (xhr.status >= 200 && xhr.status < 300) try { var d = JSON.parse(xhr.responseText); if (d && d.sensors) handleLatestValues(d.sensors); } catch (e) { Logger.debug('Failed to parse /getLatestValues', e); } }; xhr.onerror = function () { Logger.debug('Sensor update failed (XHR)'); }; xhr.send(null);
    }
  }

  function handleLatestValues(sensors) {
    for (var sensorKey in sensors) if (sensors.hasOwnProperty(sensorKey)) {
      var sensorData = sensors[sensorKey];
      updateMeasuredValue(sensorKey, sensorData);
      updateRawValue(sensorKey, sensorData);
      updateAbsoluteMinMax(sensorKey, sensorData);
      updateAbsoluteRawMinMax(sensorKey, sensorData);
      updateCalibrationMode(sensorKey, sensorData);
      // If server emitted minmax (calculation limits), update them in the UI
      try {
        if (sensorData.minmax && typeof sensorData.minmax.min !== 'undefined' && typeof sensorData.minmax.max !== 'undefined') {
          var m = sensorKey.match(/^(.*)_(\d+)$/);
          if (m) {
            var sid = m[1], midx = parseInt(m[2], 10);
            // Call through DisplayUpdater to avoid a global reference error
            DisplayUpdater.updateAnalogCalculationMinMax(sid, midx, sensorData.minmax.min, sensorData.minmax.max);
          }
        }
      } catch (e) { Logger.debug('No minmax in sensorData or failed to update minmax', e); }
    }
  }

  function updateMeasuredValue(sensorKey, sensorData) {
    var input = DOMUtils.querySelector("input.readonly-value[data-sensor='" + sensorKey + "']");
    if (!input || typeof sensorData.value === 'undefined') return;
    var newValue = (parseFloat(sensorData.value)).toFixed(1);
    if (input.value === newValue) return;
    input.value = newValue;
    var card = input.closest ? input.closest('.measurement-card') : (function(){ var el = input; while (el && el.className && el.className.indexOf('measurement-card') === -1) el = el.parentNode; return el; })();
    if (card) DOMUtils.flashElement(card);
    updateThresholdMarker(sensorKey, sensorData.value);
  }

  function updateThresholdMarker(sensorKey, value) {
    var m = sensorKey.match(/^(.*)_(\d+)$/);
    if (!m) return; var sensorId = m[1], measurementIndex = m[2]; var container = document.getElementById('threshold_' + sensorId + '_' + measurementIndex); if (!container) return; var slider = container.querySelector('.threshold-slider'); if (slider && typeof slider.updateBar === 'function') { slider.lastValue = parseFloat(value); slider.updateBar(); }
  var sliderContainer = container.parentElement;
  if (!sliderContainer || !sliderContainer.classList || !sliderContainer.classList.contains('threshold-slider-container')) return;
  var valueDot = sliderContainer.querySelector('.measured-value-dot');
  var valueLabel = sliderContainer.querySelector('.measured-value-label');
  // Only update marker if it exists (i.e., after first measurement)
  if (!valueDot || !valueLabel) return;
  // Always use calculation min/max for marker position
  var min = parseFloat(container.getAttribute('data-min'));
  var max = parseFloat(container.getAttribute('data-max'));
  if (!isFinite(min) || !isFinite(max) || max <= min) { min = 0; max = 100; }
  var percent = ((parseFloat(value) - min) / (max - min)) * 100;
  percent = Math.max(0, Math.min(100, percent));
  valueDot.style.left = percent + '%';
  valueLabel.style.left = percent + '%';
  valueLabel.textContent = (parseFloat(value)).toFixed(2);
  }

  function updateRawValue(sensorKey, sensorData) {
    if (typeof sensorData.raw === 'undefined') return; var m = sensorKey.match(/^(.*)_(\d+)$/); if (!m) return; var sensorId = m[1], measurementIndex = m[2]; var minInput = DOMUtils.querySelector('.minmax-section input.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]'); if (!minInput) return; var minmaxRow = minInput.closest ? minInput.closest('.minmax-section') : (function(){ var el = minInput; while (el && el.className && el.className.indexOf('minmax-section') === -1) el = el.parentNode; return el; })(); if (!minmaxRow) return; var rawInput = minmaxRow.querySelector('input.readonly-value'); if (rawInput && rawInput.value !== String(sensorData.raw)) rawInput.value = sensorData.raw;
  }

  function updateAbsoluteMinMax(sensorKey, sensorData) { var m = sensorKey.match(/^(.*)_(\d+)$/); if (!m) return; var sensorId = m[1], measurementIndex = parseInt(m[2], 10); if (typeof sensorData.absoluteMin === 'undefined' && typeof sensorData.absoluteMax === 'undefined') return; DisplayUpdater.updateAbsoluteMinMax(sensorId, measurementIndex, sensorData.absoluteMin, sensorData.absoluteMax); }
  function updateAbsoluteRawMinMax(sensorKey, sensorData) { var m = sensorKey.match(/^(.*)_(\d+)$/); if (!m) return; var sensorId = m[1], measurementIndex = parseInt(m[2], 10); if (typeof sensorData.absoluteRawMin === 'undefined' && typeof sensorData.absoluteRawMax === 'undefined') return; DisplayUpdater.updateAbsoluteRawMinMax(sensorId, measurementIndex, sensorData.absoluteRawMin, sensorData.absoluteRawMax); }
  function updateCalibrationMode(sensorKey, sensorData) { if (typeof sensorData.calibrationMode === 'undefined') return; var m = sensorKey.match(/^(.*)_(\d+)$/); if (!m) return; var sensorId = m[1], measurementIndex = parseInt(m[2], 10); DisplayUpdater.updateCalibrationMode(sensorId, measurementIndex, sensorData.calibrationMode); }

  return { start: start, stop: stop };
}());

// ------------------------------
// EVENT HANDLERS (delegation-based)
// ------------------------------
var EventHandlers = (function () {
  function init() { initFlowerStatusSelect(); initEventDelegation(); initResetButtons(); }

  function initFlowerStatusSelect() { var sel = document.getElementById('flower-status-sensor'); if (!sel) return; sel.addEventListener('change', function () { SensorConfigAPI.updateFlowerStatus(this.value); }); }

  function initEventDelegation() {
    document.addEventListener('input', function (e) { handleInput(e); });
    document.addEventListener('change', function (e) { handleChange(e); });
    document.addEventListener('click', function (e) { handleClick(e); });
  }

  function handleInput(e) {
    var t = e.target;
    if (DOMUtils.elementMatches(t, '.measurement-interval-input')) handleIntervalInput(t);
    else if (DOMUtils.elementMatches(t, '.analog-min-input') || DOMUtils.elementMatches(t, '.analog-max-input')) handleAnalogMinMaxInput(t);
    else if (DOMUtils.elementMatches(t, '.threshold-input')) handleThresholdInput(t);
    else if (DOMUtils.elementMatches(t, '.measurement-name')) handleMeasurementNameInput(t);
  }

  function handleChange(e) {
    var t = e.target;
    if (DOMUtils.elementMatches(t, '.analog-inverted-checkbox')) handleInvertedChange(t);
    else if (DOMUtils.elementMatches(t, '.analog-autocal-checkbox')) handleAutocalChange(t);
  }

  function handleClick(e) {
    var t = e.target;
    if (DOMUtils.elementMatches(t, '.reset-minmax-button')) handleResetMinMax(t);
    else if (DOMUtils.elementMatches(t, '.reset-raw-minmax-button')) handleResetRawMinMax(t);
    else if (DOMUtils.elementMatches(t, '.measure-button')) handleMeasureButton(t);
  }

  function handleIntervalInput(input) { var sensorId = input.dataset.sensorId; var interval = parseInt(input.value, 10); Debouncer.debounce('interval_' + sensorId, function () { if (interval >= 10 && interval <= 3600) SensorConfigAPI.updateMeasurementInterval(sensorId, interval); }, 1000); }

  function handleAnalogMinMaxInput(input) { var sensorId = input.dataset.sensorId; var measurementIndex = parseInt(input.dataset.measurementIndex, 10); var key = 'analog_minmax_' + sensorId + '_' + measurementIndex; Debouncer.debounce(key, function () { var minInput = document.querySelector('.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]'); var maxInput = document.querySelector('.analog-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]'); if (minInput && maxInput) { var minValue = parseFloat(minInput.value); var maxValue = parseFloat(maxInput.value); if (!isNaN(minValue) && !isNaN(maxValue) && minValue < maxValue) SensorConfigAPI.updateAnalogMinMax(sensorId, measurementIndex, minValue, maxValue); else Logger.warn('Invalid analog min/max values: min=' + minValue + ', max=' + maxValue); } }, 1000); }

  function handleThresholdInput(input) { var name = input.name; var m = name.match(/^(.+)_(\d+)_(.+)$/); if (!m) return; var sensorId = m[1], measurementIndex = parseInt(m[2], 10); Debouncer.debounce('threshold_' + sensorId + '_' + measurementIndex, function () { var yellowLow = document.querySelector('input[name="' + sensorId + '_' + measurementIndex + '_yellowLow"]'); var greenLow = document.querySelector('input[name="' + sensorId + '_' + measurementIndex + '_greenLow"]'); var greenHigh = document.querySelector('input[name="' + sensorId + '_' + measurementIndex + '_greenHigh"]'); var yellowHigh = document.querySelector('input[name="' + sensorId + '_' + measurementIndex + '_yellowHigh"]'); if (yellowLow && greenLow && greenHigh && yellowHigh) { var thresholds = [ parseFloat(yellowLow.value), parseFloat(greenLow.value), parseFloat(greenHigh.value), parseFloat(yellowHigh.value) ]; if (thresholds.every(function (t) { return !isNaN(t); })) SensorConfigAPI.updateThresholds(sensorId, measurementIndex, thresholds); } }, 1000); }

  function handleMeasurementNameInput(input) { var name = input.name; var m = name.match(/^name_(.+)_(\d+)$/); if (!m) return; var sensorId = m[1]; var idx = parseInt(m[2], 10); Debouncer.debounce('name_' + sensorId + '_' + idx, function () { if (input.value.trim()) SensorConfigAPI.updateMeasurementName(sensorId, idx, input.value.trim()); }, 1000); }

  function handleInvertedChange(checkbox) { var sensorId = checkbox.dataset.sensorId; var measurementIndex = parseInt(checkbox.dataset.measurementIndex, 10); var inverted = checkbox.checked; SensorConfigAPI.updateAnalogInverted(sensorId, measurementIndex, inverted); }

  function handleAutocalChange(checkbox) {
    var sensorId = checkbox.dataset.sensorId;
    var measurementIndex = parseInt(checkbox.dataset.measurementIndex, 10);
    var enabled = checkbox.checked;
    // Optimistically update UI
    DisplayUpdater.updateCalibrationMode(sensorId, measurementIndex, enabled);
    // Ensure immediate visual feedback: toggle min/max input disabled state and readonly class
    try {
      var minInput = document.querySelector('.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      var maxInput = document.querySelector('.analog-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      if (minInput) {
        minInput.disabled = enabled;
        if (enabled) minInput.classList.add('readonly-value'); else minInput.classList.remove('readonly-value');
      }
      if (maxInput) {
        maxInput.disabled = enabled;
        if (enabled) maxInput.classList.add('readonly-value'); else maxInput.classList.remove('readonly-value');
      }
    } catch (e) { Logger.debug('Immediate UI toggle failed', e); }
    // If enabling autocal, first persist the current manual min/max (if present)
    // so the autocal starts from the user's configured values.
    if (enabled) {
      var minInput = document.querySelector('.analog-min-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      var maxInput = document.querySelector('.analog-max-input[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
      var ensureMinMax = Promise.resolve();
      if (minInput && maxInput) {
        var minValue = parseFloat(minInput.value);
        var maxValue = parseFloat(maxInput.value);
        if (!isNaN(minValue) && !isNaN(maxValue) && minValue < maxValue) {
          ensureMinMax = SensorConfigAPI.updateAnalogMinMax(sensorId, measurementIndex, minValue, maxValue);
        }
      }
      ensureMinMax.then(function () {
        return SensorConfigAPI.updateAnalogAutocal(sensorId, measurementIndex, enabled);
      }).catch(function (err) {
        // Revert UI state on error
        Logger.error('Failed to enable autocal (or persist min/max), reverting UI', err);
        checkbox.checked = !enabled;
        DisplayUpdater.updateCalibrationMode(sensorId, measurementIndex, !enabled);
        if (typeof showErrorMessage === 'function') showErrorMessage('Fehler beim Aktivieren der Autokalibrierung');
      });
    } else {
      // Disabling autocal: simple call
      SensorConfigAPI.updateAnalogAutocal(sensorId, measurementIndex, enabled).catch(function (err) {
        Logger.error('Failed to disable autocal on server, reverting UI', err);
        checkbox.checked = !enabled;
        DisplayUpdater.updateCalibrationMode(sensorId, measurementIndex, !enabled);
        if (typeof showErrorMessage === 'function') showErrorMessage('Fehler beim Deaktivieren der Autokalibrierung');
      });
    }
  }

  function handleResetMinMax(button) { var sensorId = button.dataset.sensorId; var measurementIndex = parseInt(button.dataset.measurementIndex, 10); SensorConfigAPI.resetAbsoluteMinMax(sensorId, measurementIndex); }
  function handleResetRawMinMax(button) { var sensorId = button.dataset.sensorId; var measurementIndex = parseInt(button.dataset.measurementIndex, 10); SensorConfigAPI.resetAbsoluteRawMinMax(sensorId, measurementIndex); }
  function handleMeasureButton(button) { var sensorId = button.dataset.sensor; var measurementIndexStr = button.dataset.measurementIndex; var measurementIndex = (typeof measurementIndexStr !== 'undefined' && measurementIndexStr !== '') ? parseInt(measurementIndexStr, 10) : undefined; SensorConfigAPI.triggerMeasurement(sensorId, measurementIndex); }

  function initResetButtons() {
    DOMUtils.querySelectorAll('.reset-minmax-button').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var sid = btn.getAttribute('data-sensor-id');
        var idx = parseInt(btn.getAttribute('data-measurement-index'), 10);
        SensorConfigAPI.resetAbsoluteMinMax(sid, idx);
      });
    });
    DOMUtils.querySelectorAll('.reset-raw-minmax-button').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var sid = btn.getAttribute('data-sensor-id');
        var idx = parseInt(btn.getAttribute('data-measurement-index'), 10);
        SensorConfigAPI.resetAbsoluteRawMinMax(sid, idx);
      });
    });
    // NOTE: reset-autocal button removed from UI; no handler attached
  }

  return { init: init };
}());

// ------------------------------
// CONFIG LOADER
// ------------------------------
var ConfigLoader = (function () {
  function load() {
    return new Promise(function (resolve, reject) {
      if (typeof fetch === 'function') {
        fetch('/admin/getSensorConfig', { credentials: 'include' }).then(function (r) { return parseJsonResponse(r); }).then(function (data) { if (data && data.sensors) { storeInitialValues(data.sensors); updateThresholdInputsFromConfig(data.sensors); ThresholdSliderInitializer.initialize(data); initializeAutocalCheckboxes(data.sensors); Logger.debug('Config loaded successfully'); resolve(data); } else { Logger.warn('No sensors in config'); resolve(data); } }).catch(function (err) { Logger.error('Failed to load config:', err); reject(err); });
      } else {
        var xhr = new XMLHttpRequest(); xhr.open('GET', '/admin/getSensorConfig', true); xhr.withCredentials = true; xhr.onload = function () { if (xhr.status >= 200 && xhr.status < 300) try { var data = JSON.parse(xhr.responseText); if (data && data.sensors) { storeInitialValues(data.sensors); updateThresholdInputsFromConfig(data.sensors); ThresholdSliderInitializer.initialize(data); initializeAutocalCheckboxes(data.sensors); Logger.debug('Config loaded successfully'); resolve(data); } else { Logger.warn('No sensors in config'); resolve(data); } } catch (e) { Logger.error('Failed to parse getSensorConfig', e); reject(e); } else reject(new Error('HTTP ' + xhr.status)); }; xhr.onerror = function () { Logger.error('Network error while loading config'); reject(new Error('Network error')); }; xhr.send(null);
      }
    });
  }

  function storeInitialValues(sensors) {
    var initialValues = {};
    for (var sensorId in sensors) if (sensors.hasOwnProperty(sensorId)) {
      var sensor = sensors[sensorId];
      if (!sensor.measurements) continue;
      var measKeys = (Object.prototype.toString.call(sensor.measurements) === '[object Array]') ? sensor.measurements : (function (m) { var arr = []; for (var kk in m) if (m.hasOwnProperty(kk)) arr.push(m[kk]); return arr; })(sensor.measurements);
      for (var idx = 0; idx < measKeys.length; idx++) {
        var m = measKeys[idx]; var key = sensorId + '_' + idx;
        initialValues[key] = { name: m.name, enabled: m.enabled, thresholds: { yellowLow: m.thresholds.yellowLow, greenLow: m.thresholds.greenLow, greenHigh: m.thresholds.greenHigh, yellowHigh: m.thresholds.yellowHigh }, min: m.minmax ? m.minmax.min : undefined, max: m.minmax ? m.minmax.max : undefined, inverted: (typeof m.inverted !== 'undefined') ? m.inverted : false, interval: sensor.interval ? sensor.interval / 1000 : undefined, absoluteMin: (typeof m.absoluteMin !== 'undefined') ? m.absoluteMin : undefined, absoluteMax: (typeof m.absoluteMax !== 'undefined') ? m.absoluteMax : undefined, absoluteRawMin: (typeof m.absoluteRawMin !== 'undefined') ? m.absoluteRawMin : undefined, absoluteRawMax: (typeof m.absoluteRawMax !== 'undefined') ? m.absoluteRawMax : undefined, calibrationMode: (typeof m.calibrationMode !== 'undefined') ? m.calibrationMode : false };
      }
    }
    AppState.setInitialConfig(initialValues);
  }

  function updateThresholdInputsFromConfig(sensors) {
    // CRITICAL: Update threshold input values from loaded config
    // This ensures the inputs show the correct values from Preferences,
    // not the server-side rendered values which might be outdated
    Logger.debug('Updating threshold inputs from config');
    for (var sensorId in sensors) if (sensors.hasOwnProperty(sensorId)) {
      var sensor = sensors[sensorId];
      if (!sensor.measurements) continue;
      var measKeys = (Object.prototype.toString.call(sensor.measurements) === '[object Array]') ? sensor.measurements : (function (m) { var arr = []; for (var kk in m) if (m.hasOwnProperty(kk)) arr.push(m[kk]); return arr; })(sensor.measurements);
      for (var idx = 0; idx < measKeys.length; idx++) {
        var m = measKeys[idx];

        // Update threshold input fields
        if (m.thresholds) {
          var inputNames = ['yellowLow', 'greenLow', 'greenHigh', 'yellowHigh'];
          var thresholdValues = [m.thresholds.yellowLow, m.thresholds.greenLow, m.thresholds.greenHigh, m.thresholds.yellowHigh];

          for (var i = 0; i < inputNames.length; i++) {
            var inputName = sensorId + '_' + idx + '_' + inputNames[i];
            var input = document.querySelector('input[name="' + inputName + '"]');
            if (input && typeof thresholdValues[i] !== 'undefined') {
              input.value = thresholdValues[i];
              Logger.debug('Updated ' + inputName + ' to ' + thresholdValues[i]);
            }
          }
        }

        // CRITICAL: Also update absolute min/max inputs and markers
        // These are rendered server-side but might be outdated
        if (typeof m.absoluteMin !== 'undefined' || typeof m.absoluteMax !== 'undefined') {
          DisplayUpdater.updateAbsoluteMinMax(sensorId, idx, m.absoluteMin, m.absoluteMax);
          Logger.debug('Updated absolute min/max for ' + sensorId + '_' + idx);
        }

        // Update absolute raw min/max if present
        if (typeof m.absoluteRawMin !== 'undefined' || typeof m.absoluteRawMax !== 'undefined') {
          DisplayUpdater.updateAbsoluteRawMinMax(sensorId, idx, m.absoluteRawMin, m.absoluteRawMax);
          Logger.debug('Updated absolute raw min/max for ' + sensorId + '_' + idx);
        }
      }
    }
  }

  function initializeAutocalCheckboxes(sensors) {
    for (var sensorId in sensors) if (sensors.hasOwnProperty(sensorId)) {
      var sensor = sensors[sensorId]; if (!sensor.measurements) continue; for (var k in sensor.measurements) if (sensor.measurements.hasOwnProperty(k)) { var m = sensor.measurements[k]; var measurementIndex = parseInt(k, 10); if (typeof m.calibrationMode !== 'undefined') { var shouldBeChecked = !!m.calibrationMode; var selector = '.analog-autocal-checkbox[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]'; var checkbox = DOMUtils.querySelector(selector); if (checkbox && checkbox.checked !== shouldBeChecked) checkbox.checked = shouldBeChecked; // Ensure UI controls are enabled/disabled accordingly
          // Also update UI controls (disable/enable min/max inputs) to reflect calibrationMode immediately
          try {
            if (typeof DisplayUpdater !== 'undefined') DisplayUpdater.updateCalibrationMode(sensorId, measurementIndex, shouldBeChecked);
          } catch (e) { Logger.debug('Failed to update calibration mode UI during init', e); }
          // Ensure autocal-duration-section visibility matches calibrationMode on initial load
          try {
            var selInit = DOMUtils.querySelector('.analog-autocal-duration[data-sensor-id="' + sensorId + '"][data-measurement-index="' + measurementIndex + '"]');
            if (selInit) {
              var cont = selInit;
              var depth2 = 0;
              while (cont && depth2 < 6 && !(cont.classList && cont.classList.contains('autocal-duration-section'))) { cont = cont.parentElement; depth2++; }
              if (cont && cont.style) cont.style.display = shouldBeChecked ? '' : 'none';
              try { selInit.disabled = !shouldBeChecked; } catch (e) {}
            }
          } catch (e) { Logger.debug('Failed to set autocal-duration visibility during init', e); }
        }
      }
    }
  }

  return { load: load, storeInitialValues: storeInitialValues, initializeAutocalCheckboxes: initializeAutocalCheckboxes };
}());

// ------------------------------
// SENSORS.JS COMPATIBILITY (adds .value and status indicator placeholders)
// ------------------------------
var SensorsJSCompat = (function () {
  function extend() { addDataAttributes(); setupMutationObserver(); }
  function addDataAttributes() { DOMUtils.querySelectorAll('.measurement-card').forEach(function (card) { var readonlyInput = card.querySelector('input.readonly-value'); if (!readonlyInput || !readonlyInput.dataset || !readonlyInput.dataset.sensor) return; card.classList.add('sensor-box'); card.setAttribute('data-sensor', readonlyInput.dataset.sensor); var valueDisplay = document.createElement('div'); valueDisplay.className = 'value'; valueDisplay.style.display = 'none'; card.appendChild(valueDisplay); var statusIndicator = document.createElement('div'); statusIndicator.className = 'status-indicator'; statusIndicator.style.display = 'none'; card.appendChild(statusIndicator); }); }
  function setupMutationObserver() { var observer = new MutationObserver(function (mutations) { for (var i = 0; i < mutations.length; i++) { var mutation = mutations[i]; if (mutation.type === 'childList' || mutation.type === 'characterData') { var target = mutation.target; if (target && target.classList && target.classList.contains('value')) syncValueToInput(target); } } }); DOMUtils.querySelectorAll('.value').forEach(function (valueElement) { observer.observe(valueElement, { childList: true, characterData: true, subtree: true }); }); }
  function syncValueToInput(valueElement) { var sensorBox = valueElement.closest ? valueElement.closest('.sensor-box') : (function () { var el = valueElement; while (el && (!el.className || el.className.indexOf('sensor-box') === -1)) el = el.parentNode; return el; })(); if (!sensorBox) return; var readonlyInput = sensorBox.querySelector('input.readonly-value'); if (!readonlyInput) return; var valueText = valueElement.textContent || ''; var m = valueText.match(/(\d+\.?\d*)/); if (!m) return; var newValue = parseFloat(m[1]).toFixed(1); if (readonlyInput.value !== newValue) { readonlyInput.value = newValue; DOMUtils.flashElement(sensorBox); } }
  return { extend: extend };
}());

// ------------------------------
// INITIALIZATION
// ------------------------------
(function () {
  function onDomReady(cb) { if (document.readyState === 'complete' || document.readyState === 'interactive') setTimeout(cb, 0); else document.addEventListener('DOMContentLoaded', cb); }
  onDomReady(function () {
    Logger.debug('Admin sensors initializing...');
    ConfigLoader.load().then(function () {
      SensorUpdater.start();
      EventHandlers.init();
      SensorsJSCompat.extend();
      Logger.info('Admin sensors initialized');
    }).catch(function (e) { Logger.error('Initialization failed:', e); });
  });
}());

// ------------------------------
// EXPORT (for testing)
// ------------------------------
if (typeof window !== 'undefined') {
  window.AdminSensors = { Logger: Logger, Debouncer: Debouncer, API: API, AppState: AppState, SensorUpdater: SensorUpdater, DisplayUpdater: DisplayUpdater, SensorConfigAPI: SensorConfigAPI, ThresholdSlider: ThresholdSlider, ThresholdSliderInitializer: ThresholdSliderInitializer, EventHandlers: EventHandlers, ConfigLoader: ConfigLoader, SensorsJSCompat: SensorsJSCompat };
}
