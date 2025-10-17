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
