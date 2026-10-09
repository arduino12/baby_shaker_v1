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
const CHR_MANUAL  = '8f1d0009-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_OTA     = '8f1d000a-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_INFO    = '8f1d000b-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const NAME_PREFIX = 'Baby Shaker';
const APP_VERSION = '1.5.1';   // keep in step with index.html (?v=) and sw.js
const MODE_OFF = 0, MODE_MANUAL = 1, MODE_AUTO = 2;
const CMD_CALIBRATE = 1, CMD_SAVE_SLOT = 2, CMD_RESET_STATS = 3, CMD_SET_TIME = 4;
const CMD_OTA_BEGIN = 5, CMD_OTA_END = 6, CMD_OTA_ABORT = 7, CMD_DROP_LINK = 8;
const OTA_READY = 1, OTA_DONE = 3, OTA_FAILED = 4;
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
    hintTapToChoose: '{0} did not answer - tap Connect to choose from the list.',
    hintPick: 'Tap Connect and pick your Baby Shaker.', hintSelect: 'Select a device and tap Connect.',
    hintUnreachable: '{0} not reachable - make sure it is powered, then tap Connect.',
    hintFail: 'Could not connect to {0}: {1}', hintLost: 'Connection lost. Tap Connect.',
    hintReconnect: 'Connection lost - reconnecting ({0}/6)…', hintModeFail: 'Mode change failed: {0}',
    hintSaveFail: 'Save failed: {0}',
    hintStall: 'Motor stalled and was stopped. Check that nothing blocks the arm.',
    hintCalibrating: 'Calibrating the motor - it sweeps its full range (~20 s)…',
    hintPending: 'Switching to {0} when the current stroke ends…',
    calConfirm: 'Calibrate the motor now? It will sweep its full range for about 20 seconds - make sure nothing blocks the arm.',
    usage: 'Statistics', last24h: 'Last 24 hours', last30d: 'Last 30 days', allTime: 'All time',
    times: '{0} times', dur: '{0}h {1}m', usageHint: 'Long-press a box to reset it.',
    resetConfirm: 'Reset "{0}"?',
    hintIos: 'Safari has no Bluetooth support. Open this page in the free "Bluefy" browser from the App Store.',
    hintNoBt: 'This browser has no Web Bluetooth. Use Chrome (Android / Windows / Mac) or Bluefy (iPhone).',
    credit: 'By Arad & Claud 2026 ©',
    versions: 'App {0} · Firmware {1}',
    fwTitle: 'Firmware update', fwAvailable: 'Version {0} is available (this device has {1}).',
    fwUpdate: 'Update firmware', fwConfirm: 'Update the firmware to {0}? The motor stops during the update (about a minute). Keep the phone close to the device.',
    fwDownload: 'Downloading…', fwProgress: 'Sending… {0}% ({1} KB/s)', fwVerify: 'Verifying…',
    fwDone: 'Updated - the device restarts and reconnects…', fwFail: 'Update failed: {0}',
    flagTip: 'To reconnect without choosing every time, open chrome://flags and enable "Experimental Web Platform features" and "Use the new permissions backend for Web Bluetooth".',
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
    hintTapToChoose: '{0} לא ענה - לחצו "התחברות" כדי לבחור מהרשימה.',
    hintPick: 'לחצו "התחברות" ובחרו את ה-Baby Shaker שלכם.', hintSelect: 'בחרו מכשיר ולחצו "התחברות".',
    hintUnreachable: '{0} לא זמין - ודאו שהוא דולק ולחצו "התחברות".',
    hintFail: 'החיבור ל-{0} נכשל: {1}', hintLost: 'החיבור נותק. לחצו "התחברות".',
    hintReconnect: 'החיבור נותק - מתחבר מחדש ({0}/6)…', hintModeFail: 'החלפת המצב נכשלה: {0}',
    hintSaveFail: 'השמירה נכשלה: {0}',
    hintIos: 'ל-Safari אין תמיכה ב-Bluetooth. פתחו את הדף בדפדפן החינמי "Bluefy" מה-App Store.',
    hintNoBt: 'בדפדפן הזה אין Web Bluetooth. השתמשו ב-Chrome (אנדרואיד / Windows / Mac) או ב-Bluefy (אייפון).',
    hintStall: 'המנוע נתקע ונעצר. ודאו ששום דבר לא חוסם את הזרוע.',
    hintCalibrating: 'מכייל את המנוע - הוא נע על כל הטווח (~20 שניות)…',
    hintPending: 'עובר ל{0} בסוף התנועה הנוכחית…',
    calConfirm: 'לכייל את המנוע עכשיו? הוא ינוע על כל הטווח כ-20 שניות - ודאו ששום דבר לא חוסם את הזרוע.',
    usage: 'סטטיסטיקה', last24h: '24 שעות אחרונות', last30d: '30 ימים אחרונים', allTime: 'מאז ומתמיד',
    times: '{0} הפעלות', dur: '{0} ש׳ {1} ד׳', usageHint: 'לחיצה ארוכה על תיבה מאפסת אותה.',
    resetConfirm: 'לאפס את "{0}"?',
    credit: 'מאת ארד וקלוד 2026 ©',
    versions: 'אפליקציה {0} · קושחה {1}',
    fwTitle: 'עדכון קושחה', fwAvailable: 'גרסה {0} זמינה (במכשיר גרסה {1}).',
    fwUpdate: 'עדכון קושחה', fwConfirm: 'לעדכן את הקושחה לגרסה {0}? המנוע ייעצר במהלך העדכון (כדקה). השאירו את הטלפון קרוב למכשיר.',
    fwDownload: 'מוריד…', fwProgress: 'שולח… {0}% ({1} KB/s)', fwVerify: 'מאמת…',
    fwDone: 'עודכן - המכשיר מופעל מחדש ומתחבר שוב…', fwFail: 'העדכון נכשל: {0}',
    flagTip: 'כדי להתחבר בלי לבחור בכל פעם: פתחו ⁨chrome://flags⁩ והפעילו את ⁨"Experimental Web Platform features"⁩ ואת ⁨"Use the new permissions backend for Web Bluetooth"⁩.',
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
const MANUAL_SLIDERS = ['mSpeed', 'mAccel'];

