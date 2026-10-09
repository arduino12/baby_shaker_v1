// Baby Shaker V1 - Web Bluetooth controller.
// Wire format must match firmware/src/types.h (little-endian, packed).
'use strict';

const SVC         = '8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_MODE    = '8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_POS     = '8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_AUTO    = '8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_STATUS  = '8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_CMD     = '8f1d0006-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_STATS   = '8f1d0008-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const NAME_PREFIX = 'Baby Shaker';
const MODE_OFF = 0, MODE_MANUAL = 1, MODE_AUTO = 2;
const CMD_CALIBRATE = 1, CMD_SAVE_SLOT = 2, CMD_RESET_STATS = 3, CMD_SET_TIME = 4;
const LONG_PRESS_MS = 700;

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
    hintCalibrating: 'Calibrating the motor - it sweeps its full range (~20 s)…',
    calConfirm: 'Calibrate the motor now? It will sweep its full range for about 20 seconds - make sure nothing blocks the arm.',
    usage: 'Statistics', last24h: 'Last 24 hours', last30d: 'Last 30 days', allTime: 'All time',
    times: '{0} times', dur: '{0}h {1}m', usageHint: 'Long-press a box to reset it.',
    resetConfirm: 'Reset "{0}"?',
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
    hintCalibrating: 'מכייל את המנוע - הוא נע על כל הטווח (~20 שניות)…',
    calConfirm: 'לכייל את המנוע עכשיו? הוא ינוע על כל הטווח כ-20 שניות - ודאו ששום דבר לא חוסם את הזרוע.',
    usage: 'סטטיסטיקה', last24h: '24 שעות אחרונות', last30d: '30 ימים אחרונים', allTime: 'מאז ומתמיד',
    times: '{0} הפעלות', dur: '{0} ש׳ {1} ד׳', usageHint: 'לחיצה ארוכה על תיבה מאפסת אותה.',
    resetConfirm: 'לאפס את "{0}"?',
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
  stVbat: $('stVbat'), posTile: $('posTile'), usageCard: $('usageCard'), stPos: $('stPos'), stRemain: $('stRemain'),
  pos: $('pos'), posVal: $('posVal'), profile: $('profile'), durationRemain: $('durationRemain'),
  saveBtn: $('saveBtn'),
  modeBtns: [...document.querySelectorAll('.modes button')],
};
const AUTO_SLIDERS = ['speed', 'accel', 'travel', 'hold', 'duration'];

let device = null, chr = {}, userDisconnect = false, draggingPos = false;
let connState = 'off', hint = null, lastStatus = null;
let mode = -1, slot = -1;          // as last reported by the device
let dirty = false;                 // sliders edited since the slot was loaded/saved
let lastStats = null;
// Optimistic mode: a pressed button shows at once; the device's status confirms
// it. Until then (max 1.5 s) older statuses must not flip the display back.
let localMode = null;              // {m, s, until}

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
function sendCmd(bytes) {
  if (!chr.cmd) return Promise.reject(new Error('not connected'));
  return gatt(() => chr.cmd.writeValueWithResponse(new Uint8Array(bytes)));
}

function sendTime() {
  const now = Math.floor(Date.now() / 1000);
  return sendCmd([CMD_SET_TIME, now & 255, (now >> 8) & 255, (now >> 16) & 255, (now >>> 24) & 255]);
}

