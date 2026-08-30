/**
 * @file sensors.js
 * @brief JavaScript for automatic sensor value and countdown updates (New UI)
 */

let updateFailureCount = 0;
const MAX_UPDATE_FAILURES = 3;
let latestSensorData = {};

/** Sperre nach einer selbst ausgelösten Messung, etwas über der 2-s-Drossel
 *  des Servers (SensorHandler::MEASURE_MIN_INTERVAL_MS) - der zweite Klick
 *  soll gar nicht erst als 429 zurückkommen. */
const MEASURE_COOLDOWN_MS = 3000;
/** Notbremse: kommt nach so langer Zeit kein neuer Messwert, wird der
 *  Wartezustand aufgelöst, damit ein Blatt nicht endlos weiteratmet. */
const MEASURING_TIMEOUT_MS = 25000;
/** Muss zur Dauer von leaf-wiggle in css/start.css passen. */
const WIGGLE_MS = 700;
/** Nach einer ausgelösten Messung wird enger abgefragt, mit wachsenden
 *  Abständen. Der ESP beantwortet nur eine Verbindung gleichzeitig, deshalb
 *  bewusst wenige Anfragen - und die Kette bricht ab, sobald der Wert da ist. */
const FAST_POLL_DELAYS_MS = [1500, 2000, 2500, 3000, 4000, 5000];

const measureCooldownUntil = {}; // Sensor-ID -> Zeitpunkt, ab dem wieder gemessen werden darf
let pollInFlight = false;
let fastPollTimer = null;
let fastPollStep = 0;

window.addEventListener('DOMContentLoaded', () => {
  const cloud = document.querySelector('.cloud');
  const box = document.querySelector('.box');
  const earth = document.querySelector('.earth');

  // Start sensor value updates
  //
  // Im verborgenen Tab wird NICHT abgefragt. Der Sensor beantwortet jeweils
  // nur eine Verbindung und baut für /getLatestValues das komplette JSON aller
  // Sensoren auf - ein vergessener Hintergrundtab hätte ihn sonst dauerhaft
  // alle 10 s beschäftigt, ohne dass jemand hinsieht. Beim Zurückkehren wird
  // sofort einmal aktualisiert, damit nichts Veraltetes stehen bleibt.
  updateSensorValues();
  setInterval(() => {
    if (document.visibilityState !== 'hidden') updateSensorValues();
  }, 10000);

  // Update countdown timers more frequently (rein lokal, keine Anfragen)
  setInterval(() => {
    if (document.visibilityState !== 'hidden') updateCountdowns();
  }, 1000);

  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') {
      updateSensorValues();
      updateCountdowns();
    }
  });

  // Klick auf ein Sensorblatt löst eine Sofortmessung aus
  bindSensorTriggers();

  // Animate cloud (gentle floating effect)
  if (cloud) {
    let start = performance.now();

    function animate(t) {
      const elapsed = (t - start) / 1000; // seconds

      // Horizontal drift: slow sine wave
      const x = Math.sin(elapsed * 0.3) * 30; // px

      // Vertical bob: subtle
      const y = Math.sin(elapsed * 0.9) * 8; // px

      cloud.style.transform = `translate(${x}px, ${y}px)`;
      requestAnimationFrame(animate);
    }

    requestAnimationFrame(animate);
  }

  // Optional: Auto-assign left/right classes if not set
  // (useful if your ESP/backend doesn't set these classes)
  const sensors = document.querySelectorAll('.sensor');
  sensors.forEach((sensor, index) => {
    if (!sensor.classList.contains('left') && !sensor.classList.contains('right')) {
      sensor.classList.add(index % 2 === 0 ? 'left' : 'right');
    }
  });

  // Navigation links to change face image and background gradient
  const faceImage = document.querySelector('.face');
  const navLinks = document.querySelectorAll('.nav-item');

  // Hilfsfunktionen, um den Inhalt der Fußzeile ohne Neuladen umzuschalten
  function showFooterAdminMenu() {
  const adminMenu = document.getElementById('footer-admin-menu');
  const statsTable = document.getElementById('footer-stats-table');
  const navAdmin = document.getElementById('nav-admin');
  if (!adminMenu || !statsTable) return;

  // Remove any active marker from admin submenu entries so none is
  // pre-selected when the menu is opened.
  const activeSubmenuItems = adminMenu.querySelectorAll('.nav-item.active');
  activeSubmenuItems.forEach(el => el.classList.remove('active'));

  adminMenu.style.display = 'block';
  statsTable.style.display = 'none';
  if (navAdmin) navAdmin.classList.add('active');
  }

  function hideFooterAdminMenu() {
  const adminMenu = document.getElementById('footer-admin-menu');
  const statsTable = document.getElementById('footer-stats-table');
  const navAdmin = document.getElementById('nav-admin');
  if (!adminMenu || !statsTable) return;

  adminMenu.style.display = 'none';
  statsTable.style.display = '';
  if (navAdmin) navAdmin.classList.remove('active');

  // Entferne aktive Markierung aus Untermenü
  const activeSub = adminMenu.querySelector('.nav-item.active');
  if (activeSub) activeSub.classList.remove('active');
  }

  function toggleFooterAdminMenu() {
    const adminMenu = document.getElementById('footer-admin-menu');
    if (!adminMenu) return;
    const visible = window.getComputedStyle(adminMenu).display !== 'none';
    if (visible) hideFooterAdminMenu(); else showFooterAdminMenu();
  }

  if (faceImage && navLinks.length > 0) {
    navLinks.forEach(link => {
      link.addEventListener('click', (e) => {
        const href = link.getAttribute('href');
        // Nur Standardaktion verhindern für Hash- oder leere Links
        if (href === '#' || !href) {
          e.preventDefault();
          return;
        }
        // Do not prevent clicks on submenu links; top-level ADMIN is handled separately
      });
    });

    // Top-Level Navigationsanker (IDs werden serverseitig gesetzt)
     const navAdmin = document.getElementById('nav-admin');
     const navStart = document.getElementById('nav-start');
     const navLogs = document.getElementById('nav-logs');

    if (navAdmin) {
      navAdmin.addEventListener('click', (e) => {
        // Always intercept ADMIN clicks on the start page (no navigation)
        if (location.pathname === '/' || location.pathname === '' || location.pathname.endsWith('/index.html')) {
          e.preventDefault();
          toggleFooterAdminMenu();
        } else {
          // On other pages, allow normal navigation
        }
      });
    }

    // Admin-Menü verbergen, wenn zu START oder LOGS navigiert wird
    const hideAdminOnClick = () => hideFooterAdminMenu();
     if (navStart) navStart.addEventListener('click', hideAdminOnClick);
     if (navLogs) navLogs.addEventListener('click', hideAdminOnClick);
   }
});

