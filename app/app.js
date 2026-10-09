// Baby Shaker V1 - Web Bluetooth controller.
// Wire format must match firmware/src/types.h (little-endian, packed).
'use strict';

const SVC         = '8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_MODE    = '8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_POS     = '8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_AUTO    = '8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_STATUS  = '8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_PRESETS = '8f1d0007-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const NAME_PREFIX = 'Baby Shaker';
const PRESET_MAX = 8, PRESET_NAME_LEN = 32, PRESET_SIZE = PRESET_NAME_LEN + 12;

// ------------------------------------------------------------ strings
const STR = {
  en: {
    langBtn: 'עברית',
    disconnected: 'Disconnected', connecting: 'Connecting…', connected: 'Connected',
    connect: 'Connect', disconnect: 'Disconnect',
    mode: 'Mode', battery: 'Battery', position: 'Position', remaining: 'Remaining',
    off: 'Off', manual: 'Manual', auto: 'Auto',
    posLabel: 'Position (degrees):', manualHint: 'Returns to Off after 1 minute without movement.',
    preset: 'Preset:', presetNone: '— choose —', save: 'Save', del: 'Delete',
    presetName: 'Preset name:', presetDelete: 'Delete preset "{0}"?', presetFull: 'Up to {0} presets - delete one first.',
    profile: 'Movement Profile:', trapezoidal: 'Trapezoidal', scurve: 'S-Curve', sinusoidal: 'Sinusoidal', cubical: 'Cubical',
    speed: 'Speed (degrees/sec):', accel: 'Acceleration (degrees/sec²):', travel: 'Travel (degrees):',
    hold: 'Hold Time (sec):', duration: 'Duration (minutes):',
    noLimit: '0 (no limit)', left: '({0} left)', na: 'N/A',
    measured: 'Measured', commanded: 'Commanded (no feedback)',
    scan: 'Scan for devices…',
    hintPick: 'Tap Connect and pick your Baby Shaker.', hintSelect: 'Select a device and tap Connect.',
    hintUnreachable: '{0} not reachable - make sure it is powered, then tap Connect.',
    hintFail: 'Could not connect to {0}: {1}', hintLost: 'Connection lost. Tap Connect.',
    hintReconnect: 'Connection lost - reconnecting ({0}/6)…', hintModeFail: 'Mode change failed: {0}',
    hintIos: 'Safari has no Bluetooth support. Open this page in the free "Bluefy" browser from the App Store.',
    hintNoBt: 'This browser has no Web Bluetooth. Use Chrome (Android / Windows / Mac) or Bluefy (iPhone).',
    credit: 'By Arad & Claud 2026 ©',
  },
  he: {
    langBtn: 'English',
    disconnected: 'מנותק', connecting: 'מתחבר…', connected: 'מחובר',
    connect: 'התחבר', disconnect: 'התנתק',
    mode: 'מצב', battery: 'סוללה', position: 'מיקום', remaining: 'זמן נותר',
    off: 'כבוי', manual: 'ידני', auto: 'אוטומטי',
    posLabel: 'מיקום (מעלות):', manualHint: 'חוזר למצב כבוי אחרי דקה ללא תזוזה.',
    preset: 'הגדרה שמורה:', presetNone: '— בחר —', save: 'שמור', del: 'מחק',
    presetName: 'שם ההגדרה:', presetDelete: 'למחוק את "{0}"?', presetFull: 'אפשר לשמור עד {0} הגדרות - מחק אחת קודם.',
    profile: 'פרופיל תנועה:', trapezoidal: 'טרפזי', scurve: 'עקומת S', sinusoidal: 'סינוסי', cubical: 'קובי',
    speed: 'מהירות (מעלות/שנייה):', accel: 'תאוצה (מעלות/שנייה²):', travel: 'טווח תנועה (מעלות):',
    hold: 'זמן המתנה (שניות):', duration: 'משך (דקות):',
    noLimit: '0 (ללא הגבלה)', left: '(נותרו {0})', na: 'לא זמין',
    measured: 'נמדד', commanded: 'לפי פקודה (אין משוב)',
    scan: 'חפש מכשירים…',
    hintPick: 'לחץ "התחבר" ובחר את ה-Baby Shaker שלך.', hintSelect: 'בחר מכשיר ולחץ "התחבר".',
    hintUnreachable: '{0} לא זמין - ודא שהוא דולק ולחץ "התחבר".',
    hintFail: 'החיבור ל-{0} נכשל: {1}', hintLost: 'החיבור נותק. לחץ "התחבר".',
    hintReconnect: 'החיבור נותק - מתחבר מחדש ({0}/6)…', hintModeFail: 'החלפת המצב נכשלה: {0}',
    hintIos: 'ל-Safari אין תמיכה ב-Bluetooth. פתח את הדף בדפדפן החינמי "Bluefy" מה-App Store.',
    hintNoBt: 'בדפדפן הזה אין Web Bluetooth. השתמש ב-Chrome (אנדרואיד / Windows / Mac) או ב-Bluefy (אייפון).',
    credit: 'מאת ארד וקלוד 2026 ©',
  },
};

