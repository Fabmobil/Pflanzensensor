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