// Sensor data update functions
function updateSensorValues() {
  // Der ESP beantwortet nur eine Verbindung gleichzeitig. Der 10-Sekunden-Takt,
  // der Sichtbarkeitswechsel und das enge Nachfragen nach einer ausgelösten
  // Messung dürfen sich deshalb nicht überholen.
  if (pollInFlight) return Promise.resolve();
  pollInFlight = true;

  console.log('Updating sensor values...');

  return fetch('/getLatestValues')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      updateFailureCount = 0;
      return response.json();
    })
    .then(data => {
      // Update system time offset
      if (data.currentTime) {
        window._serverStartTime = Date.now() - data.currentTime;
      }

      // Update footer stats
      updateFooterStats(data);

      // Update sensor values
      if (data.sensors) {
        latestSensorData = data.sensors;

        // Determine flower face status from configured sensor
        // Default to ANALOG_1 if not specified in data
        const flowerSensorId = data.flowerStatusSensor || 'ANALOG_1';
        const flowerStatus = data.sensors[flowerSensorId]
                           ? data.sensors[flowerSensorId].status
                           : 'unknown';
        updateFlowerFace(flowerStatus);

        // Mark active sensor (determines overall flower status)
        document.querySelectorAll('.sensor').forEach(sensor => {
          sensor.classList.remove('active');
        });
        const activeSensorElement = document.querySelector(`[data-sensor="${flowerSensorId}"]`);
        if (activeSensorElement) {
          activeSensorElement.classList.add('active');
        }

        Object.entries(data.sensors).forEach(([fieldName, sensorData]) => {
          const sensorElement = document.querySelector(`[data-sensor="${fieldName}"]`);
          if (sensorElement) {
            updateSensorCard(sensorElement, sensorData);
          }
        });
      }
    })
    .catch(error => {
      console.error('Error fetching sensor values:', error);
      updateFailureCount++;

      if (updateFailureCount >= MAX_UPDATE_FAILURES) {
        console.error('Too many update failures, showing error state');
        showErrorState();
      }
    })
    .then(() => {
      // Läuft in beiden Fällen - .catch() liefert eine erfüllte Zusage zurück.
      pollInFlight = false;
    });
}

function updateFooterStats(data) {
  // Update IP if available
  if (data.ip) {
    const ipElement = document.querySelector('#footer-stats-values li:nth-child(3)');
    if (ipElement) {
      ipElement.textContent = data.ip;
    }
  }
}

