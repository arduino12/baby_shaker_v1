#include "ble_service.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "config.h"

namespace ble {

static char                 s_name[24];
static NimBLECharacteristic *s_modeChr, *s_autoChr, *s_statusChr, *s_statsChr, *s_manualChr, *s_otaChr;
static NimBLEServer         *s_server;
static volatile bool        s_connected = false;
static volatile uint16_t    s_conn = BLE_HS_CONN_HANDLE_NONE;   // newest link (MTU, OTA)
static Inbox                s_inbox;
static portMUX_TYPE         s_mux = portMUX_INITIALIZER_UNLOCKED;

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *srv, NimBLEConnInfo &info) override {
    s_connected = true;
    s_conn = info.getConnHandle();
    // Snappy slider response: 15-30 ms connection interval, 4 s supervision timeout.
    srv->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
    // Ask for a big ATT MTU from our side too: Chrome never asks, and OTA
    // chunks are limited to MTU - 3 bytes.
    ble_gattc_exchange_mtu(info.getConnHandle(), nullptr, nullptr);
    Serial.println("[ble] connected");
  }
  void onDisconnect(NimBLEServer *srv, NimBLEConnInfo &, int reason) override {
    // The count still includes the link that is going away.
    s_connected = srv->getConnectedCount() > 1;
    Serial.printf("[ble] disconnected (reason %d)\n", reason);
    // Advertising is restarted by poll(), not here.
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo &) override { Serial.printf("[ble] MTU %u\n", mtu); }
};

class WriteCb : public NimBLECharacteristicCallbacks {
  // A write sets the characteristic's value too - for OTA, put the progress
  // report back before anyone reads it.
  void onRead(NimBLECharacteristic *c, NimBLEConnInfo &info) override {
    if (c == s_otaChr) {
      const ota::Report r = ota::report(s_server->getPeerMTU(info.getConnHandle()));
      c->setValue((const uint8_t *)&r, sizeof(r));
    }
  }
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) override {
    const NimBLEAttValue v = c->getValue();
    const NimBLEUUID uuid = c->getUUID();
    if (uuid == NimBLEUUID(BLE_OTA_UUID)) {   // firmware chunk: straight to flash
      s_conn = info.getConnHandle();
      ota::onChunk(v.data(), v.size());
      return;
    }
    portENTER_CRITICAL(&s_mux);
    if (uuid == NimBLEUUID(BLE_MODE_UUID) && v.size() >= 1) {
      s_inbox.hasMode = true;
      s_inbox.mode = v[0];
      s_inbox.slot = v.size() >= 2 ? v[1] : NO_SLOT;
    } else if (uuid == NimBLEUUID(BLE_POS_UUID) && v.size() >= 2) {
      s_inbox.hasPos = true;
      s_inbox.posDeg10 = v[0] | (v[1] << 8);
    } else if (uuid == NimBLEUUID(BLE_AUTO_UUID) && v.size() >= sizeof(AutoParams)) {
      s_inbox.hasAuto = true;
      memcpy(&s_inbox.autoParams, v.data(), sizeof(AutoParams));
    } else if (uuid == NimBLEUUID(BLE_MANUAL_UUID) && v.size() >= sizeof(ManualParams)) {
      s_inbox.hasManual = true;
      memcpy(&s_inbox.manualParams, v.data(), sizeof(ManualParams));
    } else if (uuid == NimBLEUUID(BLE_CMD_UUID) && v.size() >= 1) {
      s_inbox.cmd = v[0];
      memset(s_inbox.cmdArgs, 0, sizeof(s_inbox.cmdArgs));
      memcpy(s_inbox.cmdArgs, v.data() + 1, std::min<size_t>(v.size() - 1, sizeof(s_inbox.cmdArgs)));
    }
    portEXIT_CRITICAL(&s_mux);
  }
};

static ServerCb s_serverCb;
static WriteCb  s_writeCb;

