// Baby Shaker V1 - Web Bluetooth controller.
// Wire format must match firmware/src/types.h (little-endian, packed).
'use strict';

const SVC         = '8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_MODE    = '8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_POS     = '8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_AUTO    = '8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_STATUS  = '8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_CMD     = '8f1d0006-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const NAME_PREFIX = 'Baby Shaker';
const MODE_OFF = 0, MODE_MANUAL = 1, MODE_AUTO = 2, CMD_SAVE_SLOT = 2;

// ------------------------------------------------------------ strings
const STR = {
  en: {
    langBtn: 'עברית',
    connect: 'Connect', connecting: 'Connecting…', disconnect: 'Disconnect',
    mode: 'Mode', battery: 'Battery', position: 'Position', remaining: 'Remaining',
    off: 'Off', manual: 'Manual', auto: 'Auto {0}',
    posLabel: 'Position (degrees):', manualHint: 'Returns to Off after 1 minute without movement.',
    profile: 'Movement Profile:', trapezoidal: 'Trapezoidal', scurve: 'S-Curve', sinusoidal: 'Sinusoidal', cubical: 'Cubical',
    speed: 'Speed (degrees/sec):', accel: 'Acceleration (degrees/sec²):', travel: 'Travel (degrees):',
    hold: 'Hold Time (sec):', duration: 'Duration (minutes):',
    save: 'Save to Auto {0}', saved: 'Saved ✓',
    noLimit: '0 (no limit)', left: '({0} left)', na: 'N/A',
    measured: 'Measured', commanded: 'Commanded (no feedback)',
    scan: 'Scan for devices…',
    hintPick: 'Tap Connect and pick your Baby Shaker.', hintSelect: 'Select a device and tap Connect.',
    hintUnreachable: '{0} not reachable - make sure it is powered, then tap Connect.',
    hintFail: 'Could not connect to {0}: {1}', hintLost: 'Connection lost. Tap Connect.',
    hintReconnect: 'Connection lost - reconnecting ({0}/6)…', hintModeFail: 'Mode change failed: {0}',
    hintSaveFail: 'Save failed: {0}',
    hintStall: 'Motor stalled and was stopped. Check that nothing blocks the arm.',
    hintIos: 'Safari has no Bluetooth support. Open this page in the free "Bluefy" browser from the App Store.',
    hintNoBt: 'This browser has no Web Bluetooth. Use Chrome (Android / Windows / Mac) or Bluefy (iPhone).',
    credit: 'By Arad & Claud 2026 ©',
  },
  he: {
    langBtn: 'English',
    connect: 'התחברות', connecting: 'מתחבר…', disconnect: 'התנתקות',
    mode: 'מצב', battery: 'סוללה', position: 'מיקום', remaining: 'זמן נותר',
    off: 'כבוי', manual: 'ידני', auto: 'אוטומטי {0}',
    posLabel: 'מיקום (מעלות):', manualHint: 'חוזר למצב כבוי אחרי דקה ללא תזוזה.',
    profile: 'פרופיל תנועה:', trapezoidal: 'טרפזי', scurve: 'עקומת S', sinusoidal: 'סינוסי', cubical: 'קובייתי',
    speed: 'מהירות (מעלות/שנייה):', accel: 'תאוצה (מעלות/שנייה²):', travel: 'טווח תנועה (מעלות):',
    hold: 'זמן המתנה (שניות):', duration: 'משך (דקות):',
    save: 'שמירה לאוטומטי {0}', saved: 'נשמר ✓',
    noLimit: '0 (ללא הגבלה)', left: '(נותרו {0})', na: 'לא זמין',
    measured: 'נמדד', commanded: 'לפי פקודה (אין משוב)',
    scan: 'חפש מכשירים…',
    hintPick: 'לחצו "התחברות" ובחרו את ה-Baby Shaker שלכם.', hintSelect: 'בחרו מכשיר ולחצו "התחברות".',
    hintUnreachable: '{0} לא זמין - ודאו שהוא דולק ולחצו "התחברות".',
    hintFail: 'החיבור ל-{0} נכשל: {1}', hintLost: 'החיבור נותק. לחצו "התחברות".',
    hintReconnect: 'החיבור נותק - מתחבר מחדש ({0}/6)…', hintModeFail: 'החלפת המצב נכשלה: {0}',
    hintSaveFail: 'השמירה נכשלה: {0}',
    hintIos: 'ל-Safari אין תמיכה ב-Bluetooth. פתחו את הדף בדפדפן החינמי "Bluefy" מה-App Store.',
    hintNoBt: 'בדפדפן הזה אין Web Bluetooth. השתמשו ב-Chrome (אנדרואיד / Windows / Mac) או ב-Bluefy (אייפון).',
    hintStall: 'המנוע נתקע ונעצר. ודאו ששום דבר לא חוסם את הזרוע.',
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
  connectBtn: $('connectBtn'), deviceSelect: $('deviceSelect'), hint: $('hint'),
  statusCard: $('statusCard'), manualCard: $('manualCard'), autoCard: $('autoCard'),
  stMode: $('stMode'), stVbat: $('stVbat'), stPos: $('stPos'), stRemain: $('stRemain'),
  pos: $('pos'), posVal: $('posVal'), profile: $('profile'), durationRemain: $('durationRemain'),
  saveBtn: $('saveBtn'),
  modeBtns: [...document.querySelectorAll('.modes button')],
};
const AUTO_SLIDERS = ['speed', 'accel', 'travel', 'hold', 'duration'];

let device = null, chr = {}, userDisconnect = false, draggingPos = false;
let connState = 'off', hint = null, lastStatus = null;
let mode = -1, slot = -1;          // as last reported by the device
let dirty = false;                 // sliders edited since the slot was loaded/saved

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

async function loadAuto() {
  if (!chr.auto) return;
  decodeAuto(await gatt(() => chr.auto.readValue()));
  setDirty(false);
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

// Auto buttons send [MODE_AUTO, slot]; the device loads that slot's saved set.
function sendMode(m, s) {
  if (!chr.mode) return;
  const bytes = m === MODE_AUTO ? [m, s] : [m];
  gatt(() => chr.mode.writeValueWithResponse(new Uint8Array(bytes)))
    .then(() => { if (m === MODE_AUTO) return loadAuto(); })
    .catch(e => setHint('hintModeFail', e.message));
}

// ------------------------------------------------------------ UI
function setHint(key, ...args) {
  hint = key ? { key, args } : null;
  ui.hint.textContent = hint ? t(key, ...args) : '';
}

// Connect button doubles as the status: green = connect, orange = connecting, red = disconnect.
function setConn(state) {   // 'off' | 'busy' | 'on'
  connState = state;
  const btn = ui.connectBtn;
  btn.textContent = t({ off: 'connect', busy: 'connecting', on: 'disconnect' }[state]);
  btn.className = { off: 'go', busy: 'busy', on: 'stop' }[state];
  btn.disabled = state === 'busy' || !navigator.bluetooth;
  ui.statusCard.hidden = state !== 'on';
  if (state !== 'on') { ui.manualCard.hidden = ui.autoCard.hidden = true; mode = slot = -1; lastStatus = null; }
}

function setDirty(d) {
  dirty = d;
  ui.saveBtn.classList.toggle('primary', d);
}

function fmtTime(s) {
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

function updateLabel(id) {
  const v = +$(id).value;
  $(id + 'Val').textContent = id === 'hold' ? v.toFixed(1) : id === 'duration' && v === 0 ? t('noLimit') : v;
}

function modeLabel(m, s) {
  return m === MODE_AUTO ? t('auto', s + 1) : m === MODE_MANUAL ? t('manual') : m === MODE_OFF ? t('off') : '?';
}

function showMode(m, s) {
  const changed = m !== mode || (m === MODE_AUTO && s !== slot);
  if (m === MODE_MANUAL && m !== mode) { posPending = null; draggingPos = false; }
  mode = m; slot = s;
  ui.stMode.textContent = modeLabel(m, s);
  ui.modeBtns.forEach(b => b.classList.toggle('active',
    +b.dataset.mode === m && (m !== MODE_AUTO || +b.dataset.slot === s)));
  ui.manualCard.hidden = m !== MODE_MANUAL;
  ui.autoCard.hidden = m !== MODE_AUTO;
  ui.saveBtn.textContent = t('save', s + 1);
  // Switched to another Auto button (here or from another phone): show its set.
  if (changed && m === MODE_AUTO && connState === 'on') loadAuto().catch(() => {});
}

function onStatus(dv) {
  lastStatus = dv;
  const m = dv.getUint8(0), fb = dv.getUint8(1) & 1, vbat = dv.getUint16(2, true);
  const pos = dv.getUint16(4, true) / 10, target = dv.getUint16(6, true) / 10, rem = dv.getUint16(8, true);
  const s = dv.byteLength > 10 ? dv.getUint8(10) : 0;
  const stall = (dv.getUint8(1) & 2) !== 0;
  if (stall && hint?.key !== 'hintStall') setHint('hintStall');
  else if (!stall && hint?.key === 'hintStall') setHint(null);
  showMode(m, s);
  ui.stVbat.textContent = vbat < 500 ? t('na') : (vbat / 1000).toFixed(2) + ' V';
  ui.stPos.textContent = pos.toFixed(0) + '°' + (fb ? '' : '*');
  ui.stPos.title = t(fb ? 'measured' : 'commanded');
  if (m === MODE_AUTO) ui.stRemain.textContent = rem === 0xFFFF ? '∞' : fmtTime(rem);
  else if (m === MODE_MANUAL) ui.stRemain.textContent = fmtTime(rem);
  else ui.stRemain.textContent = '—';
  ui.durationRemain.textContent = m === MODE_AUTO && rem !== 0xFFFF ? t('left', fmtTime(rem)) : '';
  if (m === MODE_MANUAL && !draggingPos && posPending === null && !posBusy) {
    ui.pos.value = target;
    ui.posVal.textContent = target.toFixed(0);
  }
}

function applyLang() {
  document.documentElement.lang = lang;
  document.documentElement.dir = lang === 'he' ? 'rtl' : 'ltr';
  document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
  ui.modeBtns.filter(b => b.dataset.slot).forEach(b => { b.textContent = t('auto', +b.dataset.slot + 1); });
  setConn(connState);
  const st = lastStatus, m = mode, s = slot;
  if (connState === 'on' && m >= 0) showMode(m, s);
  if (st) onStatus(st);
  AUTO_SLIDERS.forEach(updateLabel);
  if (hint) setHint(hint.key, ...hint.args);
  ui.saveBtn.textContent = t('save', Math.max(slot, 0) + 1);
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
      cmd: await svc.getCharacteristic(CHR_CMD),
    };
    decodeAuto(await chr.auto.readValue());
    setDirty(false);
    chr.status.addEventListener('characteristicvaluechanged', e => onStatus(e.target.value));
    await chr.status.startNotifications();
    setConn('on');
    onStatus(await chr.status.readValue());
  } catch (e) {
    console.error(e);
    chr = {};
    setConn('off');
    setHint('hintFail', dev.name || NAME_PREFIX, e.message);
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

// Close the link when the page goes away (refresh, tab closed) instead of
// leaving it for the phone's Bluetooth stack to time out.
window.addEventListener('pagehide', () => {
  userDisconnect = true;
  try { if (device && device.gatt.connected) device.gatt.disconnect(); } catch {}
});

// ------------------------------------------------------------ controls
ui.modeBtns.forEach(b => b.addEventListener('click', () => sendMode(+b.dataset.mode, +(b.dataset.slot || 0))));

ui.pos.addEventListener('input', () => {
  ui.posVal.textContent = ui.pos.value;
  sendPos(+ui.pos.value);
});
ui.pos.addEventListener('pointerdown', () => { draggingPos = true; });
window.addEventListener('pointerup', () => { draggingPos = false; });

AUTO_SLIDERS.forEach(id => $(id).addEventListener('input', () => { updateLabel(id); setDirty(true); sendAutoSoon(); }));
ui.profile.addEventListener('change', () => { setDirty(true); sendAutoSoon(); });

ui.saveBtn.addEventListener('click', async () => {
  if (!chr.cmd || slot < 0) return;
  clearTimeout(autoTimer);
  try {
    // Push the latest slider values first, then store them in the active slot.
    await gatt(() => chr.auto.writeValueWithResponse(encodeAuto()));
    await gatt(() => chr.cmd.writeValueWithResponse(new Uint8Array([CMD_SAVE_SLOT, slot])));
    setDirty(false);
    ui.saveBtn.textContent = t('saved');
    setTimeout(() => { ui.saveBtn.textContent = t('save', slot + 1); }, 1500);
  } catch (e) {
    setHint('hintSaveFail', e.message);
  }
});

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
  setConn('on');
  decodeAuto(new DataView(new Uint8Array([2, 0, 90, 0, 44, 1, 60, 0, 5, 0, 30, 0]).buffer));
  const s = new DataView(new ArrayBuffer(11));
  s.setUint8(0, m); s.setUint16(2, 5040, true); s.setUint16(4, 1123, true);
  s.setUint16(6, 1123, true); s.setUint16(8, m === MODE_AUTO ? 1754 : 47, true); s.setUint8(10, 1);
  onStatus(s);
  if (m === MODE_AUTO) setDirty(true);
}

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
applyLang();
if (location.hash.startsWith('#demo')) demo(location.hash === '#demo-manual' ? MODE_MANUAL : MODE_AUTO);
else init();