function updateSensorCard(sensorElement, sensorData) {
  // Update value
  const valueElement = sensorElement.querySelector('.value span');
  if (valueElement && sensorData.value !== undefined) {
    const unit = sensorData.unit || '';
    valueElement.textContent = `${parseFloat(sensorData.value).toFixed(1)}${unit}`;
  }

  // Update status
  const statusElement = sensorElement.querySelector('.status');
  if (statusElement && sensorData.status) {
    const statusText = translateStatus(sensorData.status);
    const statusSpan = statusElement.querySelector('span');
    if (statusSpan) {
      statusSpan.textContent = `STATUS: ${statusText}`;
    }

    // Update status color class
    // 'warmup' gehört dazu: der Zustand kommt vom Gerät wie jeder andere, blieb
    // aber als Klasse kleben und färbte Text und Blatt danach weiter falsch.
    statusElement.classList.remove('green', 'yellow', 'red', 'error', 'warmup', 'unknown');
    statusElement.classList.add(sensorData.status);

    // Update sensor status class for leaf animations
    sensorElement.classList.remove('sensor-status-green', 'sensor-status-yellow', 'sensor-status-red', 'sensor-status-error', 'sensor-status-warmup', 'sensor-status-unknown');
    sensorElement.classList.add(`sensor-status-${sensorData.status}`);
  }

  // Neue Messung erkennen. Der Zeitstempel kommt vom Gerät und ändert sich
  // genau dann, wenn ein neuer Messzyklus begonnen hat.
  //
  // Beim ersten Abruf nach dem Laden steht noch keiner im Datensatz - das
  // serverseitig gebaute HTML setzt ihn nicht -, dann wird nicht gewackelt.
  // Sonst wackelten beim Öffnen der Seite alle Blätter auf einmal.
  //
  // Verglichen wird auf Ungleichheit statt auf "größer": nach einem Neustart
  // des Geräts fängt millis() wieder bei null an, und auch das ist eine echte
  // neue Messung.
  if (sensorData.lastMeasurement !== undefined && sensorData.lastMeasurement !== null) {
    const previous = sensorElement.dataset.lastMeasurement;
    const current = String(sensorData.lastMeasurement);
    sensorElement.dataset.lastMeasurement = current;
    if (previous && previous !== current) {
      onNewMeasurement(sensorElement);
    }
  }
  if (sensorData.measurementInterval) {
    sensorElement.dataset.measurementInterval = sensorData.measurementInterval;
  }

  // Update interval/timing
  const intervalElement = sensorElement.querySelector('.interval span');
  if (intervalElement && sensorData.lastMeasurement && sensorData.measurementInterval &&
      !countdownSuppressed(sensorElement)) {
    const now = Date.now();
    const serverTime = window._serverStartTime ? (now - window._serverStartTime) : now;
    const elapsed = Math.floor((serverTime - sensorData.lastMeasurement) / 1000);
    const intervalSec = Math.floor(sensorData.measurementInterval / 1000);
    intervalElement.textContent = `(${elapsed}s/${intervalSec}s)`;
  }
}

function updateCountdowns() {
  releaseStaleMeasuring();

  const sensors = document.querySelectorAll('.sensor[data-last-measurement]');

  sensors.forEach(sensor => {
    // Wartet die Zeile gerade auf eine Messung oder zeigt sie einen Hinweis,
    // bleibt der Text stehen - sonst überschriebe ihn dieser Sekundentakt sofort.
    if (countdownSuppressed(sensor)) return;

    const lastMeasurement = parseInt(sensor.dataset.lastMeasurement);
    const measurementInterval = parseInt(sensor.dataset.measurementInterval);

    if (!lastMeasurement || !measurementInterval) return;

    const now = Date.now();
    const serverTime = window._serverStartTime ? (now - window._serverStartTime) : now;
    const elapsed = Math.floor((serverTime - lastMeasurement) / 1000);
    const intervalSec = Math.floor(measurementInterval / 1000);

    const intervalElement = sensor.querySelector('.interval span');
    if (intervalElement) {
      intervalElement.textContent = `(${elapsed}s/${intervalSec}s)`;
    }
  });
}

// === Messung per Klick auslösen und neue Messwerte sichtbar machen ===

/**
 * Reine Sensor-ID einer Zeile (ohne Messwertindex).
 * data-sensor ist "<ID>_<Index>", und IDs enthalten selbst Unterstriche
 * (ANALOG_1 -> ANALOG_1_0). Deshalb liefert der Server die ID zusätzlich
 * ungeschnitten in data-sensor-id; das Abschneiden ist nur der Rückfall für
 * Geräte, deren Firmware älter ist als diese Datei.
 */