function sendMode(m, s) {
  if (!chr.mode) return;
  localMode = { m, s, until: Date.now() + 1500 };
  showMode(m, s, false);
  const bytes = m === MODE_AUTO ? [m, s] : [m];
  gatt(() => chr.mode.writeValueWithResponse(new Uint8Array(bytes)))
    // The device loads the slot's set when it handles the write - read it a bit later.
    .then(() => { if (m === MODE_AUTO) setTimeout(() => loadAuto().catch(() => {}), 250); })
    .catch(e => { localMode = null; setHint('hintModeFail', e.message); });
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
  ui.statusCard.hidden = ui.usageCard.hidden = state !== 'on';
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

function showMode(m, s, fromDevice = true) {
  const changed = m !== mode || (m === MODE_AUTO && s !== slot);
  if (m === MODE_MANUAL && m !== mode) { posPending = null; draggingPos = false; }
  mode = m; slot = s;
  ui.modeBtns.forEach(b => b.classList.toggle('active',
    +b.dataset.mode === m && (m !== MODE_AUTO || +b.dataset.slot === s)));
  ui.manualCard.hidden = m !== MODE_MANUAL;
  ui.autoCard.hidden = m !== MODE_AUTO;
  ui.saveBtn.textContent = t('save', s + 1);
  // Switched to another Auto button elsewhere (another phone, serial): show its set.
  if (fromDevice && changed && m === MODE_AUTO && connState === 'on') loadAuto().catch(() => {});
}

function onStatus(dv) {
  lastStatus = dv;
  const m = dv.getUint8(0), fb = dv.getUint8(1) & 1, vbat = dv.getUint16(2, true);
  const pos = dv.getUint16(4, true) / 10, target = dv.getUint16(6, true) / 10, rem = dv.getUint16(8, true);
  const s = dv.byteLength > 10 ? dv.getUint8(10) : 0;
  const flags = dv.getUint8(1), stall = (flags & 2) !== 0, calibrating = (flags & 4) !== 0;
  const want = calibrating ? 'hintCalibrating' : stall ? 'hintStall' : null;
  if (want && hint?.key !== want) setHint(want);
  else if (!want && (hint?.key === 'hintStall' || hint?.key === 'hintCalibrating')) setHint(null);
  const pending = localMode && Date.now() < localMode.until;
  if (pending && (m !== localMode.m || (m === MODE_AUTO && s !== localMode.s))) {
    // A status from before the device handled our click: keep showing the click.
  } else {
    localMode = null;
    showMode(m, s);
  }
  ui.stVbat.textContent = vbat < 500 ? t('na') : (vbat / 1000).toFixed(2) + ' V';
  ui.stPos.textContent = pos.toFixed(0) + '°';
  ui.stPos.title = t(fb ? 'measured' : 'commanded');
  if (m === MODE_AUTO) ui.stRemain.textContent = rem === 0xFFFF ? '∞' : fmtTime(rem);
  else if (m === MODE_MANUAL) ui.stRemain.textContent = fmtTime(rem);
  else ui.stRemain.textContent = '—';
  ui.durationRemain.textContent = m === MODE_AUTO && rem !== 0xFFFF ? t('left', fmtTime(rem)) : '';
  if (mode === MODE_MANUAL && m === MODE_MANUAL && !draggingPos && posPending === null && !posBusy) {
    ui.pos.value = target;
    ui.posVal.textContent = target.toFixed(0);
  }
}

function fmtDuration(sec) {
  const h = Math.floor(sec / 3600), m = Math.floor(sec / 60) % 60;
  return t('dur', h, m);
}

// Stats value: 6 x u32 = 24h count, sec; 30d count, sec; all-time count, sec.
// 0xFFFFFFFF = unknown (the device has no time yet).
function onStats(dv) {
  lastStats = dv;
  for (let i = 0; i < 3; i++) {
    const count = dv.getUint32(i * 8, true), sec = dv.getUint32(i * 8 + 4, true);
    const known = count !== 0xFFFFFFFF;
    $('u' + i).textContent = known ? fmtDuration(sec) : '—';
    $('u' + i + 't').textContent = known ? t('times', count) : '';
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
  if (lastStats) onStats(lastStats);
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
    try { chr.stats = await svc.getCharacteristic(CHR_STATS); } catch { chr.stats = null; }
    decodeAuto(await chr.auto.readValue());
    setDirty(false);
    chr.status.addEventListener('characteristicvaluechanged', e => onStatus(e.target.value));
    await chr.status.startNotifications();
    await sendTime();   // the device has no clock; the stats windows need one
    if (chr.stats) {
      chr.stats.addEventListener('characteristicvaluechanged', e => onStats(e.target.value));
      await chr.stats.startNotifications();
      onStats(await chr.stats.readValue());
    }
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

// Long press (touch or mouse): fires after LONG_PRESS_MS of holding.
function onLongPress(el, fn) {
  let timer = null;
  const cancel = () => { clearTimeout(timer); timer = null; el.classList.remove('holding'); };
  el.addEventListener('pointerdown', () => {
    cancel();
    el.classList.add('holding');
    timer = setTimeout(() => { cancel(); fn(); }, LONG_PRESS_MS);
  });
  ['pointerup', 'pointerleave', 'pointercancel'].forEach(ev => el.addEventListener(ev, cancel));
  el.addEventListener('contextmenu', e => e.preventDefault());   // Android long-press menu
}

onLongPress(ui.posTile, () => {
  if (!confirm(t('calConfirm'))) return;
  setHint('hintCalibrating');
  sendCmd([CMD_CALIBRATE]).catch(e => setHint('hintFail', NAME_PREFIX, e.message));
});

document.querySelectorAll('[data-reset]').forEach(el => onLongPress(el, () => {
  const which = +el.dataset.reset, name = t(['last24h', 'last30d', 'allTime'][which]);
  if (confirm(t('resetConfirm', name))) sendCmd([CMD_RESET_STATS, which]).catch(e => console.warn('reset', e));
}));

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
  const u = new DataView(new ArrayBuffer(24));
  [3, 2460, 41, 52380, 128, 190620].forEach((v, i) => u.setUint32(i * 4, v, true));
  onStats(u);
  if (m === MODE_AUTO) setDirty(true);
}

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
applyLang();
if (location.hash.startsWith('#demo')) demo(location.hash === '#demo-manual' ? MODE_MANUAL : MODE_AUTO);
else init();
