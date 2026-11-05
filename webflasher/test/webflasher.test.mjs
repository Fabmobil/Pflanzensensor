/**
 * Tests des Webflashers: gleiche Prüfregeln wie die Firmware, Voreinstellungen
 * wie die Board-Profile, Block wie core/provision.h.
 * Vektor neu schreiben: WRITE_VECTORS=1 node --test webflasher/test/
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..', '..');
const VECTORS = path.join(ROOT, 'firmware', 'test', 'vectors');

function load(...files) {
  const ctx = { TextEncoder, Uint8Array, Uint32Array, DataView, JSON, Object, Error, Math };
  ctx.globalThis = ctx;
  vm.createContext(ctx);
  for (const f of files) vm.runInContext(fs.readFileSync(path.join(HERE, '..', f), 'utf8'), ctx, { filename: f });
  return ctx;
}

test('Prüfregeln wie die Firmware (gemeinsame Vektoren)', () => {
  const { Hardware } = load('hardware.js');
  const v = JSON.parse(fs.readFileSync(path.join(VECTORS, 'hardware_check.json'), 'utf8'));
  assert.ok(v.cases.length >= 20);
  for (const c of v.cases) {
    const f = Object.assign(Hardware.emptyFeatures(), c.features);
    const p = Object.assign(Hardware.emptyPins(), c.pins);
    const r = Hardware.check(c.platform, f, p);
    assert.equal(r.problem, c.problem, c.name);
    assert.equal(r.pin, c.pin, c.name);
    assert.ok(Hardware.describe(r).length > 3);
  }
});

test('Voreinstellungen entsprechen den Board-Profilen der Firmware', () => {
  const { Hardware } = load('hardware.js');
  for (const [name, file] of [['pcb_v1', 'pcb_v1.h'], ['esp32_devkit', 'esp32_devkit.h']]) {
    const src = fs.readFileSync(path.join(ROOT, 'firmware', 'boards', file), 'utf8');
    const hw = Hardware.preset(name);
    const pins = [...src.matchAll(/p\.(\w+) = (-?\d+);/g)];
    assert.ok(pins.length >= 8, file);
    for (const [, k, v] of pins) assert.equal(hw.pins[k], Number(v), `${file}: Pin ${k}`);
    for (const [, k, v] of src.matchAll(/f\.(\w+) = ([^;]+);/g)) {
      let want = v.trim();
      if (want === 'true' || want === 'false') want = want === 'true';
      else if (/DhtType::Dht(\d+)/.test(want)) want = Number(/Dht(\d+)/.exec(want)[1]);
      else want = Number(want);
      assert.equal(hw.features[k], want, `${file}: ${k}`);
    }
    assert.equal(Hardware.check(hw.chip, hw.features, hw.pins).problem, 'ok', file);
    assert.equal(hw.chip, /Platform::Esp32/.test(src) ? 'ESP32' : 'ESP8266', file);
  }
});

test('Kanalnamen entsprechen dem Katalog der Firmware', () => {
  const { Hardware } = load('hardware.js');
  const src = fs.readFileSync(path.join(ROOT, 'firmware', 'src', 'core', 'sensors', 'catalog.cpp'), 'utf8');
  const tables = { ANALOG: 'kAnalog', DHT: 'kDht', DS18B20: 'kDs18b20', SDS011: 'kSds011', MHZ19: 'kMhz19',
    HX711: 'kHx711', BMP280: 'kBmp280', SERIAL_RECEIVER: 'kSerial' };
  assert.deepEqual(JSON.parse(JSON.stringify(Hardware.CHANNELS.map(c => c.id))), Object.keys(tables), 'Reihenfolge wie SensorType');
  for (const c of Hardware.CHANNELS) {
    const body = new RegExp('ChannelDef ' + tables[c.id] + '\\[\\d+\\] PS_PROGMEM = \\{([\\s\\S]*?)\\n\\};').exec(src);
    assert.ok(body, c.id);
    const names = [...body[1].matchAll(/\{"[^"]*", "([^"]*)"/g)].map(m => m[1]);
    assert.deepEqual(JSON.parse(JSON.stringify(c.names)), names, c.id);
  }
  const pcb = Hardware.preset('pcb_v1');
  assert.deepEqual(JSON.parse(JSON.stringify(Hardware.channels(pcb.features).map(c => c.key))), ['ANALOG_0', 'ANALOG_1', 'DHT_0', 'DHT_1']);
});

test('CRC-32 und Blockaufbau', () => {
  const { Provision, Hardware } = load('hardware.js', 'provision.js');
  assert.equal(Provision.crc32(new TextEncoder().encode('123456789')), 0xCBF43926);
  const hw = Hardware.preset('pcb_v1');
  const b = Provision.encode(hw, { deviceName: '', wifi: [null, { ssid: '' }, null], channelNames: {} });
  assert.equal(b.length, 4096);
  assert.equal(String.fromCharCode(...b.slice(0, 7)), 'PSPROV1');
  const len = new DataView(b.buffer).getUint32(8, true);
  const json = JSON.parse(new TextDecoder().decode(b.slice(16, 16 + len)));
  assert.equal(json.hardware.name, 'pcb_v1');
  assert.equal(json.settings, undefined, 'leere Einstellungen werden weggelassen');
  assert.equal(b[4095], 0xFF);
  assert.equal(Provision.OFFSET.ESP8266, 0x3FA000);
  assert.equal(Provision.OFFSET.ESP32, 0x3F0000);
});

test('Block für den C++-Test (firmware/test/vectors/provision_block.json)', () => {
  const { Provision, Hardware } = load('hardware.js', 'provision.js');
  const hw = Hardware.preset('pcb_v1');
  hw.name = 'eigene';
  hw.features.ledTraffic = false;
  hw.features.ds18b20 = true;
  hw.features.ds18b20Max = 3;
  hw.pins.oneWire = 14;
  hw.staticIp = { ip: '192.168.1.50', gateway: '192.168.1.1', subnet: '255.255.255.0' };
  assert.equal(Hardware.check('ESP8266', hw.features, hw.pins).problem, 'ok');
  const settings = { deviceName: 'Balkon Süd', wifi: [{ ssid: 'Garten', password: 'geheim12' }, null, { ssid: 'Schule', password: '' }],
    channelNames: { ANALOG_0: 'Licht Süd', DS18B20_2: 'Teich' } };
  const b = Provision.encode(hw, settings);
  const len = 16 + new DataView(b.buffer).getUint32(8, true);
  const hex = Buffer.from(b.slice(0, len)).toString('hex');
  const vec = { description: 'Vom Webflasher (webflasher/provision.js) erzeugter Block; test_hardware parst ihn.',
    hex, expect: { name: 'eigene', slots: 7, oneWire: 14, deviceName: 'Balkon Süd', wifi: ['Garten', '', 'Schule'],
      channelNames: settings.channelNames } };
  const file = path.join(VECTORS, 'provision_block.json');
  if (process.env.WRITE_VECTORS) fs.writeFileSync(file, JSON.stringify(vec, null, 1) + '\n');
  assert.deepEqual(JSON.parse(fs.readFileSync(file, 'utf8')), vec, 'Vektor veraltet: WRITE_VECTORS=1 setzen');
});

test('Manifest: Firmware-Teile absolut, Block an der Chip-Adresse; Einstellungen geprüft', () => {
  const ctx = load('hardware.js', 'provision.js', 'flasher.js');
  ctx.URL = URL;
  const { Flasher } = ctx;
  const rel = { version: '3.1.0', parts: { ESP8266: [{ path: 'firmware/v3.1.0/pflanzensensor-esp8266.bin', offset: 0 }],
    ESP32: [{ path: 'firmware/v3.1.0/a.bin', offset: 4096 }, { path: 'firmware/v3.1.0/b.bin', offset: 65536 }] } };
  const m = Flasher.manifest(rel, 'ESP8266', 'https://fabmobil.github.io/Pflanzensensor/', 'blob:x/1');
  assert.equal(m.builds.length, 1);
  assert.equal(m.builds[0].chipFamily, 'ESP8266');
  assert.equal(m.builds[0].parts[0].path, 'https://fabmobil.github.io/Pflanzensensor/firmware/v3.1.0/pflanzensensor-esp8266.bin');
  assert.deepEqual(JSON.parse(JSON.stringify(m.builds[0].parts[1])), { path: 'blob:x/1', offset: 0x3FA000 });
  assert.equal(m.new_install_prompt_erase, true);
  const only = Flasher.manifest(rel, 'ESP32', 'https://h/', null);
  assert.equal(only.builds[0].parts.length, 2, 'nur Firmware: kein Block');
  assert.throws(() => Flasher.manifest({ version: '1', parts: {} }, 'ESP32', 'https://h/', null));
  const clean = s => JSON.parse(JSON.stringify(Flasher.cleanSettings(s)));
  const fab = { ssid: Flasher.FABMOBIL_SSID, password: '' };
  assert.deepEqual(clean({ deviceName: '  ', wifi: [{ ssid: 'Netz', password: '' }, {}, fab], adminPassword: '' }),
    { wifi: [{ ssid: 'Netz', password: '' }, null, null] }, 'Slot 3 unverändert: nicht geschickt');
  assert.deepEqual(clean({ wifi: [{}, {}, fab] }), {}, 'nur Voreinstellungen: keine Einstellungen');
  assert.deepEqual(clean({ wifi: [{}, {}, { ssid: 'Anderes', password: 'geheim123' }] }),
    { wifi: [null, null, { ssid: 'Anderes', password: 'geheim123' }] });
  assert.deepEqual(clean({ channelNames: { ANALOG_0: ' Licht ', DHT_0: '  ' } }), { channelNames: { ANALOG_0: 'Licht' } });
  assert.match(Flasher.settingsProblem({ channelNames: { ANALOG_0: 'ÄÄÄÄÄÄÄÄÄÄÄÄÄ' } }), /24 Bytes/);
  assert.equal(Flasher.settingsProblem({ channelNames: { ANALOG_0: 'ÄÄÄÄÄÄÄÄÄÄÄÄ' } }), '');
  assert.match(Flasher.settingsProblem({ wifi: [{}, { ssid: '', password: 'geheim123' }] }), /WLAN 2: Name/);
  assert.equal(Flasher.settingsProblem({ adminPassword: 'ä' }), 'Adminpasswort: 1 bis 32 druckbare ASCII-Zeichen.');
  assert.match(Flasher.settingsProblem({ wifi: [{ ssid: 'a', password: 'kurz' }] }), /WLAN 1: Passwort mindestens 8/);
  assert.match(Flasher.settingsProblem({ staticIp: { ip: '192.168.1.300' } }), /keine IPv4/);
  assert.match(Flasher.settingsProblem({ staticIp: { ip: '192.168.1.30' } }), /Gateway/);
  assert.equal(Flasher.settingsProblem({ staticIp: { ip: '192.168.1.30', gateway: '192.168.1.1' } }), '');
});

const FIXTURES = path.join(HERE, 'fixtures');
const plain = x => JSON.parse(JSON.stringify(x));
const fixture = name => fs.readFileSync(path.join(FIXTURES, name), 'utf8');

test('Konfiguration übernehmen: config.json der Firmware 2.x', () => {
  const { ConfigImport, Hardware } = load('hardware.js', 'configimport.js');
  const r = ConfigImport.read(fixture('config_v2.json'));
  assert.equal(r.format, 'v2');
  assert.deepEqual(plain(r.form), {
    deviceName: 'Gemeinschaftskueche', adminPassword: 'Kuechenpasswort',
    wifi: [{ ssid: 'Kuechen-WLAN', password: 'kuechen-wlan-pw' }, null, null],
    channelNames: { DHT_0: 'Lufttemperatur', DHT_1: 'Luftfeuchte' }
  });
  const c = JSON.stringify(r.config);
  for (const secret of ['Kuechenpasswort', 'kuechen-wlan-pw', 'Kuechen-WLAN', 'Gemeinschaftskueche'])
    assert.ok(!c.includes(secret), secret + ' gehört ins Formular, nicht in die Datei');
  assert.equal(r.config.wifi, undefined);
  assert.equal(r.config.display.screen_dur, 3000, 'der Rest bleibt im Altformat (die Firmware rechnet um)');
  assert.equal(r.config.sensors[1].measurements[0].thresh.yl, -10);
  assert.equal(r.config.sensors[1].measurements[0].nm, undefined, 'Kanalnamen kommen aus dem Formular');
  assert.deepEqual(plain(r.sensors).sort(), ['ANALOG', 'DHT', 'MHZ19'], 'auch Sensoren aus flower_sens/sel_meas');
  assert.deepEqual(plain(ConfigImport.missingSensors(r, Hardware.preset('pcb_v1').features)), ['MHZ19']);
  assert.equal(JSON.parse(fixture('config_v2.json')).wifi.pwd1, 'kuechen-wlan-pw', 'Eingabe unverändert');
});

test('Konfiguration übernehmen: Export der Firmware 3', () => {
  const { ConfigImport, Hardware } = load('hardware.js', 'configimport.js');
  const r = ConfigImport.read(fixture('config_v3.json'));
  assert.equal(r.format, 'v3');
  assert.deepEqual(plain(r.form), {
    deviceName: 'Fensterbank', adminPassword: '', wifi: [null, null, null],
    channelNames: { ANALOG_0: 'Licht Fenster', ANALOG_1: 'Bodenfeuchte', DHT_0: 'Lufttemperatur', DHT_1: 'Luftfeuchte' }
  });
  assert.deepEqual(plain(r.wifiNames), ['Werkstatt'], 'ohne Passwort nicht ins Formular (wäre ein offenes Netz)');
  assert.equal(r.config.device.wifi, undefined);
  assert.equal(r.config.device.general.deviceName, undefined);
  assert.equal(r.config.device.display.screenDuration, 5);
  assert.equal(r.config.sensors.ANALOG.channels['1'].rawMin, 300, 'Rohgrenzen der Analogkanäle bleiben');
  assert.equal(r.config.sensors.DHT.channels['0'].rawMin, undefined, 'ohne Bedeutung für DHT: weg');
  assert.equal(r.config.sensors.DHT.scale, undefined, 'Waagenfaktor nur bei HX711');
  assert.equal(r.config.sensors.DHT.channels['0'].yellowLow, 5);
  assert.equal(r.config.templates.boot.subject, '🌱 Küche ist wach');
  assert.deepEqual(plain(ConfigImport.missingSensors(r, Hardware.preset('pcb_v1').features)), []);
});

test('Konfiguration übernehmen: Fehler und Grenzfälle', () => {
  const { ConfigImport } = load('configimport.js');
  assert.throws(() => ConfigImport.read('kein json'), /kein gültiges JSON/);
  assert.throws(() => ConfigImport.read('{"schema":2,"device":{}}'), /schema 2/);
  assert.throws(() => ConfigImport.read('{"foo":1}'), /keine Konfigurationsdatei/);
  assert.throws(() => ConfigImport.read('[1,2]'), /keine Konfigurationsdatei/);
  assert.equal(ConfigImport.detect({ general: {} }), 'v2');
  assert.equal(ConfigImport.detect({ schema: 1 }), 'v3');
  // Altformat ohne "measurements": Gruppe = eine Messung
  const old = ConfigImport.read('{"sensors":[{"id":"ANALOG","idx":1,"nm":"Topf"}]}');
  assert.deepEqual(plain(old.form.channelNames), { ANALOG_1: 'Topf' });
  assert.equal(ConfigImport.sensorOfKey('SERIAL_RECEIVER_3'), 'SERIAL_RECEIVER');
  assert.equal(ConfigImport.sensorOfKey('quatsch'), '');
});

test('Konfiguration im Block: zu groß → erst ohne Mailtexte, sonst gar nicht', () => {
  const { ConfigImport, Provision, Hardware, Flasher } = load('hardware.js', 'provision.js', 'flasher.js', 'configimport.js');
  const hw = Hardware.preset('pcb_v1');
  const small = { config: { schema: 1, device: {} } };
  assert.ok(Flasher.cleanSettings(small).config, 'cleanSettings reicht config durch');
  assert.equal(ConfigImport.encodeFitting(Provision, hw, small).droppedTemplates, false);
  const big = { config: { schema: 1, device: {}, templates: { boot: { subject: 'x', body: 'y'.repeat(3500) } } } };
  const fit = ConfigImport.encodeFitting(Provision, hw, big);
  assert.equal(fit.droppedTemplates, true);
  assert.ok(fit.bytes);
  assert.ok(big.config.templates, 'Eingabe unverändert');
  const tooBig = { config: { schema: 1, device: { x: 'y'.repeat(4000) } } };
  assert.equal(ConfigImport.encodeFitting(Provision, hw, tooBig).bytes, null);
});

test('Blöcke mit Konfiguration für den C++-Test (firmware/test/vectors/provision_config_blocks.json)', () => {
  const { ConfigImport, Provision, Hardware, Flasher } = load('hardware.js', 'provision.js', 'flasher.js', 'configimport.js');
  const cases = [];
  for (const [name, file] of [['v2', 'config_v2.json'], ['v3', 'config_v3.json']]) {
    const r = ConfigImport.read(fixture(file));
    const hw = Hardware.preset('pcb_v1');
    // wie app.js: Formular aus der Datei, Slot 3 bleibt auf der Fabmobil-Voreinstellung
    const wifi = r.form.wifi.map((w, i) => w || (i === 2 ? { ssid: Flasher.FABMOBIL_SSID, password: '' } : { ssid: '', password: '' }));
    const s = { deviceName: r.form.deviceName, adminPassword: r.form.adminPassword, wifi,
      channelNames: r.form.channelNames, config: r.config };
    assert.equal(Flasher.settingsProblem(s), '', name);
    const fit = ConfigImport.encodeFitting(Provision, hw, Flasher.cleanSettings(s));
    assert.ok(fit.bytes && !fit.droppedTemplates, name + ' passt in den Block');
    const len = 16 + new DataView(fit.bytes.buffer).getUint32(8, true);
    cases.push({ name, bytes: len, hex: Buffer.from(fit.bytes.slice(0, len)).toString('hex') });
  }
  const vec = { description: 'Vom Webflasher erzeugte Blöcke mit importierter Konfiguration (webflasher/test/fixtures); test_hardware wendet sie an.', cases };
  const file = path.join(VECTORS, 'provision_config_blocks.json');
  if (process.env.WRITE_VECTORS) fs.writeFileSync(file, JSON.stringify(vec, null, 1) + '\n');
  assert.deepEqual(JSON.parse(fs.readFileSync(file, 'utf8')), vec, 'Vektor veraltet: WRITE_VECTORS=1 setzen');
});