function sensorIdOf(sensorElement) {
  if (sensorElement.dataset.sensorId) return sensorElement.dataset.sensorId;
  const key = sensorElement.dataset.sensor || '';
  const cut = key.lastIndexOf('_');
  return cut > 0 ? key.substring(0, cut) : key;
}

/** Alle Zeilen desselben Sensors - ein Sensor misst immer alle seine Messwerte
 *  auf einmal, also gehören sie gemeinsam in den Wartezustand. */
function rowsOfSensor(sensorId) {
  const all = document.querySelectorAll('.sensor');
  return Array.prototype.filter.call(all, el => sensorIdOf(el) === sensorId);
}

/** Zeigt die Zeile gerade einen befristeten Hinweis statt des Countdowns? */
function rowNoteActive(sensorElement) {
  const until = parseInt(sensorElement.dataset.noteUntil);
  if (!until) return false;
  if (Date.now() < until) return true;
  delete sensorElement.dataset.noteUntil;
  return false;
}

function countdownSuppressed(sensorElement) {
  return sensorElement.classList.contains('measuring') || rowNoteActive(sensorElement);
}

/** Kurzen Hinweis ins Intervallfeld schreiben, der den Countdown übertönt. */
function setRowNote(sensorElement, text, durationMs) {
  sensorElement.dataset.noteUntil = String(Date.now() + durationMs);
  const intervalElement = sensorElement.querySelector('.interval span');
  if (intervalElement) intervalElement.textContent = text;
}

function startMeasuring(sensorElement) {
  sensorElement.classList.add('measuring');
  sensorElement.dataset.measuringSince = String(Date.now());
  sensorElement.setAttribute('aria-busy', 'true');
  const intervalElement = sensorElement.querySelector('.interval span');
  if (intervalElement) intervalElement.textContent = '(misst …)';
}

function stopMeasuring(sensorElement) {
  sensorElement.classList.remove('measuring');
  delete sensorElement.dataset.measuringSince;
  sensorElement.removeAttribute('aria-busy');
}

/** Notbremse: bleibt eine Antwort aus, atmete das Blatt sonst endlos weiter. */
function releaseStaleMeasuring() {
  const waiting = document.querySelectorAll('.sensor.measuring');
  waiting.forEach(sensorElement => {
    const since = parseInt(sensorElement.dataset.measuringSince);
    if (since && (Date.now() - since) <= MEASURING_TIMEOUT_MS) return;
    stopMeasuring(sensorElement);
    setRowNote(sensorElement, '(keine Antwort)', 3000);
  });
}

/** Wackeln lassen: eine neue Messung ist eingetroffen. */
function onNewMeasurement(sensorElement) {
  stopMeasuring(sensorElement);
  delete sensorElement.dataset.noteUntil;

  // Klasse entfernen, Reflow erzwingen, wieder setzen: sonst startet die
  // Animation bei zwei kurz aufeinanderfolgenden Messungen nicht neu, weil sich
  // aus Sicht des Browsers nichts geändert hat.
  sensorElement.classList.remove('wiggle');
  void sensorElement.offsetWidth;
  sensorElement.classList.add('wiggle');
  setTimeout(() => sensorElement.classList.remove('wiggle'), WIGGLE_MS);
}

/**
 * Sofortmessung für den Sensor dieser Zeile anfordern.
 * Ohne Anmeldung erreichbar (POST /measure), die Drossel sitzt im Gerät.
 */