function loadLang() {
  const q = new URLSearchParams(location.search).get('lang');   // ?lang=he for a shareable link
  if (STR[q]) return q;
  try { const l = localStorage.getItem('lang'); if (STR[l]) return l; } catch {}
  return (navigator.language || '').startsWith('he') ? 'he' : 'en';
}
let lang = loadLang();
const t = (key, ...args) => (STR[lang][key] ?? STR.en[key]).replace(/\{(\d)\}/g, (_, i) => args[i]);

// ------------------------------------------------------------ state
const $ = id => document.getElementById(id);
const ui = {
  connState: $('connState'), connectBtn: $('connectBtn'), deviceSelect: $('deviceSelect'), hint: $('hint'),
  statusCard: $('statusCard'), manualCard: $('manualCard'), autoCard: $('autoCard'),
  stMode: $('stMode'), stVbat: $('stVbat'), stPos: $('stPos'), stRemain: $('stRemain'),
  pos: $('pos'), posVal: $('posVal'), profile: $('profile'), durationRemain: $('durationRemain'),
  presetRow: $('presetRow'), presetSelect: $('presetSelect'), presetSave: $('presetSave'), presetDel: $('presetDel'),
  modeBtns: [...document.querySelectorAll('.modes button')],
};
const AUTO_SLIDERS = ['speed', 'accel', 'travel', 'hold', 'duration'];
const MODE_KEYS = ['off', 'manual', 'auto'];

let device = null, chr = {}, mode = -1, userDisconnect = false, draggingPos = false;
let connState = 'off', hint = null, lastStatus = null, presets = [];

// ------------------------------------------------------------ GATT plumbing
// Chrome rejects overlapping GATT operations, so everything goes through one chain.
let chain = Promise.resolve();
function gatt(fn) {
  const p = chain.then(fn);
  chain = p.catch(() => {});
  return p;
}

function encodeAuto() {
  const b = new DataView(new ArrayBuffer(12));
  b.setUint8(0, +ui.profile.value);
  b.setUint16(2, +$('speed').value, true);
  b.setUint16(4, +$('accel').value, true);
  b.setUint16(6, +$('travel').value, true);
  b.setUint16(8, Math.round(+$('hold').value * 10), true);
  b.setUint16(10, +$('duration').value, true);
  return new Uint8Array(b.buffer);
}

function decodeAuto(dv) {
  ui.profile.value = dv.getUint8(0);
  $('speed').value = dv.getUint16(2, true);
  $('accel').value = dv.getUint16(4, true);
  $('travel').value = dv.getUint16(6, true);
  $('hold').value = (dv.getUint16(8, true) / 10).toFixed(1);
  $('duration').value = dv.getUint16(10, true);
  AUTO_SLIDERS.forEach(updateLabel);
}

// Position: real-time, write-without-response, only the newest value matters.
let posBusy = false, posPending = null;
function sendPos(deg) {
  posPending = deg;
  if (posBusy || !chr.pos) return;
  posBusy = true;
  const v = posPending; posPending = null;
  const b = new DataView(new ArrayBuffer(2));
  b.setUint16(0, Math.round(v * 10), true);
  gatt(() => chr.pos.writeValueWithoutResponse(b.buffer))
    .catch(e => console.warn('pos write', e))
    .finally(() => {
      posBusy = false;
      if (posPending !== null) setTimeout(() => sendPos(posPending), 30);   // ~30 Hz
    });
}

let autoTimer = null;
function sendAutoSoon() {
  clearTimeout(autoTimer);
  autoTimer = setTimeout(() => {
    if (chr.auto) gatt(() => chr.auto.writeValueWithResponse(encodeAuto())).catch(e => console.warn('auto write', e));
  }, 150);
}

