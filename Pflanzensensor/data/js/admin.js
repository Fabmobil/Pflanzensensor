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
 * Updates the navigation to show active items (for footer nav-items)
 */
function initNavigation() {
  const path = window.location.pathname;

  // Update footer navigation links
  document.querySelectorAll('.nav-item').forEach(link => {
    const href = link.getAttribute('href');
    if (href === path || (href && path.startsWith(href) && href !== '/')) {
      link.classList.add('active');
    }
  });
}

// Initialize on load
window.addEventListener('load', () => {
  initNavigation();
});
