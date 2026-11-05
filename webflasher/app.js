/**
 * Webflasher: Versionen aus releases.json, Manifest an den Installationsknopf.
 */
(function () {
  'use strict';
  const select = document.getElementById('version');
  const install = document.getElementById('install');
  const notes = document.getElementById('notes');
  const chips = document.getElementById('chips');
  const CHIP_TEXT = { ESP8266: 'ESP8266 (Pflanzensensor-Platine, NodeMCU)', ESP32: 'ESP32 (DevKit)' };

  function label(r) {
    return 'v' + r.version + (r.prerelease ? ' (Vorabversion)' : '') + (r.date ? ' – ' + r.date : '');
  }

  function choose(r) {
    install.setAttribute('manifest', r.manifest);
    notes.href = r.url || '#';
    notes.hidden = !r.url;
    chips.textContent = 'Für: ' + r.chips.map(c => CHIP_TEXT[c] || c).join(', ');
  }

  fetch('releases.json', { cache: 'no-store' }).then(r => r.json()).then(data => {
    const list = data.releases || [];
    if (!list.length) {
      document.getElementById('empty').hidden = false;
      select.disabled = true;
      install.hidden = true;
      return;
    }
    list.forEach((r, i) => {
      const o = document.createElement('option');
      o.value = String(i);
      o.textContent = label(r);
      select.appendChild(o);
    });
    // Standard: neueste reguläre Version
    const stable = list.findIndex(r => !r.prerelease);
    select.value = String(stable >= 0 ? stable : 0);
    choose(list[Number(select.value)]);
    select.addEventListener('change', () => choose(list[Number(select.value)]));
  }).catch(() => {
    document.getElementById('empty').hidden = false;
    document.getElementById('empty').textContent = 'Versionsliste konnte nicht geladen werden.';
  });
})();