function sendMode(m) {
  if (!chr.mode) return;
  gatt(() => chr.mode.writeValueWithResponse(new Uint8Array([m])))
    .catch(e => setHint('hintModeFail', e.message));
}

// ------------------------------------------------------------ presets
// Characteristic value: u8 count, then count * (name[32] UTF-8 zero-padded, AutoParams[12]).
function parsePresets(dv) {
  const out = [], n = dv.byteLength ? dv.getUint8(0) : 0;
  for (let i = 0; i < n && 1 + (i + 1) * PRESET_SIZE <= dv.byteLength; i++) {
    const off = 1 + i * PRESET_SIZE;
    const raw = new Uint8Array(dv.buffer, dv.byteOffset + off, PRESET_NAME_LEN);
    const end = raw.indexOf(0);
    out.push({
      name: new TextDecoder().decode(raw.subarray(0, end < 0 ? raw.length : end)),
      params: new DataView(dv.buffer.slice(dv.byteOffset + off + PRESET_NAME_LEN, dv.byteOffset + off + PRESET_SIZE)),
    });
  }
  return out;
}

// UTF-8 name that fits 31 bytes without splitting a character.
function encodeName(name) {
  const enc = new TextEncoder();
  let s = name.trim();
  while (enc.encode(s).length > PRESET_NAME_LEN - 1) s = [...s].slice(0, -1).join('');
  const out = new Uint8Array(PRESET_NAME_LEN);
  out.set(enc.encode(s));
  return out;
}

function renderPresets(selectName) {
  const sel = ui.presetSelect, keep = selectName ?? sel.selectedOptions[0]?.dataset.name;
  sel.innerHTML = '';
  sel.add(new Option(t('presetNone'), ''));
  presets.forEach((p, i) => {
    const o = new Option(p.name, i);
    o.dataset.name = p.name;
    sel.add(o);
  });
  const idx = presets.findIndex(p => p.name === keep);
  sel.value = idx >= 0 ? idx : '';
  ui.presetDel.disabled = sel.value === '';
}

async function writePreset(bytes) {
  await gatt(() => chr.presets.writeValueWithResponse(bytes));
  presets = parsePresets(await gatt(() => chr.presets.readValue()));
}

ui.presetSelect.addEventListener('change', () => {
  const p = presets[+ui.presetSelect.value];
  ui.presetDel.disabled = !p;
  if (!p) return;
  decodeAuto(p.params);
  sendAutoSoon();
});

ui.presetSave.addEventListener('click', async () => {
  const current = presets[+ui.presetSelect.value];
  const name = (prompt(t('presetName'), current ? current.name : '') || '').trim();
  if (!name || !chr.presets) return;
  let idx = presets.findIndex(p => p.name === name);
  if (idx < 0 && presets.length >= PRESET_MAX) { alert(t('presetFull', PRESET_MAX)); return; }
  const buf = new Uint8Array(2 + PRESET_SIZE);
  buf.set([1, idx < 0 ? 0xFF : idx]);
  buf.set(encodeName(name), 2);
  buf.set(encodeAuto(), 2 + PRESET_NAME_LEN);
  try { await writePreset(buf); } catch (e) { console.warn('preset save', e); }
  renderPresets(name);
});

ui.presetDel.addEventListener('click', async () => {
  const idx = +ui.presetSelect.value, p = presets[idx];
  if (!p || !chr.presets || !confirm(t('presetDelete', p.name))) return;
  try { await writePreset(new Uint8Array([2, idx])); } catch (e) { console.warn('preset delete', e); }
  renderPresets('');
});

// ------------------------------------------------------------ UI
function setHint(key, ...args) {
  hint = key ? { key, args } : null;
  ui.hint.textContent = hint ? t(key, ...args) : '';
}

function setConn(state) {   // 'off' | 'busy' | 'on'
  connState = state;
  ui.connState.textContent = state === 'on' ? (device ? device.name : t('connected'))
                           : t(state === 'busy' ? 'connecting' : 'disconnected');
  ui.connState.className = 'pill' + (state === 'on' ? ' on' : state === 'busy' ? ' busy' : '');
  ui.connectBtn.textContent = t(state === 'on' ? 'disconnect' : 'connect');
  ui.connectBtn.classList.toggle('primary', state !== 'on');
  ui.connectBtn.disabled = state === 'busy' || !navigator.bluetooth;
  ui.statusCard.hidden = state !== 'on';
  if (state !== 'on') { ui.manualCard.hidden = ui.autoCard.hidden = true; mode = -1; lastStatus = null; }
}

