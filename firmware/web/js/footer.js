/**
 * @file footer.js
 * @brief Gemeinsamer Kopf (Wolke) und Fußzeile aller Seiten
 *
 * Die Seiten sind statisch; Gerätename, Statustabelle und Version kommen aus
 * GET /api/v1/system. Auf Admin-Seiten (und auf der Startseite nach Klick auf
 * ADMIN) erscheint statt der Statustabelle das Admin-Menü.
 */
(function (global) {
  'use strict';
  const { el } = global.UI;

  const ADMIN_LINKS = [
    ['/admin', 'Einstellungen'],
    ['/admin/sensors', 'Sensoren'],
    ['/admin/display', 'Display', 'display'],
    ['/admin/mail', 'E-Mail'],
    ['/admin/update', 'Update']
  ];

  let systemInfo = null;
  const listeners = [];

  function section() {
    const p = location.pathname.replace(/\/+$/, '') || '/';
    if (p === '/' || p === '/index.html') return 'start';
    if (p.startsWith('/chronik')) return 'chronik';
    if (p.startsWith('/logs')) return 'logs';
    return 'admin';
  }

  function renderCloud(title) {
    const box = document.getElementById('cloud');
    if (!box) return;
    box.textContent = '';
    box.setAttribute('aria-label', title);
    box.appendChild(el('img', { class: 'cloud-img', src: '/img/cloud_big.png', alt: '' }));
    box.appendChild(el('div', { class: 'cloud-label', id: 'cloud-label', text: title }));
  }

  function statsRow(label, id) {
    return el('div', { class: 'stats-row' }, [
      el('span', { class: 'stats-label', text: label }),
      el('span', { class: 'stats-value', id })
    ]);
  }

  function render() {
    const host = document.getElementById('footer');
    if (!host) return;
    const sec = section();
    const nav = el('ul', { class: 'nav-list' });
    [['/', 'START', 'start'], ['/chronik', 'CHRONIK', 'chronik'], ['/logs', 'LOGS', 'logs'],
     ['/admin', 'ADMIN', 'admin']].forEach(([href, text, s]) => {
      nav.appendChild(el('li', {}, [el('a', { href, id: 'nav-' + s, class: 'nav-item' + (sec === s ? ' active' : ''), text })]));
    });
    const stats = el('div', { class: 'stats-table', id: 'footer-stats-table' }, [
      statsRow('📅 Zeit', 'f-time'), statsRow('🌐 SSID', 'f-ssid'), statsRow('🏷️ NAME', 'f-name'),
      statsRow('💻 IP', 'f-ip'), statsRow('📶 WIFI', 'f-rssi'), statsRow('⏲️ UPTIME', 'f-uptime'),
      statsRow('🔄 RESTARTS', 'f-reboots')
    ]);
    const menu = el('div', { class: 'stats-table', id: 'footer-admin-menu' });
    ADMIN_LINKS.forEach(([href, text, feature]) => {
      const row = el('div', { class: 'stats-row', 'data-feature': feature || null }, [
        el('span', { class: 'stats-label' }, [el('a', { href, class: 'nav-item' + (location.pathname === href ? ' active' : ''), text })]),
        el('span', { class: 'stats-value', text: ' ' })
      ]);
      menu.appendChild(row);
    });
    const logout = el('a', { href: '/', class: 'nav-item', text: 'Abmelden' });
    logout.addEventListener('click', e => {
      e.preventDefault();
      global.Api.post('/api/v1/logout', null, { quiet: true }).catch(() => {}).then(() => { location.href = '/'; });
    });
    menu.appendChild(el('div', { class: 'stats-row' }, [el('span', { class: 'stats-label' }, [logout]),
      el('span', { class: 'stats-value', text: ' ' })]));
    // Auf der Anmeldeseite gibt es noch kein Admin-Menü
    const admin = sec === 'admin' && location.pathname !== '/login';
    stats.style.display = admin ? 'none' : '';
    menu.style.display = admin ? '' : 'none';
    host.className = 'footer';
    host.textContent = '';
    host.appendChild(el('div', { class: 'base' }, [
      el('img', { class: 'earth', src: '/img/earth.png', alt: 'Erde' }),
      el('footer', { class: 'base-overlay', 'aria-label': 'Statusleiste' }, [
        el('div', { class: 'footer-grid' }, [
          el('nav', { class: 'nav-box', 'aria-label': 'Navigation' }, [nav]),
          stats, menu,
          el('div', { class: 'footer-logo' }, [
            el('a', { href: 'https://www.fabmobil.org', target: '_blank', rel: 'noopener' },
               [el('img', { src: '/img/fabmobil.png', alt: 'FABMOBIL' })])]),
          el('div', { class: 'footer-version', id: 'f-version', text: 'V –' }),
          el('div', { class: 'footer-build', id: 'f-build', text: 'BUILD: –' })
        ])
      ])
    ]));
    // Startseite: ADMIN klappt das Menü auf statt zu navigieren
    if (sec === 'start') {
      document.getElementById('nav-admin').addEventListener('click', e => {
        e.preventDefault();
        const show = menu.style.display === 'none';
        menu.style.display = show ? '' : 'none';
        stats.style.display = show ? 'none' : '';
        e.currentTarget.classList.toggle('active', show);
      });
    }
  }

  function set(id, text) {
    const e = document.getElementById(id);
    if (e) e.textContent = text;
  }

  function fill(s) {
    systemInfo = s;
    set('f-time', s.time || 'Uhrzeit unbekannt');
    set('f-ssid', s.ap ? s.ssid + ' (AP)' : (s.ssid || '–'));
    set('f-name', (s.hostname || '–') + '.local');
    set('f-ip', s.ip || '–');
    set('f-rssi', s.ap ? '–' : s.rssi + ' dBm');
    set('f-uptime', global.UI.formatUptime(s.uptime));
    set('f-reboots', String(s.rebootCount));
    set('f-version', 'V ' + s.version);
    set('f-build', 'BUILD: ' + s.buildDate);
    if (section() === 'start') {
      renderCloud(s.deviceName);
      document.title = s.deviceName;
    } else {
      document.title = (document.body.dataset.title || '') + ' – ' + s.deviceName;
    }
    if (!s.features || !s.features.display) {
      document.querySelectorAll('[data-feature="display"]').forEach(e => { e.style.display = 'none'; });
    }
    listeners.forEach(fn => fn(s));
  }

  function refresh() {
    return global.Api.get('/api/v1/system', { quiet: true }).then(fill).catch(() => {});
  }

  function onSystem(fn) {
    listeners.push(fn);
    if (systemInfo) fn(systemInfo);
  }

  if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', () => {
      renderCloud(document.body.dataset.title || '');
      render();
      refresh();
      setInterval(() => { if (document.visibilityState !== 'hidden') refresh(); }, 30000);
      // Wolke schwebt sanft (Sinus x ±30 px, y ±8 px)
      const cloud = document.getElementById('cloud');
      if (cloud && global.requestAnimationFrame) {
        const t0 = performance.now();
        const step = t => {
          const s = (t - t0) / 1000;
          cloud.style.transform = `translate(${Math.sin(s * 0.3) * 30}px, ${Math.sin(s * 0.9) * 8}px)`;
          requestAnimationFrame(step);
        };
        requestAnimationFrame(step);
      }
    });
  }

  global.Footer = { refresh, onSystem, section };
})(typeof window !== 'undefined' ? window : globalThis);
