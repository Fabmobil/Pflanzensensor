/**
 * @file threshold-slider.js
 * @brief Schwellenregler: Farbverlauf rot|gelb|grün|gelb|rot, 4 ziehbare Griffe,
 *        Zahlen an den Griffen, Skala mit Teilstrichen, aktueller Wert und
 *        beobachteter Bereich (laufend aktualisiert)
 *
 * Gegenüber der Firmware 2.x korrigiert: gleiche Werte und einseitige Kanäle
 * werden dargestellt (B-57); der Skalenbereich richtet sich nach Einheit und
 * Werten, nicht nach den Rohgrenzen eines Analogkanals (B-58).
 */
(function (global) {
  'use strict';

  const KEYS = ['yellowLow', 'greenLow', 'greenHigh', 'yellowHigh'];

  /** Skalenbereich [min, max] für einen Kanal */
  function calculateRange(ch) {
    if (ch.unit === '%') return [0, 100];
    const vals = [];
    const keys = ch.oneSided ? ['greenHigh', 'yellowHigh'] : KEYS;
    keys.forEach(k => vals.push(Number(ch[k])));
    [ch.value, ch.obsMin, ch.obsMax].forEach(v => {
      if (v !== null && v !== undefined && isFinite(v)) vals.push(Number(v));
    });
    if (ch.oneSided) vals.push(0);
    let lo = Math.min(...vals), hi = Math.max(...vals);
    if (!isFinite(lo) || !isFinite(hi)) return [0, 100];
    if (hi === lo) { lo -= 1; hi += 1; }
    const pad = (hi - lo) * 0.15;
    lo -= pad;
    hi += pad;
    if (isFinite(ch.validMin)) lo = Math.max(lo, ch.validMin);
    if (isFinite(ch.validMax)) hi = Math.min(hi, ch.validMax);
    if (hi <= lo) hi = lo + 1;
    return [lo, hi];
  }

  /** Werte geordnet halten: ein Griff schiebt die Nachbarn nicht, er stoppt an ihnen */
  function clampOrdered(limits, key, value, oneSided) {
    const out = Object.assign({}, limits);
    const order = oneSided ? ['greenHigh', 'yellowHigh'] : KEYS;
    const i = order.indexOf(key);
    if (i < 0) return out;
    const lo = i > 0 ? out[order[i - 1]] : -Infinity;
    const hi = i < order.length - 1 ? out[order[i + 1]] : Infinity;
    out[key] = Math.min(hi, Math.max(lo, value));
    return out;
  }

  /** CSS-Verlauf aus den Grenzen */
  function gradient(limits, range, oneSided) {
    const pct = v => Math.max(0, Math.min(100, (v - range[0]) / (range[1] - range[0]) * 100));
    const R = '#ff6f6f', Y = '#ffd24a', G = '#5ac85a';
    if (oneSided) {
      const gh = pct(limits.greenHigh), yh = pct(limits.yellowHigh);
      return `linear-gradient(to right, ${G} 0%, ${G} ${gh}%, ${Y} ${gh}%, ${Y} ${yh}%, ${R} ${yh}%, ${R} 100%)`;
    }
    const [a, b, c, d] = KEYS.map(k => pct(limits[k]));
    return `linear-gradient(to right, ${R} 0%, ${R} ${a}%, ${Y} ${a}%, ${Y} ${b}%, ${G} ${b}%, ${G} ${c}%, ${Y} ${c}%, ${Y} ${d}%, ${R} ${d}%, ${R} 100%)`;
  }

  function decimalsFor(range) {
    const span = range[1] - range[0];
    return span > 1000 ? 0 : span > 20 ? 1 : 2;
  }

  /** Schrittweite der Skala: 1, 2 oder 5 mal Zehnerpotenz, etwa 5 Teilstriche */
  function tickStep(range) {
    const raw = (range[1] - range[0]) / 5;
    const p = Math.pow(10, Math.floor(Math.log10(raw)));
    const m = raw / p;
    return (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * p;
  }

  /** Teilstriche innerhalb des Bereichs */
  function ticks(range) {
    const step = tickStep(range);
    const out = [];
    for (let v = Math.ceil(range[0] / step) * step; v <= range[1] + step * 1e-6; v += step) {
      out.push(Math.abs(v) < step * 1e-6 ? 0 : v);
    }
    return out;
  }

  /** Nachkommastellen für eine Zahl in dieser Schrittweite */
  function decimalsForStep(step) {
    return step >= 1 ? 0 : Math.min(3, Math.ceil(-Math.log10(step)));
  }

  /**
   * Beschriftungen in Reihen verteilen, damit sie sich nicht überdecken.
   * @param items [{pos, width}] in Pixeln, nach pos sortiert
   * @return Reihe je Eintrag (0 = nah am Balken)
   */
  function stackRows(items, gap) {
    const rowEnd = [];
    return items.map(it => {
      const left = it.pos - it.width / 2;
      let r = 0;
      while (r < rowEnd.length && rowEnd[r] + gap > left) r++;
      rowEnd[r] = it.pos + it.width / 2;
      return r;
    });
  }

  const LABELS = { yellowLow: 'Gelb unten', greenLow: 'Grün unten', greenHigh: 'Grün oben', yellowHigh: 'Gelb oben' };

  /**
   * Regler erzeugen.
   * @param ch Kanal (Grenzen, oneSided, unit, value, obsMin/Max, validMin/Max)
   * @param onChange(limits, key) bei jeder Änderung durch Ziehen oder Pfeiltasten
   */
  function create(ch, onChange) {
    const { el } = global.UI;
    const range = calculateRange(ch);
    const unit = ch.unit || '';
    const active = ch.oneSided ? ['greenHigh', 'yellowHigh'] : KEYS;
    let limits = {};
    KEYS.forEach(k => { limits[k] = Number(ch[k]); });
    let live = { value: ch.value, obsMin: ch.obsMin, obsMax: ch.obsMax };
    const step = tickStep(range);
    /** Raster für Ziehen und Pfeiltasten: 0-100 % in ganzen Zahlen, 25 °C in Zehnteln */
    const grid = Math.pow(10, Math.floor(Math.log10(range[1] - range[0])) - 2);
    const gridDec = Math.max(0, -Math.floor(Math.log10(grid)));
    const dec = decimalsFor(range);
    const tickDec = decimalsForStep(step);
    const fmt = (v, d) => Number(v).toFixed(d);
    const trim = v => String(Number(Number(v).toFixed(Math.max(dec, gridDec))));
    const pct = v => Math.max(0, Math.min(100, (v - range[0]) / (range[1] - range[0]) * 100));
    const finite = v => v !== null && v !== undefined && isFinite(v);

    const top = el('div', { class: 'ts-top' });
    const bar = el('div', { class: 'threshold-gradient-bar' });
    const band = el('div', { class: 'ts-observed' });
    const dot = el('div', { class: 'measured-value-dot', title: 'aktueller Wert' });
    const box = el('div', { class: 'threshold-container' + (ch.oneSided ? ' one-sided' : '') }, [bar, band, dot]);
    const axis = el('div', { class: 'ts-axis' });
    const bottom = el('div', { class: 'ts-bottom' });
    const handles = {}, chips = {};

    const tickList = ticks(range);
    tickList.forEach((v, i) => {
      const last = i === tickList.length - 1;
      const t = el('div', { class: 'ts-tick' + (pct(v) < 3 ? ' first' : '') + (pct(v) > 97 ? ' last' : '') },
                   [el('span', { text: fmt(v, tickDec) + (last && unit ? ' ' + unit : '') })]);
      t.style.left = pct(v) + '%';
      axis.appendChild(t);
    });

    const valueChip = el('div', { class: 'ts-chip ts-chip-value', title: 'aktueller Wert' });
    bottom.appendChild(valueChip);

    /** Chips einer Zeile setzen: Text, Position, Reihe gegen Überdeckung */
    function place(container, list) {
      const width = container.getBoundingClientRect().width || 0;
      const shown = list.filter(c => c.show);
      list.forEach(c => { c.el.style.display = c.show ? '' : 'none'; });
      shown.forEach(c => { c.el.textContent = c.text; });
      if (!width) {
        shown.forEach(c => { c.el.style.left = c.pct + '%'; c.el.style.top = '0px'; });
        return 1;
      }
      const items = shown.map(c => ({ c, pos: c.pct / 100 * width, width: c.el.offsetWidth || 40 }))
        .sort((a, b) => a.pos - b.pos);
      const rows = stackRows(items, 4);
      items.forEach((it, i) => {
        const left = Math.max(0, Math.min(width - it.width, it.pos - it.width / 2));
        it.c.el.style.left = left + 'px';
        it.c.el.style.top = rows[i] * 26 + 'px';
      });
      return rows.length ? Math.max(...rows) + 1 : 0;
    }

    function draw() {
      bar.style.background = gradient(limits, range, ch.oneSided);
      KEYS.forEach(k => {
        handles[k].style.left = pct(limits[k]) + '%';
        handles[k].title = LABELS[k] + ': ' + fmt(limits[k], dec) + unit;
        handles[k].setAttribute('aria-valuenow', String(limits[k]));
      });
      const topRows = place(top, active.map(k => ({ el: chips[k], show: true, pct: pct(limits[k]), text: trim(limits[k]) })));
      top.style.height = Math.max(1, topRows) * 26 + 'px';

      const hasObs = finite(live.obsMin) && finite(live.obsMax);
      band.style.display = hasObs ? '' : 'none';
      if (hasObs) {
        band.style.left = pct(live.obsMin) + '%';
        band.style.width = Math.max(0, pct(live.obsMax) - pct(live.obsMin)) + '%';
        band.title = 'beobachtet ' + fmt(live.obsMin, dec) + ' … ' + fmt(live.obsMax, dec) + unit;
      }
      dot.style.display = finite(live.value) ? '' : 'none';
      if (finite(live.value)) dot.style.left = pct(live.value) + '%';
      const bottomRows = place(bottom, [
        { el: valueChip, show: finite(live.value), pct: finite(live.value) ? pct(live.value) : 0,
          text: finite(live.value) ? fmt(live.value, dec) + unit : '' }
      ]);
      bottom.style.height = bottomRows * 26 + 'px';
    }

    function setLimit(k, v) {
      limits = clampOrdered(limits, k, Number((Math.round(v / grid) * grid).toFixed(gridDec)), ch.oneSided);
      draw();
      onChange(Object.assign({}, limits), k);
    }

    KEYS.forEach(k => {
      const h = el('div', { class: 'threshold-handle', 'data-key': k, tabindex: '0', role: 'slider',
                            'aria-label': LABELS[k], 'aria-valuemin': String(range[0]), 'aria-valuemax': String(range[1]) });
      handles[k] = h;
      chips[k] = el('div', { class: 'ts-chip ts-chip-' + k });
      if (active.includes(k)) top.appendChild(chips[k]);
      box.appendChild(h);
      h.addEventListener('pointerdown', e => {
        e.preventDefault();
        if (h.setPointerCapture) h.setPointerCapture(e.pointerId);
        h.classList.add('dragging');
        const move = ev => {
          const r = bar.getBoundingClientRect();
          setLimit(k, range[0] + Math.max(0, Math.min(1, (ev.clientX - r.left) / r.width)) * (range[1] - range[0]));
        };
        const up = () => {
          h.classList.remove('dragging');
          h.removeEventListener('pointermove', move);
          h.removeEventListener('pointerup', up);
          h.removeEventListener('pointercancel', up);
        };
        h.addEventListener('pointermove', move);
        h.addEventListener('pointerup', up);
        h.addEventListener('pointercancel', up);
      });
      h.addEventListener('keydown', e => {
        const s = grid * (e.shiftKey ? 10 : 1);
        if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') setLimit(k, limits[k] - s);
        else if (e.key === 'ArrowRight' || e.key === 'ArrowUp') setLimit(k, limits[k] + s);
        else return;
        e.preventDefault();
      });
    });

    const element = el('div', { class: 'threshold-slider-container' }, [top, box, axis, bottom]);
    draw();
    if (typeof global.addEventListener === 'function') global.addEventListener('resize', draw);
    if (typeof global.requestAnimationFrame === 'function') global.requestAnimationFrame(draw);
    return {
      element,
      set(newLimits) {
        limits = Object.assign({}, newLimits);
        draw();
      },
      /** aktueller Wert und beobachtete Extrema aus dem Abruf alle 5 s */
      update(c) {
        live = { value: c.value, obsMin: c.obsMin, obsMax: c.obsMax };
        draw();
      }
    };
  }

  global.ThresholdSlider = { KEYS, calculateRange, clampOrdered, gradient, ticks, stackRows, create };
})(typeof window !== 'undefined' ? window : globalThis);