function fmtTime(s) {
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

function updateLabel(id) {
  const v = +$(id).value;
  $(id + 'Val').textContent = id === 'hold' ? v.toFixed(1) : id === 'duration' && v === 0 ? t('noLimit') : v;
}

function showMode(m) {
  if (m === mode) return;
  mode = m;
  ui.stMode.textContent = MODE_KEYS[m] ? t(MODE_KEYS[m]) : '?';
  ui.modeBtns.forEach(b => b.classList.toggle('active', +b.dataset.mode === m));
  ui.manualCard.hidden = m !== 1;
  ui.autoCard.hidden = m !== 2;
}

function onStatus(dv) {
  lastStatus = dv;
  const m = dv.getUint8(0), fb = dv.getUint8(1) & 1, vbat = dv.getUint16(2, true);
  const pos = dv.getUint16(4, true) / 10, target = dv.getUint16(6, true) / 10, rem = dv.getUint16(8, true);
  showMode(m);
  ui.stVbat.textContent = vbat < 500 ? t('na') : (vbat / 1000).toFixed(2) + ' V';
  ui.stPos.textContent = pos.toFixed(0) + '°' + (fb ? '' : '*');
  ui.stPos.title = t(fb ? 'measured' : 'commanded');
  if (m === 2) ui.stRemain.textContent = rem === 0xFFFF ? '∞' : fmtTime(rem);
  else if (m === 1) ui.stRemain.textContent = fmtTime(rem);
  else ui.stRemain.textContent = '—';
  ui.durationRemain.textContent = m === 2 && rem !== 0xFFFF ? t('left', fmtTime(rem)) : '';
  if (m === 1 && !draggingPos && posPending === null && !posBusy) {
    ui.pos.value = target;
    ui.posVal.textContent = target.toFixed(0);
  }
}

function applyLang() {
  document.documentElement.lang = lang;
  document.documentElement.dir = lang === 'he' ? 'rtl' : 'ltr';
  document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
  setConn(connState);
  const m = mode; mode = -1;
  if (connState === 'on') showMode(m);
  if (lastStatus) onStatus(lastStatus);
  AUTO_SLIDERS.forEach(updateLabel);
  if (hint) setHint(hint.key, ...hint.args);
  renderPresets();
  const scan = ui.deviceSelect.querySelector('option[value=scan]');
  if (scan) scan.textContent = t('scan');
}

$('langBtn').addEventListener('click', () => {
  lang = lang === 'he' ? 'en' : 'he';
  try { localStorage.setItem('lang', lang); } catch {}
  applyLang();
});

// ------------------------------------------------------------ connection
async function connect(dev) {
  device = dev;
  userDisconnect = false;
  setConn('busy');
  setHint(null);
  dev.removeEventListener('gattserverdisconnected', onDisconnected);
  dev.addEventListener('gattserverdisconnected', onDisconnected);
  try {
    const server = await dev.gatt.connect();
    const svc = await server.getPrimaryService(SVC);
    chr = {
      mode: await svc.getCharacteristic(CHR_MODE),
      pos: await svc.getCharacteristic(CHR_POS),
      auto: await svc.getCharacteristic(CHR_AUTO),
      status: await svc.getCharacteristic(CHR_STATUS),
    };
    // Older firmware has no presets characteristic - just hide the row.
    try { chr.presets = await svc.getCharacteristic(CHR_PRESETS); } catch { chr.presets = null; }
    decodeAuto(await chr.auto.readValue());
    presets = chr.presets ? parsePresets(await chr.presets.readValue()) : [];
    ui.presetRow.hidden = !chr.presets;
    renderPresets('');
    if (chr.presets) {
      chr.presets.addEventListener('characteristicvaluechanged', e => { presets = parsePresets(e.target.value); renderPresets(); });
      await chr.presets.startNotifications();
    }
    chr.status.addEventListener('characteristicvaluechanged', e => onStatus(e.target.value));
    await chr.status.startNotifications();
    setConn('on');
    onStatus(await chr.status.readValue());
  } catch (e) {
    console.error(e);
    chr = {};
    setConn('off');
    setHint('hintFail', dev.name || 'device', e.message);
    try { dev.gatt.disconnect(); } catch {}
    throw e;
  }
}

async function onDisconnected() {
  chr = {};
  posBusy = false; posPending = null;
  setConn('off');
  if (userDisconnect || !device) return;
  // Dropped (out of range, device reset): try to come back for ~30 s.
  for (let i = 0; i < 6 && !userDisconnect; i++) {
    setHint('hintReconnect', i + 1);
    try { await connect(device); setHint(null); return; } catch {}
    await new Promise(r => setTimeout(r, 4000));
  }
  setHint('hintLost');
}

async function scanAndConnect() {
  const dev = await navigator.bluetooth.requestDevice({
    filters: [{ namePrefix: NAME_PREFIX }],
    optionalServices: [SVC],
  });
  await connect(dev);
  refreshRemembered();
}

// Devices this browser already has permission for (Chrome; not on every platform).
let remembered = [];
async function refreshRemembered() {
  if (!navigator.bluetooth.getDevices) return;
  try {
    remembered = (await navigator.bluetooth.getDevices()).filter(d => (d.name || '').startsWith(NAME_PREFIX));
  } catch { remembered = []; }
  const sel = ui.deviceSelect;
  sel.innerHTML = '';
  remembered.forEach((d, i) => sel.add(new Option(d.name, i)));
  sel.add(new Option(t('scan'), 'scan'));
  sel.hidden = remembered.length < 2;
}

ui.connectBtn.addEventListener('click', async () => {
  if (device && device.gatt.connected) {
    userDisconnect = true;
    device.gatt.disconnect();
    return;
  }
  try {
    const sel = ui.deviceSelect;
    if (!sel.hidden && sel.value !== 'scan') await connect(remembered[+sel.value]);
    else await scanAndConnect();
  } catch (e) {
    if (e.name !== 'NotFoundError') setHint('hintFail', NAME_PREFIX, e.message);   // NotFoundError = chooser cancelled
    setConn('off');
  }
});

// ------------------------------------------------------------ controls
ui.modeBtns.forEach(b => b.addEventListener('click', () => sendMode(+b.dataset.mode)));

ui.pos.addEventListener('input', () => {
  ui.posVal.textContent = ui.pos.value;
  sendPos(+ui.pos.value);
});
ui.pos.addEventListener('pointerdown', () => { draggingPos = true; });
window.addEventListener('pointerup', () => { draggingPos = false; });

// Editing a slider detaches the settings from the selected preset.
function detachPreset() { ui.presetSelect.value = ''; ui.presetDel.disabled = true; }
AUTO_SLIDERS.forEach(id => $(id).addEventListener('input', () => { updateLabel(id); detachPreset(); sendAutoSoon(); }));
ui.profile.addEventListener('change', () => { detachPreset(); sendAutoSoon(); });

// ------------------------------------------------------------ start-up
async function init() {
  if (!navigator.bluetooth) {
    setConn('off');
    setHint(/iPhone|iPad|iPod/.test(navigator.userAgent) ? 'hintIos' : 'hintNoBt');
    return;
  }
  await refreshRemembered();
  if (remembered.length === 1) {
    // Exactly one known device: connect without the chooser.
    try { await connect(remembered[0]); return; } catch {}
    setHint('hintUnreachable', remembered[0].name);
  } else {
    setHint(remembered.length ? 'hintSelect' : 'hintPick');
  }
}

// #demo, #demo-manual: render the connected UI with fake status (layout checks, no device).
function demo(m) {
  device = { name: 'Baby Shaker DEMO' };
  setConn('on');
  decodeAuto(new DataView(new Uint8Array([2, 0, 90, 0, 44, 1, 60, 0, 5, 0, 30, 0]).buffer));
  presets = [{ name: 'Default', params: new DataView(encodeAuto().buffer) }];
  renderPresets('Default');
  const s = new DataView(new ArrayBuffer(10));
  s.setUint8(0, m); s.setUint16(2, 5040, true); s.setUint16(4, 1123, true);
  s.setUint16(6, 1123, true); s.setUint16(8, m === 2 ? 1754 : 47, true);
  onStatus(s);
}

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
applyLang();
if (location.hash.startsWith('#demo')) demo(location.hash === '#demo-manual' ? 1 : 2);
else init();
