// Baby Shaker V1 - Web Bluetooth controller.
// Wire format must match firmware/src/types.h (little-endian, packed).
'use strict';

const SVC         = '8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_MODE    = '8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_POS     = '8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_AUTO    = '8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const CHR_STATUS  = '8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a';
const NAME_PREFIX = 'Baby Shaker';
const MODE_NAMES  = ['Off', 'Manual', 'Auto'];

const $ = id => document.getElementById(id);
const ui = {
  connState: $('connState'), connectBtn: $('connectBtn'), deviceSelect: $('deviceSelect'), hint: $('hint'),
  statusCard: $('statusCard'), manualCard: $('manualCard'), autoCard: $('autoCard'),
  stMode: $('stMode'), stVbat: $('stVbat'), stPos: $('stPos'), stRemain: $('stRemain'),
  pos: $('pos'), posVal: $('posVal'), profile: $('profile'), durationRemain: $('durationRemain'),
  modeBtns: [...document.querySelectorAll('.modes button')],
};
const AUTO_SLIDERS = ['speed', 'accel', 'travel', 'hold', 'duration'];

let device = null, chr = {}, mode = -1, userDisconnect = false, draggingPos = false;

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
  return b.buffer;
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
  gatt(() => chr.mode.writeValueWithResponse(new Uint8Array([m]))).catch(e => setHint('Mode change failed: ' + e.message));
}

// ------------------------------------------------------------ UI
function setHint(t) { ui.hint.textContent = t || ''; }

function setConn(state) {   // 'off' | 'busy' | 'on'
  ui.connState.textContent = { off: 'Disconnected', busy: 'Connecting…', on: device ? device.name : 'Connected' }[state];
  ui.connState.className = 'pill' + (state === 'on' ? ' on' : state === 'busy' ? ' busy' : '');
  ui.connectBtn.textContent = state === 'on' ? 'Disconnect' : 'Connect';
  ui.connectBtn.disabled = state === 'busy';
  ui.statusCard.hidden = state !== 'on';
  if (state !== 'on') { ui.manualCard.hidden = ui.autoCard.hidden = true; mode = -1; }
}

function fmtTime(s) {
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

function updateLabel(id) {
  const v = +$(id).value;
  $(id + 'Val').textContent = id === 'hold' ? v.toFixed(1) : id === 'duration' && v === 0 ? '0 (no limit)' : v;
}

function showMode(m) {
  if (m === mode) return;
  mode = m;
  ui.stMode.textContent = MODE_NAMES[m] ?? '?';
  ui.modeBtns.forEach(b => b.classList.toggle('active', +b.dataset.mode === m));
  ui.manualCard.hidden = m !== 1;
  ui.autoCard.hidden = m !== 2;
}

function onStatus(dv) {
  const m = dv.getUint8(0), fb = dv.getUint8(1) & 1, vbat = dv.getUint16(2, true);
  const pos = dv.getUint16(4, true) / 10, target = dv.getUint16(6, true) / 10, rem = dv.getUint16(8, true);
  showMode(m);
  ui.stVbat.textContent = vbat < 500 ? 'N/A' : (vbat / 1000).toFixed(2) + ' V';
  ui.stPos.textContent = pos.toFixed(0) + '°' + (fb ? '' : '*');
  ui.stPos.title = fb ? 'Measured' : 'Commanded (no feedback)';
  if (m === 2) ui.stRemain.textContent = rem === 0xFFFF ? '∞' : fmtTime(rem);
  else if (m === 1) ui.stRemain.textContent = fmtTime(rem);
  else ui.stRemain.textContent = '—';
  ui.durationRemain.textContent = m === 2 && rem !== 0xFFFF ? `(${fmtTime(rem)} left)` : '';
  if (m === 1 && !draggingPos && posPending === null && !posBusy) {
    ui.pos.value = target;
    ui.posVal.textContent = target.toFixed(0);
  }
}

// ------------------------------------------------------------ connection
async function connect(dev) {
  device = dev;
  userDisconnect = false;
  setConn('busy');
  setHint('');
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
    decodeAuto(await chr.auto.readValue());
    chr.status.addEventListener('characteristicvaluechanged', e => onStatus(e.target.value));
    await chr.status.startNotifications();
    onStatus(await chr.status.readValue());
    setConn('on');
  } catch (e) {
    console.error(e);
    chr = {};
    setConn('off');
    setHint(`Could not connect to ${dev.name || 'device'}: ${e.message}`);
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
    setHint(`Connection lost - reconnecting (${i + 1}/6)…`);
    try { await connect(device); setHint(''); return; } catch {}
    await new Promise(r => setTimeout(r, 4000));
  }
  setHint('Connection lost. Tap Connect.');
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
  sel.add(new Option('Scan for devices…', 'scan'));
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
    if (e.name !== 'NotFoundError') setHint(e.message);   // NotFoundError = chooser cancelled
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

AUTO_SLIDERS.forEach(id => $(id).addEventListener('input', () => { updateLabel(id); sendAutoSoon(); }));
ui.profile.addEventListener('change', sendAutoSoon);

// ------------------------------------------------------------ start-up
async function init() {
  setConn('off');
  if (!navigator.bluetooth) {
    ui.connectBtn.disabled = true;
    const ios = /iPhone|iPad|iPod/.test(navigator.userAgent);
    setHint(ios
      ? 'Safari has no Bluetooth support. Open this page in the free "Bluefy" browser from the App Store.'
      : 'This browser has no Web Bluetooth. Use Chrome (Android / Windows / Mac) or Bluefy (iPhone).');
    return;
  }
  await refreshRemembered();
  if (remembered.length === 1) {
    // Exactly one known device: connect without the chooser.
    try { await connect(remembered[0]); return; } catch {}
    setHint(`${remembered[0].name} not reachable - make sure it is powered, then tap Connect.`);
  } else if (remembered.length === 0) {
    setHint('Tap Connect and pick your Baby Shaker.');
  } else {
    setHint('Select a device and tap Connect.');
  }
}

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
AUTO_SLIDERS.forEach(updateLabel);
init();
