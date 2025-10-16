/**
 * @file sensors.js
 * @brief JavaScript for automatic sensor value and countdown updates (New UI)
 */

let updateFailureCount = 0;
const MAX_UPDATE_FAILURES = 3;
let latestSensorData = {};

window.addEventListener('DOMContentLoaded', () => {
  const cloud = document.querySelector('.cloud');
  const box = document.querySelector('.box');
  const earth = document.querySelector('.earth');

  // Start sensor value updates
  updateSensorValues();
  setInterval(updateSensorValues, 10000); // Update every 10 seconds

  // Update countdown timers more frequently
  setInterval(updateCountdowns, 1000); // Update every second

  // Smooth scroll animation on page load
  if (box && earth) {
    // Wait a brief moment for layout to settle
    setTimeout(() => {
      const maxScroll = box.scrollHeight - box.clientHeight;
      // Only animate if there's content to scroll
      if (maxScroll <= 0) {
        console.log('No scrollable content - page fits in viewport');
        return;
      }

      // Smooth scroll to bottom
      const scrollToBottom = () => {
        return new Promise((resolve) => {
          const duration = 1500; // 1.5 seconds to scroll down
          const start = box.scrollTop;
          const startTime = performance.now();

          function animateScroll(currentTime) {
            const elapsed = currentTime - startTime;
            const progress = Math.min(elapsed / duration, 1);

            // Easing function for smooth animation
            const easeInOutCubic = progress < 0.5
              ? 4 * progress * progress * progress
              : 1 - Math.pow(-2 * progress + 2, 3) / 2;

            box.scrollTop = start + (maxScroll - start) * easeInOutCubic;

            if (progress < 1) {
              requestAnimationFrame(animateScroll);
            } else {
              resolve();
            }
          }

          requestAnimationFrame(animateScroll);
        });
      };

      // Scroll back up to show just the bottom of earth
      const scrollBackUp = () => {
        return new Promise((resolve) => {
          const maxScroll = box.scrollHeight - box.clientHeight;

          // Scroll back up by a percentage of viewport height (relative)
          const scrollUpAmount = box.clientHeight * 0.25; // 25% of viewport height
          const targetScroll = maxScroll - scrollUpAmount;
          const duration = 800; // 0.8 seconds to scroll back up
          const start = box.scrollTop;
          const startTime = performance.now();

          function animateScroll(currentTime) {
            const elapsed = currentTime - startTime;
            const progress = Math.min(elapsed / duration, 1);

            // Easing function for smooth animation
            const easeInOutCubic = progress < 0.5
              ? 4 * progress * progress * progress
              : 1 - Math.pow(-2 * progress + 2, 3) / 2;

            box.scrollTop = start + (targetScroll - start) * easeInOutCubic;

            if (progress < 1) {
              requestAnimationFrame(animateScroll);
            } else {
              resolve();
            }
          }

          requestAnimationFrame(animateScroll);
        });
      };

      // Execute scroll sequence
      scrollToBottom().then(() => {
        // Small pause at bottom
        setTimeout(() => {
          scrollBackUp();
        }, 300);
      });
    }, 100);
  }

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

  if (faceImage && navLinks.length > 0) {
    navLinks.forEach(link => {
      link.addEventListener('click', (e) => {
        const href = link.getAttribute('href');
        // Only prevent default for hash or empty links
        if (href === '#' || !href) {
          e.preventDefault();
        }
      });
    });
  }
});

// Sensor data update functions
function updateSensorValues() {
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
    });
}

function updateFooterStats(data) {
  // Update IP if available
  if (data.ip) {
    const ipElement = document.querySelector('.stats-values li:nth-child(3)');
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
    statusElement.classList.remove('green', 'yellow', 'red', 'error', 'unknown');
    statusElement.classList.add(sensorData.status);

    // Update sensor status class for leaf animations
    sensorElement.classList.remove('sensor-status-green', 'sensor-status-yellow', 'sensor-status-red', 'sensor-status-error', 'sensor-status-unknown');
    sensorElement.classList.add(`sensor-status-${sensorData.status}`);
  }

  // Update interval/timing
  const intervalElement = sensorElement.querySelector('.interval span');
  if (intervalElement && sensorData.lastMeasurement && sensorData.measurementInterval) {
    const now = Date.now();
    const serverTime = window._serverStartTime ? (now - window._serverStartTime) : now;
    const elapsed = Math.floor((serverTime - sensorData.lastMeasurement) / 1000);
    const intervalSec = Math.floor(sensorData.measurementInterval / 1000);
    intervalElement.textContent = `(${elapsed}s/${intervalSec}s)`;

    // Store timing data for countdown updates
    sensorElement.dataset.lastMeasurement = sensorData.lastMeasurement;
    sensorElement.dataset.measurementInterval = sensorData.measurementInterval;
  }
}

function updateCountdowns() {
  const sensors = document.querySelectorAll('.sensor[data-last-measurement]');

  sensors.forEach(sensor => {
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
