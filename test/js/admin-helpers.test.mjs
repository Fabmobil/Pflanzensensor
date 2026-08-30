/**
 * Tests für die seitenübergreifenden Helfer aus data/js/admin.js.
 *
 * parseJsonResponse() lag früher doppelt vor - einmal hier, einmal in
 * admin_display.js. Weil die Seiten beide Dateien laden ({"admin",
 * "admin_display"}) und Component::endResponse() sie in Vektorreihenfolge
 * ausgibt, verdrängte die zweite, schwächere Fassung die hiesige für die
 * gesamte Seite, auch für Aufrufe aus admin.js selbst. Die Dublette ist
 * entfernt; diese Tests halten fest, was die verbliebene Fassung leisten muss,
 * damit sie nicht unbemerkt wieder verkümmert.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadAdmin, fakeResponse } from './helpers/load.mjs';

test('401 wird als Authentifizierungsfehler gemeldet und bricht ab', () => {
  const { sandbox } = loadAdmin();
  assert.throws(
    () => sandbox.parseJsonResponse(fakeResponse({ status: 401 })),
    /Unauthorized/
  );
});

test('sauberes JSON wird geparst', async () => {
  const { sandbox } = loadAdmin();
  const data = await sandbox.parseJsonResponse(
    fakeResponse({ body: '{"success":true,"wert":42}' })
  );
  assert.equal(data.success, true);
  assert.equal(data.wert, 42);
});

test('JSON ohne passenden Content-Type wird trotzdem geparst', async () => {
  // Der ESP schickt nicht überall application/json. Die Antwort deshalb zu
  // verwerfen, obwohl sie erkennbar JSON ist, wäre unnötig streng.
  const { sandbox } = loadAdmin();
  const data = await sandbox.parseJsonResponse(
    fakeResponse({ contentType: 'text/html', body: '{"success":true}' })
  );
  assert.equal(data.success, true);
});

test('HTML-Antwort landet mit ihrem Text im Fehler', async () => {
  // Diagnosehilfe: liefert das Gerät eine Fehlerseite statt JSON, soll deren
  // Text sichtbar sein und nicht hinter "unerwarteter Fehler" verschwinden.
  const { sandbox } = loadAdmin();
  await assert.rejects(
    () => sandbox.parseJsonResponse(
      fakeResponse({ contentType: 'text/html', body: '<h2>Fehler</h2>' })
    ),
    /<h2>Fehler<\/h2>/
  );
});

test('kaputtes JSON trotz passendem Content-Type wird als Fehler gemeldet', async () => {
  const { sandbox } = loadAdmin();
  await assert.rejects(
    () => sandbox.parseJsonResponse(fakeResponse({ body: '{kaputt' })),
    /JSON parse error/
  );
});

test('leerer Rumpf ohne Content-Type ergibt einen verständlichen Fehler', async () => {
  const { sandbox } = loadAdmin();
  await assert.rejects(
    () => sandbox.parseJsonResponse(fakeResponse({ contentType: '', body: '' })),
    /Ungültige Server-Antwort/
  );
});

test('showSuccessMessage und showErrorMessage legen ihr Element selbst an', () => {
  const { sandbox } = loadAdmin();
  sandbox.showSuccessMessage('Gespeichert');
  const el = sandbox.document.body.children.find((c) => c.id === 'ajax-message');
  assert.ok(el, 'Meldungselement muss angelegt werden, wenn die Seite keins hat');
  assert.equal(el.textContent, 'Gespeichert');
  assert.match(el.className, /ajax-message-success/);

  sandbox.showErrorMessage('Kaputt');
  assert.equal(el.textContent, 'Kaputt');
  assert.match(el.className, /ajax-message-error/);
});