let device = null, chr = {}, userDisconnect = false, draggingPos = false;
let connState = 'off', hint = null, lastStatus = null;
let mode = -1, slot = -1;          // as last reported by the device
let dirty = false;                 // sliders edited since the slot was loaded/saved
let lastStats = null;
let pending = null;                // {m, s}: requested, waiting for the stroke to end (device-reported)
let fwVersion = null, siteFw = null, otaLast = null, otaBusy = false;

// ------------------------------------------------------------ GATT plumbing
// Chrome rejects overlapping GATT operations, so everything goes through one
// chain. Each operation gets a deadline: one that never settles (link
// trouble) would otherwise hold up every later write until a page reload.
const GATT_TIMEOUT_MS = 5000;
let chain = Promise.resolve();
function gatt(fn) {
  const p = chain.then(() => Promise.race([
    fn(),
    new Promise((_, reject) => setTimeout(() => reject(new Error('Bluetooth operation timed out')), GATT_TIMEOUT_MS)),
  ]));
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

// The buttons show only what the device reports (status notification), never
// a guess. Without-response: no round trip before the device acts on it.
function sendMode(m, s) {
  if (!chr.mode || otaBusy) return;
  const bytes = m === MODE_AUTO ? [m, s] : [m];
  gatt(() => chr.mode.writeValueWithoutResponse(new Uint8Array(bytes)))
    .catch(e => setHint('hintModeFail', e.message));
}

function encodeManual() {
  const b = new DataView(new ArrayBuffer(6));
  b.setUint8(0, +$('mProfile').value);
  b.setUint16(2, +$('mSpeed').value, true);
  b.setUint16(4, +$('mAccel').value, true);
  return new Uint8Array(b.buffer);
}

function decodeManual(dv) {
  $('mProfile').value = dv.getUint8(0);
  $('mSpeed').value = dv.getUint16(2, true);
  $('mAccel').value = dv.getUint16(4, true);
  MANUAL_SLIDERS.forEach(updateLabel);
}

let manualTimer = null;
function sendManualSoon() {
  clearTimeout(manualTimer);
  manualTimer = setTimeout(() => {
    if (chr.manual) gatt(() => chr.manual.writeValueWithResponse(encodeManual())).catch(e => console.warn('manual write', e));
  }, 150);
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

const MODE_KEYS = ['off', 'manual', 'auto'];

function fmtTime(s) {
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

function updateLabel(id) {
  const v = +$(id).value;
  $(id + 'Val').textContent = id === 'hold' ? v.toFixed(1) : id === 'duration' && v === 0 ? t('noLimit') : v;
}

function showMode(m, s) {
  const changed = m !== mode || (m === MODE_AUTO && s !== slot);
  if (m === MODE_MANUAL && m !== mode) { posPending = null; draggingPos = false; }
  mode = m; slot = s;
  const is = (b, mm, ss) => +b.dataset.mode === mm && (mm !== MODE_AUTO || +b.dataset.slot === ss);
  ui.modeBtns.forEach(b => {
    b.classList.toggle('active', is(b, m, s));
    b.classList.toggle('pending', !!pending && is(b, pending.m, pending.s));
  });
  ui.manualCard.hidden = m !== MODE_MANUAL;
  ui.autoCard.hidden = m !== MODE_AUTO;
  ui.saveBtn.textContent = t('save', s + 1);
  // Now on another Auto button: show its saved set.
  if (changed && m === MODE_AUTO && connState === 'on') loadAuto().catch(() => {});
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
  const p = dv.byteLength > 11 ? dv.getUint8(11) : 0xFF;
  pending = p === 0xFF ? null : { m: p & 15, s: p >> 4 };
  showMode(m, s);
  const pendingName = pending && t(pending.m === MODE_AUTO ? 'auto' : MODE_KEYS[pending.m], pending.s + 1);
  if (!want && pending) setHint('hintPending', pendingName);
  else if (!want && !pending && hint?.key === 'hintPending') setHint(null);
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
  MANUAL_SLIDERS.forEach(updateLabel);
  if (hint) setHint(hint.key, ...hint.args);
  ui.saveBtn.textContent = t('save', Math.max(slot, 0) + 1);
  showVersions();
  showFirmwareCard();
  const scan = ui.deviceSelect.querySelector('option[value=scan]');
  if (scan) scan.textContent = t('scan');
}

$('langBtn').addEventListener('click', () => {
  lang = lang === 'he' ? 'en' : 'he';
  try { localStorage.setItem('lang', lang); } catch {}
  applyLang();
});

// ------------------------------------------------------------ connection
// Chrome hands back the SAME characteristic objects after a reconnect, so a
// listener added on every connect piles up (each status was handled once per
// reconnect so far). Named handlers + remove-before-add keep exactly one.
const onStatusEvt = e => onStatus(e.target.value);
const onStatsEvt = e => onStats(e.target.value);
const onOtaEvt = e => { const r = parseOta(e.target.value); if (r) otaLast = r; };
function listen(c, fn) {
  c.removeEventListener('characteristicvaluechanged', fn);
  c.addEventListener('characteristicvaluechanged', fn);
}

// quiet: a retry attempt - on failure keep the link attempt open and the UI
// as is (connectWithin decides when to give up).
async function connect(dev, quiet = false) {
  device = dev;
  userDisconnect = false;
  setConn('busy');
  if (!quiet) setHint(null);
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
    try { chr.manual = await svc.getCharacteristic(CHR_MANUAL); } catch { chr.manual = null; }
    try { chr.ota = await svc.getCharacteristic(CHR_OTA); } catch { chr.ota = null; }
    try {
      fwVersion = new TextDecoder().decode(await (await svc.getCharacteristic(CHR_INFO)).readValue());
    } catch { fwVersion = null; }
    if (chr.ota) {
      listen(chr.ota, onOtaEvt);
      await chr.ota.startNotifications();
    }
    if (chr.manual) decodeManual(await chr.manual.readValue());
    $('manualMotion').hidden = !chr.manual;
    decodeAuto(await chr.auto.readValue());
    setDirty(false);
    listen(chr.status, onStatusEvt);
    await chr.status.startNotifications();
    await sendTime();   // the device has no clock; the stats windows need one
    if (chr.stats) {
      listen(chr.stats, onStatsEvt);
      await chr.stats.startNotifications();
      onStats(await chr.stats.readValue());
    }
    setConn('on');
    onStatus(await chr.status.readValue());
    try { localStorage.setItem('lastDevice', dev.name || ''); } catch {}
    showVersions();
    checkFirmware();
  } catch (e) {
    console.error(e);
    chr = {};
    if (quiet) throw e;
    setConn('off');
    setHint('hintFail', dev.name || NAME_PREFIX, e.message);
    try { dev.gatt.disconnect(); } catch {}
    throw e;
  }
}

async function onDisconnected() {
  chr = {};
  showFirmwareCard();
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

// The device this browser connected to last (Chrome remembers permission only
// with the flags in STR.flagTip; without them getDevices() doesn't exist).
function lastRemembered() {
  let name = '';
  try { name = localStorage.getItem('lastDevice') || ''; } catch {}
  return remembered.find(d => d.name === name) || (remembered.length === 1 ? remembered[0] : null);
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

// Page load: connect to the remembered device without the chooser - once it is
// heard advertising (after a reload Chrome doesn't know it is in range), and
// with time limits, so a device that is off doesn't keep the page "connecting".
async function connectRemembered(dev) {
  if (dev.watchAdvertisements) {
    const ac = new AbortController();
    try {
      const heard = new Promise(res => dev.addEventListener('advertisementreceived', () => res(true), { once: true }));
      await dev.watchAdvertisements({ signal: ac.signal });
      const ok = await Promise.race([heard, sleep(8000).then(() => false)]);
      ac.abort();
      if (!ok) return false;
    } catch { ac.abort(); }
  }
  try {
    await Promise.race([connect(dev), sleep(10000).then(() => { throw new Error('timeout'); })]);
    return true;
  } catch { return false; }
}

// Connect within `ms`. Attempts are capped at 1.5 s and retried without
// closing in between: right after a link closes, the OS may hand back the old,
// dying link once (measured on Windows: "GATT Server is disconnected").
async function connectWithin(dev, ms) {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    let timer;
    try {
      const cap = Math.min(1500, Math.max(0, deadline - Date.now()));
      await Promise.race([connect(dev, true), new Promise((_, rej) => { timer = setTimeout(() => rej(new Error('timeout')), cap); })]);
      return true;
    } catch {
      // try again
    } finally {
      clearTimeout(timer);
    }
    await sleep(150);
  }
  userDisconnect = true;   // our own cancel: no auto-reconnect loop
  try { dev.gatt.disconnect(); } catch {}
  setConn('off');
  return false;
}

// Disconnect by asking the board to close the link. When the phone/PC closes
// it instead, its stack keeps the old link for a few seconds and a quick
// reconnect fails (measured: ~4 s vs ~1.3 s). Falls back to closing it here.
async function disconnectNow() {
  userDisconnect = true;
  const dev = device;
  if (!dev || !dev.gatt.connected) return;
  const gone = new Promise(res => dev.addEventListener('gattserverdisconnected', () => res(true), { once: true }));
  try { await sendCmd([CMD_DROP_LINK]); } catch {}
  if (!await Promise.race([gone, sleep(1500).then(() => false)])) {
    try { dev.gatt.disconnect(); } catch {}
  }
}

let chooseNext = false;   // the last device didn't answer: the next tap opens the list

ui.connectBtn.addEventListener('click', async () => {
  if (device && device.gatt.connected) {
    ui.connectBtn.disabled = true;
    await disconnectNow();
    ui.connectBtn.disabled = false;
    return;
  }
  try {
    const sel = ui.deviceSelect;
    if (!sel.hidden && sel.value !== 'scan') {
      await connect(remembered[+sel.value]);
      return;
    }
    // Straight back to the device used last (the one just disconnected, or the
    // one Chrome remembers) - no list. The list is only the fallback: the
    // browser allows opening it for ~5 s after the tap, so try for 4 s.
    const last = device || lastRemembered();
    if (last && !chooseNext) {
      if (await connectWithin(last, 4000)) return;
      setConn('off');
      if (!(navigator.userActivation && navigator.userActivation.isActive)) {
        chooseNext = true;
        setHint('hintTapToChoose', last.name || NAME_PREFIX);
        return;
      }
    }
    chooseNext = false;
    await scanAndConnect();
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

AUTO_SLIDERS.forEach(id => $(id).addEventListener('input', () => { updateLabel(id); setDirty(true); sendAutoSoon(); }));
ui.profile.addEventListener('change', () => { setDirty(true); sendAutoSoon(); });
MANUAL_SLIDERS.forEach(id => $(id).addEventListener('input', () => { updateLabel(id); sendManualSoon(); }));
$('mProfile').addEventListener('change', sendManualSoon);

// Sliders move only when the drag starts on the knob - a swipe to scroll the
// page that starts on a track must not change anything. The range inputs get
// no pointer events (CSS); this drives them from their wrapper instead.
const THUMB_HIT_PX = 26;
function thumbX(input) {
  const r = input.getBoundingClientRect(), knob = 13;   // half the 26 px thumb
  const f = (input.value - input.min) / (input.max - input.min);
  const rtl = getComputedStyle(input).direction === 'rtl';
  const x = knob + f * (r.width - 2 * knob);
  return rtl ? r.right - x : r.left + x;
}
function valueAt(input, clientX) {
  const r = input.getBoundingClientRect(), knob = 13;
  let f = (clientX - r.left - knob) / (r.width - 2 * knob);
  if (getComputedStyle(input).direction === 'rtl') f = 1 - f;
  f = Math.min(1, Math.max(0, f));
  const step = +input.step || 1, min = +input.min;
  return min + Math.round(f * (input.max - min) / step) * step;
}
document.querySelectorAll('input[type=range]').forEach(input => {
  const host = input.parentElement;
  let active = null;
  host.addEventListener('pointerdown', e => {
    const r = input.getBoundingClientRect();
    if (Math.abs(e.clientX - thumbX(input)) > THUMB_HIT_PX || Math.abs(e.clientY - (r.top + r.height / 2)) > THUMB_HIT_PX) return;
    active = e.pointerId;
    host.setPointerCapture(e.pointerId);
    input.classList.add('dragging');
    if (input === ui.pos) draggingPos = true;
    e.preventDefault();
  });
  host.addEventListener('pointermove', e => {
    if (e.pointerId !== active) return;
    const v = valueAt(input, e.clientX);
    if (+input.value === v) return;
    input.value = v;
    input.dispatchEvent(new Event('input', { bubbles: true }));
  });
  const end = e => {
    if (e.pointerId !== active) return;
    active = null;
    input.classList.remove('dragging');
    if (input === ui.pos) draggingPos = false;
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };
  host.addEventListener('pointerup', end);
  host.addEventListener('pointercancel', end);
});

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
// Touch uses touch events: Android starts its own long-press gesture and sends
// pointercancel, which killed the pointer-event version on the first press.
function onLongPress(el, fn) {
  let timer = null, x0 = 0, y0 = 0;
  const cancel = () => { clearTimeout(timer); timer = null; el.classList.remove('holding'); };
  const start = (x, y) => {
    cancel();
    x0 = x; y0 = y;
    el.classList.add('holding');
    timer = setTimeout(() => { cancel(); fn(); }, LONG_PRESS_MS);
  };
  el.addEventListener('touchstart', e => start(e.touches[0].clientX, e.touches[0].clientY), { passive: true });
  el.addEventListener('touchmove', e => {
    const t0 = e.touches[0];
    if (Math.hypot(t0.clientX - x0, t0.clientY - y0) > 10) cancel();   // it's a scroll
  }, { passive: true });
  el.addEventListener('touchend', cancel);
  el.addEventListener('touchcancel', cancel);
  el.addEventListener('mousedown', e => start(e.clientX, e.clientY));
  el.addEventListener('mouseup', cancel);
  el.addEventListener('mouseleave', cancel);
  el.addEventListener('contextmenu', e => e.preventDefault());   // no long-press menu
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

// ------------------------------------------------------------ firmware update
function showVersions() {
  $('versions').textContent = t('versions', APP_VERSION, fwVersion || '—');
}

const newer = (a, b) => {   // is version a newer than b ("1.10.0" > "1.9.2")
  const x = a.split('.').map(Number), y = (b || '0').split('.').map(Number);
  for (let i = 0; i < 3; i++) if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) > (y[i] || 0);
  return false;
};

async function checkFirmware() {
  try {
    siteFw = await (await fetch('fw/version.json', { cache: 'no-store' })).json();
  } catch { siteFw = null; }
  showFirmwareCard();
}

function showFirmwareCard() {
  const show = otaBusy || (connState === 'on' && chr.ota && siteFw && fwVersion && newer(siteFw.version, fwVersion));
  $('fwCard').hidden = !show;
  if (show && !otaBusy) {
    $('fwText').textContent = t('fwAvailable', siteFw.version, fwVersion);
    $('fwBtn').textContent = t('fwUpdate');
  }
}

function parseOta(dv) {
  if (dv.byteLength !== 12) return null;
  return { state: dv.getUint8(0), err: dv.getUint8(1), mtu: dv.getUint16(2, true),
           size: dv.getUint32(4, true), rx: dv.getUint32(8, true) };
}

async function waitOta(pred, ms) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    if (otaLast && pred(otaLast)) return true;
    await new Promise(r => setTimeout(r, 20));
  }
  return false;
}

const u32 = v => [v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >>> 24) & 255];

// Image goes as chunks of [u32 offset][data], sized from the MTU the device
// reports. Up to 4 KB may be in flight; the device reports every 4 KB, and a
// lost chunk shows up as a report behind our offset - we resume from there.
async function updateFirmware() {
  if (otaBusy || !siteFw || !confirm(t('fwConfirm', siteFw.version))) return;
  otaBusy = true;
  const text = $('fwText'), bar = $('fwBar');
  $('fwBtn').hidden = true;
  bar.hidden = false;
  bar.value = 0;
  try {
    text.textContent = t('fwDownload');
    const img = new Uint8Array(await (await fetch(`fw/firmware.bin?v=${siteFw.version}`, { cache: 'no-store' })).arrayBuffer());
    const hash = [...new Uint8Array(await crypto.subtle.digest('SHA-256', img))].map(b => b.toString(16).padStart(2, '0')).join('');
    if (img.length !== siteFw.size || (siteFw.sha256 && hash !== siteFw.sha256)) throw new Error('download corrupted');

    otaLast = null;
    await sendCmd([CMD_OTA_BEGIN, ...u32(img.length)]);
    if (!await waitOta(r => r.state === OTA_READY || r.state === OTA_FAILED, 15000) || otaLast.state !== OTA_READY)
      throw new Error('device not ready' + (otaLast ? ` (${otaLast.err})` : ''));
    const chunk = Math.max(16, Math.min(otaLast.mtu - 7, 500));
    const t0 = Date.now();
    let off = 0;
    while (off < img.length) {
      for (let k = 0; k < 16 && off < img.length; k++) {
        const part = img.subarray(off, off + chunk);
        const buf = new Uint8Array(4 + part.length);
        buf.set(u32(off));
        buf.set(part, 4);
        await gatt(() => chr.ota.writeValueWithoutResponse(buf));
        off += part.length;
      }
      if (!await waitOta(r => r.rx >= off - 4096 || r.state === OTA_FAILED, 2000)) {
        const r = parseOta(await gatt(() => chr.ota.readValue()));
        if (r) { otaLast = r; if (r.rx < off) off = r.rx; }   // resume where the device is
      }
      if (otaLast && otaLast.state === OTA_FAILED) throw new Error(`device error ${otaLast.err}`);
      const kbs = off / 1024 / Math.max(0.1, (Date.now() - t0) / 1000);
      bar.value = off / img.length;
      text.textContent = t('fwProgress', Math.floor(100 * off / img.length), kbs.toFixed(1));
    }
    await waitOta(r => r.rx >= img.length || r.state === OTA_FAILED, 5000);
    text.textContent = t('fwVerify');
    await sendCmd([CMD_OTA_END]);
    if (!await waitOta(r => r.state === OTA_DONE || r.state === OTA_FAILED, 20000) || otaLast.state !== OTA_DONE)
      throw new Error(`verification failed (${otaLast ? otaLast.err : '?'})`);
    text.textContent = t('fwDone');   // the device restarts; onDisconnected reconnects
    fwVersion = null;
  } catch (e) {
    text.textContent = t('fwFail', e.message);
    sendCmd([CMD_OTA_ABORT]).catch(() => {});
    $('fwBtn').hidden = false;
  } finally {
    otaBusy = false;
    bar.hidden = true;
  }
}

$('fwBtn').addEventListener('click', updateFirmware);

// ------------------------------------------------------------ start-up
async function init() {
  if (!navigator.bluetooth) {
    setConn('off');
    setHint(/iPhone|iPad|iPod/.test(navigator.userAgent) ? 'hintIos' : 'hintNoBt');
    return;
  }
  await refreshRemembered();
  const last = lastRemembered();
  if (last) {
    // The device used last time: connect without the chooser.
    setConn('busy');
    if (await connectRemembered(last)) return;
    setConn('off');
    setHint('hintUnreachable', last.name);
  } else if (remembered.length) {
    setHint('hintSelect');
  } else {
    setHint('hintPick');
  }
  // Chrome without the flags can't remember devices: say how to turn them on.
  $('flagTip').hidden = !!navigator.bluetooth.getDevices || /iPhone|iPad|iPod/.test(navigator.userAgent);
}

// #demo, #demo-manual: render the connected UI with fake status (layout checks, no device).
function demo(m) {
  setConn('on');
  decodeAuto(new DataView(new Uint8Array([2, 0, 90, 0, 44, 1, 60, 0, 5, 0, 30, 0]).buffer));
  decodeManual(new DataView(new Uint8Array([2, 0, 90, 0, 44, 1]).buffer));
  const s = new DataView(new ArrayBuffer(12));
  s.setUint8(0, m); s.setUint16(2, 5040, true); s.setUint16(4, 1123, true);
  s.setUint16(6, 1123, true); s.setUint16(8, m === MODE_AUTO ? 1754 : 47, true); s.setUint8(10, 1);
  if (location.hash.endsWith('-pending')) s.setUint8(11, MODE_AUTO | 2 << 4); else s.setUint8(11, 0xFF);
  onStatus(s);
  const u = new DataView(new ArrayBuffer(24));
  [3, 2460, 41, 52380, 128, 190620].forEach((v, i) => u.setUint32(i * 4, v, true));
  onStats(u);
  if (m === MODE_AUTO) setDirty(true);
}

// updateViaCache 'none': the browser checks sw.js itself without its HTTP cache.
if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js', { updateViaCache: 'none' }).catch(() => {});
applyLang();
if (location.hash.startsWith('#demo')) demo(location.hash.startsWith('#demo-manual') ? MODE_MANUAL : MODE_AUTO);
else init();