function triggerMeasurement(sensorElement) {
  if (!sensorElement) return Promise.resolve();

  const sensorId = sensorIdOf(sensorElement);
  if (!sensorId) return Promise.resolve();

  if (sensorElement.classList.contains('measuring')) return Promise.resolve();
  const blockedUntil = measureCooldownUntil[sensorId];
  if (blockedUntil && Date.now() < blockedUntil) return Promise.resolve();

  const rows = rowsOfSensor(sensorId);
  rows.forEach(startMeasuring);

  return fetch('/measure', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'sensor=' + encodeURIComponent(sensorId)
  })
    .then(response => {
      // Der Rumpf ist immer JSON; bei einem Fehler unterwegs soll trotzdem der
      // Statuscode entscheiden und nicht ein Parserfehler.
      const asJson = () => ({ ok: response.ok, status: response.status, data: {} });
      return response.json().then(
        data => ({ ok: response.ok, status: response.status, data: data || {} }), asJson);
    })
    .then(result => {
      if (result.ok) {
        measureCooldownUntil[sensorId] =
          Date.now() + (result.data.cooldownMs || MEASURE_COOLDOWN_MS);
        startFastPolls();
        return;
      }

      const zuVieleAnfragen = result.status === 429;
      measureCooldownUntil[sensorId] =
        Date.now() + (zuVieleAnfragen && result.data.retryAfterMs ? result.data.retryAfterMs
                                                                 : MEASURE_COOLDOWN_MS);
      rows.forEach(row => {
        stopMeasuring(row);
        setRowNote(row, zuVieleAnfragen ? '(bitte warten)' : '(Fehler)', 3000);
      });
    })
    .catch(error => {
      console.error('Messung konnte nicht ausgelöst werden:', error);
      rows.forEach(row => {
        stopMeasuring(row);
        setRowNote(row, '(Fehler)', 3000);
      });
    });
}

/** Nach dem Auslösen enger nachfragen, bis der neue Wert da ist. */
function startFastPolls() {
  fastPollStep = 0;
  clearTimeout(fastPollTimer);
  scheduleNextFastPoll();
}

function scheduleNextFastPoll() {
  if (fastPollStep >= FAST_POLL_DELAYS_MS.length) return;
  const delay = FAST_POLL_DELAYS_MS[fastPollStep++];
  fastPollTimer = setTimeout(() => {
    updateSensorValues().then(() => {
      // Sobald keine Zeile mehr wartet, ist der Wert angekommen - Kette beenden.
      if (document.querySelector('.sensor.measuring')) scheduleNextFastPoll();
    });
  }, delay);
}

/**
 * Ein Zuhörer für alle Sensorzeilen statt einer pro Zeile: die Zeilen sind
 * gleichartig, und so überlebt die Bindung auch ein späteres Neuaufbauen.
 */
function bindSensorTriggers() {
  const root = document.querySelector('.sensors-container') || document;

  const rowOf = (target) => (target && target.closest ? target.closest('.sensor') : null);

  root.addEventListener('click', event => {
    triggerMeasurement(rowOf(event.target));
  });

  root.addEventListener('keydown', event => {
    if (event.key !== 'Enter' && event.key !== ' ' && event.key !== 'Spacebar') return;
    const row = rowOf(event.target);
    if (!row) return;
    event.preventDefault(); // die Leertaste würde sonst die Seite scrollen
    triggerMeasurement(row);
  });
}

function updateFlowerFace(status) {
  const box = document.querySelector('.box');
  const faceImage = document.querySelector('.face');

  if (!box || !faceImage) return;

  // Remove all status classes
  box.classList.remove('status-green', 'status-yellow', 'status-red', 'status-error', 'status-unknown');

  // Add current status and update face
  switch(status) {
    case 'green':
      box.classList.add('status-green');
      faceImage.src = '/img/face-happy.gif';
      break;
    case 'yellow':
      box.classList.add('status-yellow');
      faceImage.src = '/img/face-neutral.gif';
      break;
    case 'red':
      box.classList.add('status-red');
      faceImage.src = '/img/face-sad.gif';
      break;
    case 'error':
      box.classList.add('status-error');
      faceImage.src = '/img/face-error.gif';
      break;
    default:
      box.classList.add('status-unknown');
      faceImage.src = '/img/face-error.gif';
  }
}

function showErrorState() {
  const box = document.querySelector('.box');
  const faceImage = document.querySelector('.face');

  if (box) {
    box.classList.remove('status-green', 'status-yellow', 'status-red', 'status-unknown');
    box.classList.add('status-error');
  }

  if (faceImage) {
    faceImage.src = '/img/face-error.gif';
  }
}

function translateStatus(status) {
  const translations = {
    'green': 'OK',
    'yellow': 'WARNUNG',
    'red': 'KRITISCH',
    'error': 'FEHLER',
    'warmup': 'AUFWÄRMEN',
    'unknown': 'UNBEKANNT'
  };

  return translations[status] || status.toUpperCase();
}

// Für die Tests herausgereicht (test/js/sensors.test.mjs) - im Browser wird
// nichts davon von außen aufgerufen.
if (typeof window !== 'undefined') {
  window.Sensors = {
    updateSensorValues,
    updateSensorCard,
    updateCountdowns,
    translateStatus,
    triggerMeasurement,
    onNewMeasurement,
    sensorIdOf,
    rowsOfSensor,
    bindSensorTriggers,
    releaseStaleMeasuring,
    measureCooldownUntil
  };
}
