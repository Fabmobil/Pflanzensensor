/**
 * devicewait.js - Warten, bis das Gerät nach einem Neustart wieder antwortet.
 *
 * Früher warteten alle Neustart-Abläufe blind eine feste Zeit ab und luden dann
 * neu. Der Reload feuerte regelmäßig zu früh und der Nutzer landete auf einer
 * Browser-Fehlerseite, ohne zu wissen, ob die Aktion funktioniert hat.
 *
 * Hier steckt die gemeinsame Logik: /status pollen, erkennen wann das Gerät
 * wirklich NEU gestartet ist (und nicht nur noch kurz auf die alte Instanz
 * antwortet), und währenddessen echten Fortschritt anzeigen.
 *
 * Die Datei definiert nur window.DeviceWait und fasst beim Laden kein DOM an -
 * sie kann deshalb auf jeder Seite eingebunden werden.
 */
(function (global) {
  'use strict';

  var STATUS_URL = '/status';

  /**
   * Einen /status-Abruf machen.
   * @returns {Promise<Object|null>} Statusobjekt, oder null bei jedem Fehler
   *          (Netzwerk, Timeout, Nicht-200, kaputtes JSON).
   */
  function getStatus(requestTimeoutMs) {
    var controller = new AbortController();
    var timer = setTimeout(function () { controller.abort(); },
                           requestTimeoutMs || 4000);

    return fetch(STATUS_URL, {
      signal: controller.signal,
      cache: 'no-cache',
      credentials: 'same-origin',
      headers: { 'Cache-Control': 'no-cache', 'Pragma': 'no-cache' }
    }).then(function (resp) {
      return resp.ok ? resp.json() : null;
    }).catch(function () {
      return null;
    }).then(function (json) {
      clearTimeout(timer);
      if (!json || typeof json !== 'object') return null;
      // Zeitstempel mitführen: die Uptime-Prüfung braucht den Abstand zum
      // Zeitpunkt der Messung, nicht zum Beginn des Wartens. Zwischen beidem
      // kann viel liegen - der auslösende Request selbst dauert beim
      // Dateisystem-Update knapp eine Minute.
      json._at = Date.now();
      return json;
    });
  }

  /**
   * Hat das Gerät seit dem Basiswert neu gestartet?
   *
   * Nicht das naheliegende `uptime < baseline.uptime`: das geht genau dann
   * schief, wenn zweimal kurz hintereinander neu gestartet wird. Basiswert
   * 12 s, das Gerät meldet sich nach 15 s Bootzeit zurück -> 15 < 12 ist
   * falsch, und die Oberfläche hinge bis zum Timeout, obwohl alles in Ordnung
   * ist. Verglichen wird deshalb gegen die Uptime, die das Gerät OHNE Neustart
   * jetzt haben müsste.
   */
  function looksRebooted(status, baseline, sawGone) {
    var now = Number(status && status.uptime);
    // baseline explizit prüfen: Number(null) ist 0 und isFinite(0) ist wahr -
    // ohne diese Prüfung liefe der Zweig unten mit Basiswert 0 los und meldete
    // nie einen Neustart. Das traf jeden Aufruf ohne Basiswert, also beide
    // OTA-Abschlüsse.
    if (baseline && isFinite(now) && isFinite(Number(baseline.uptime))) {
      // Seit der Basismessung vergangene Zeit - NICHT seit Beginn des Wartens.
      var sinceBaseline = (Date.now() - (baseline._at || Date.now())) / 1000;
      var expected = Number(baseline.uptime) + sinceBaseline;
      // 2 s Toleranz für Latenz und Sekundenraster. Klein gehalten, damit ein
      // schnell gebootetes Gerät nicht übersehen wird.
      // Die Drift allein beweist den Neustart - eine zusätzlich beobachtete
      // Lücke wird bewusst NICHT verlangt: das Gerät kann bereits während des
      // auslösenden Requests neu starten (beim Dateisystem-Update sichert
      // setUpdateFlags() vorher den Flash und antwortet ~60 s lang nicht),
      // und dann hätte das Pollen die Lücke nie zu sehen bekommen.
      return now < expected - 2;
    }
    // Ohne brauchbaren Basiswert bleibt nur "war weg und ist wieder da".
    // Hinweis: uptime ist millis()/1000 und läuft nach ~49,7 Tagen über. Das
    // sähe wie ein Neustart aus - Folge wäre ein überflüssiger Reload.
    return sawGone;
  }

  /**
   * Pollt /status, bis das Gerät nachweislich neu gestartet und bereit ist.
   *
   * @param {Object}   o
   * @param {Object}   o.baseline      Status VOR dem Neustart (für die Uptime-Prüfung)
   * @param {Function} o.until         (status) => bool, zusätzliche Bereitschaftsbedingung
   * @param {boolean}  o.requireReboot Muss ein Neustart nachgewiesen sein? Aus,
   *        wenn schon die Bedingung selbst den neuen Zustand beweist (etwa
   *        inUpdateMode beim Wechsel in den Update-Modus).
   * @param {Function} o.onProgress  ({phase, elapsedMs, timeoutMs, attempts, sawGone, status})
   * @returns {Promise<{ok:true,status:Object}|{ok:false,reason:'timeout'}>}
   *          Wirft nie und läuft nie endlos.
   */
  function waitForDevice(o) {
    o = o || {};
    var baseline = o.baseline || null;
    var until = o.until || function () { return true; };
    var requireReboot = o.requireReboot !== false;
    var timeoutMs = o.timeoutMs || 120000;
    var intervalMs = o.intervalMs || 1000;
    var requestTimeoutMs = o.requestTimeoutMs || 4000;
    var settleMs = o.settleMs === undefined ? 1500 : o.settleMs;
    var onProgress = o.onProgress || function () {};

    var started = Date.now();
    var deadline = started + timeoutMs;
    var sawGone = false;
    var attempts = 0;

    function report(phase, status) {
      try {
        onProgress({
          phase: phase,
          elapsedMs: Date.now() - started,
          timeoutMs: timeoutMs,
          attempts: attempts,
          sawGone: sawGone,
          status: status || null
        });
      } catch (e) {
        console.error('onProgress:', e);
      }
    }

    function sleep(ms) {
      return new Promise(function (r) { setTimeout(r, ms); });
    }

    // Bewusst KEIN Abbruch nach N Fehlern hintereinander (das tat der frühere
    // waitForUpdateMode). Während eines Neustarts scheitert jede einzelne
    // Anfrage - die Heuristik konnte nur zu früh und zu Unrecht greifen.
    // Einziges Abbruchkriterium ist die Frist.
    function step() {
      if (Date.now() >= deadline) {
        report('timeout');
        return Promise.resolve({ ok: false, reason: 'timeout' });
      }

      attempts++;
      return getStatus(requestTimeoutMs).then(function (status) {
        if (!status) {
          sawGone = true;
          report('unreachable');
          return sleep(intervalMs).then(step);
        }

        if (requireReboot && !looksRebooted(status, baseline, sawGone)) {
          // Antwortet noch - das ist die alte Instanz kurz vor dem Neustart.
          report('shutting-down', status);
          return sleep(intervalMs).then(step);
        }
        if (!until(status)) {
          // Neu gestartet, aber noch nicht fertig (z.B. weiter im Update-Modus).
          report('not-ready', status);
          return sleep(intervalMs).then(step);
        }

        report('ready', status);
        return { ok: true, status: status };
      });
    }

    report('settling');
    return sleep(settleMs).then(step);
  }

  /** Vollflächiges Overlay mit Titel, Meldung, Detailzeile und Schaltflächen. */
  function createOverlay(opts) {
    opts = opts || {};
    var overlay = document.createElement('div');
    overlay.className = 'devicewait-overlay';

    var box = document.createElement('div');
    box.className = 'devicewait-box';

    var title = document.createElement('h3');
    title.className = 'devicewait-title';
    title.textContent = opts.title || '';
    box.appendChild(title);

    var msg = document.createElement('p');
    msg.className = 'devicewait-message';
    msg.textContent = opts.message || '';
    box.appendChild(msg);

    var detail = document.createElement('div');
    detail.className = 'devicewait-detail';
    box.appendChild(detail);

    var actions = document.createElement('div');
    actions.className = 'devicewait-actions';
    box.appendChild(actions);

    overlay.appendChild(box);
    document.body.appendChild(overlay);

    return {
      setTitle: function (t) { title.textContent = t; },
      setMessage: function (m) { msg.textContent = m; },
      setDetail: function (d) { detail.textContent = d; },
      setActions: function (list) {
        actions.textContent = '';
        (list || []).forEach(function (a) {
          var b = document.createElement('button');
          b.type = 'button';
          b.className = a.className || 'button button-primary';
          b.textContent = a.label;
          b.addEventListener('click', a.onClick);
          actions.appendChild(b);
        });
      },
      close: function () {
        if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      }
    };
  }

  /** Fortschrittstext für eine Phase. */
  function phaseText(p) {
    var secs = Math.round(p.elapsedMs / 1000);
    switch (p.phase) {
      case 'settling':      return 'Anfrage wird gesendet...';
      case 'shutting-down': return 'Gerät fährt herunter...';
      case 'unreachable':   return 'Warte auf Neustart... (' + secs + ' s)';
      case 'not-ready':     return 'Gerät antwortet, Vorgang wird abgeschlossen...';
      case 'ready':         return 'Gerät ist wieder da.';
      default:              return '';
    }
  }

  /**
   * Kompletter Ablauf für die Admin-Seiten: Basiswert nehmen, Overlay zeigen,
   * Neustart auslösen, warten, neu laden.
   */
  function runReboot(o) {
    o = o || {};
    var overlay = createOverlay({ title: o.title || 'Gerät startet neu...',
                                  message: o.message || '' });
    var onSuccess = o.onSuccess || function () { global.location.reload(); };
    var timeoutMs = o.timeoutMs || 90000;

    function wait(baseline) {
      return waitForDevice({
        baseline: baseline,
        until: o.until,
        timeoutMs: timeoutMs,
        onProgress: function (p) { overlay.setDetail(phaseText(p)); }
      }).then(function (res) {
        if (res.ok) {
          overlay.setDetail('Gerät ist wieder da. Seite wird neu geladen...');
          onSuccess(res.status);
          return res;
        }
        showFailure(baseline);
        return res;
      });
    }

    function showFailure(baseline) {
      overlay.setTitle('Gerät nicht erreichbar');
      overlay.setMessage(
        'Das Gerät hat sich innerhalb von ' + Math.round(timeoutMs / 1000) +
        ' Sekunden nicht zurückgemeldet. Gesucht wurde unter ' +
        global.location.host + '.');
      overlay.setDetail(
        (o.failureHint ? o.failureHint + ' ' : '') +
        'Prüfe die auf dem Display angezeigte IP-Adresse - das Gerät kann eine ' +
        'neue Adresse bekommen haben. Kommt es nicht ins WLAN, öffnet es einen ' +
        'eigenen Zugangspunkt mit dem Gerätenamen, erreichbar unter ' +
        'http://192.168.4.1');
      overlay.setActions([
        { label: 'Erneut prüfen', onClick: function () {
            overlay.setTitle(o.title || 'Gerät startet neu...');
            overlay.setMessage(o.message || '');
            overlay.setActions([]);
            // Ohne erneuten POST - nur weiter warten.
            wait(baseline);
          } },
        { label: 'Trotzdem neu laden', className: 'button button-warning',
          onClick: function () { global.location.reload(); } }
      ]);
    }

    return getStatus().then(function (baseline) {
      return Promise.resolve()
        .then(o.trigger || function () {})
        .catch(function (e) {
          // Nicht fatal: das Gerät kann neu starten, bevor die Antwort
          // rausgeht. Genauso wird der /admin/config/update-POST behandelt.
          console.log('Auslöser meldete Fehler (Gerät startet vermutlich schon):', e);
        })
        .then(function () { return wait(baseline); });
    });
  }

  global.DeviceWait = {
    getStatus: getStatus,
    waitForDevice: waitForDevice,
    createOverlay: createOverlay,
    runReboot: runReboot,
    phaseText: phaseText
  };
})(window);