void begin(const AutoParams &initial, const ManualParams &manualInitial) {
  // "Baby Shaker XXXX" from the last two bytes of the factory MAC.
  const uint64_t mac = ESP.getEfuseMac();   // byte 0 of the MAC is the LSB here
  const uint8_t b4 = (mac >> 32) & 0xFF, b5 = (mac >> 40) & 0xFF;
  snprintf(s_name, sizeof(s_name), BLE_NAME_PREFIX "%02X%02X", b4, b5);

  NimBLEDevice::init(s_name);
#ifdef BLE_TEST_ADDRESS
  // Bench builds only: a random static address, so a PC whose BLE stack has
  // cached a stale GATT table for the real address sees a fresh device.
  const uint8_t addr[6] = {0x5A, b5, b4, 0xBB, 0x5B, 0xC0 | 0x1A};   // LSB first, top bits 11 = static
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
  NimBLEDevice::setOwnAddr(addr);
#endif
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(517);
  NimBLEServer *srv = s_server = NimBLEDevice::createServer();
  srv->setCallbacks(&s_serverCb, false);

  NimBLEService *svc = srv->createService(BLE_SVC_UUID);
  s_modeChr = svc->createCharacteristic(BLE_MODE_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *pos = svc->createCharacteristic(BLE_POS_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  s_autoChr = svc->createCharacteristic(BLE_AUTO_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  s_statusChr = svc->createCharacteristic(BLE_STATUS_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *cmd = svc->createCharacteristic(BLE_CMD_UUID, NIMBLE_PROPERTY::WRITE);
  s_statsChr = svc->createCharacteristic(BLE_STATS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  s_manualChr = svc->createCharacteristic(BLE_MANUAL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  s_otaChr = svc->createCharacteristic(BLE_OTA_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY, 512);
  NimBLECharacteristic *info = svc->createCharacteristic(BLE_INFO_UUID, NIMBLE_PROPERTY::READ);

  for (NimBLECharacteristic *c : {s_modeChr, pos, s_autoChr, cmd, s_manualChr, s_otaChr}) c->setCallbacks(&s_writeCb);
  s_modeChr->setValue((uint8_t)MODE_OFF);
  s_autoChr->setValue((const uint8_t *)&initial, sizeof(initial));
  s_manualChr->setValue((const uint8_t *)&manualInitial, sizeof(manualInitial));
  info->setValue(FW_VERSION);
  publishOta();

  // Name in the advertisement (the app filters on it), 128-bit service UUID
  // in the scan response - both won't fit in 31 bytes together.
  NimBLEAdvertisementData adv, scan;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  adv.setName(s_name);
  scan.setCompleteServices(NimBLEUUID(BLE_SVC_UUID));
  NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->setScanResponseData(scan);
  a->setMinInterval(320);   // 200 ms: quick to find, light on power
  a->setMaxInterval(480);   // 300 ms
  a->start();
  Serial.printf("[ble] advertising as \"%s\" (%s), firmware %s\n", s_name,
                NimBLEDevice::getAddress().toString().c_str(), FW_VERSION);
}

// NimBLE-Arduino 2.x does NOT restart advertising after a disconnect by
// default (advertiseOnDisconnect is off), which left the board invisible after
// any page refresh until a reset. Keep advertising whenever a link slot is
// free (3 by default), so a leftover link from a refreshed page can't lock
// the next client out either.
void poll() {
  if (ota::reportDue()) publishOta();
  static uint32_t last = 0;
  if (millis() - last < 250) return;
  last = millis();
  s_connected = s_server->getConnectedCount() > 0;
  NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
  if (!a->isAdvertising() && s_server->getConnectedCount() < CONFIG_BT_NIMBLE_MAX_CONNECTIONS) a->start();
}

const char *deviceName() { return s_name; }
bool connected() { return s_connected; }

bool takeInbox(Inbox &out) {
  portENTER_CRITICAL(&s_mux);
  out = s_inbox;
  s_inbox = Inbox{};
  portEXIT_CRITICAL(&s_mux);
  return out.hasMode || out.hasPos || out.hasAuto || out.hasManual || out.cmd != CMD_NONE;
}

void publishMode(uint8_t mode) {
  s_modeChr->setValue(mode);
  if (s_connected) s_modeChr->notify();
}

void publishAuto(const AutoParams &p) { s_autoChr->setValue((const uint8_t *)&p, sizeof(p)); }

void publishManual(const ManualParams &p) { s_manualChr->setValue((const uint8_t *)&p, sizeof(p)); }

void publishStats(const stats::Summary &s) {
  s_statsChr->setValue((const uint8_t *)&s, sizeof(s));
  if (s_connected) s_statsChr->notify();
}

void publishStatus(const Status &s) {
  s_statusChr->setValue((const uint8_t *)&s, sizeof(s));
  if (s_connected) s_statusChr->notify();
}

void publishOta() {
  const uint16_t mtu = s_conn != BLE_HS_CONN_HANDLE_NONE && s_server ? s_server->getPeerMTU(s_conn) : 23;
  const ota::Report r = ota::report(mtu);
  s_otaChr->setValue((const uint8_t *)&r, sizeof(r));
  // Explicit data: a chunk being written right now overwrites the value, so
  // notify() of "the current value" could send a chunk instead of the report.
  if (s_connected) s_otaChr->notify((const uint8_t *)&r, sizeof(r));
}

// The app's Disconnect asks the board to close the link: when the phone/PC
// closes it, its Bluetooth stack keeps the old link around for a few seconds
// and a reconnect in that window fails.
void dropLinks() {
  for (uint16_t h : s_server->getPeerDevices()) s_server->disconnect(h);
}

int8_t linkRssi() {
  int8_t rssi;
  if (!s_connected || s_conn == BLE_HS_CONN_HANDLE_NONE || ble_gap_conn_rssi(s_conn, &rssi) != 0) return 127;
  return rssi;
}

void fastLink() {
  if (s_conn != BLE_HS_CONN_HANDLE_NONE) s_server->updateConnParams(s_conn, 6, 12, 0, 400);   // 7.5-15 ms
}

}  // namespace ble
