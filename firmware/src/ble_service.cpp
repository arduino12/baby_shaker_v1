#include "ble_service.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "config.h"

namespace ble {

static char                 s_name[24];
static NimBLECharacteristic *s_modeChr, *s_autoChr, *s_statusChr;
static NimBLEServer         *s_server;
static volatile bool        s_connected = false;
static Inbox                s_inbox;
static portMUX_TYPE         s_mux = portMUX_INITIALIZER_UNLOCKED;

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *srv, NimBLEConnInfo &info) override {
    s_connected = true;
    // Snappy slider response: 15-30 ms connection interval, 4 s supervision timeout.
    srv->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
    Serial.println("[ble] connected");
  }
  void onDisconnect(NimBLEServer *srv, NimBLEConnInfo &, int reason) override {
    // The count still includes the link that is going away.
    s_connected = srv->getConnectedCount() > 1;
    Serial.printf("[ble] disconnected (reason %d)\n", reason);
    // NimBLE restarts advertising on its own (advertiseOnDisconnect).
  }
};

class WriteCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    const NimBLEAttValue v = c->getValue();
    const NimBLEUUID uuid = c->getUUID();
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
    } else if (uuid == NimBLEUUID(BLE_CMD_UUID) && v.size() >= 1) {
      s_inbox.cmd = v[0];
    }
    portEXIT_CRITICAL(&s_mux);
  }
};

static ServerCb s_serverCb;
static WriteCb  s_writeCb;

void begin(const AutoParams &initial) {
  // "Baby Shaker XXXX" from the last two bytes of the factory MAC.
  const uint64_t mac = ESP.getEfuseMac();   // byte 0 of the MAC is the LSB here
  const uint8_t b4 = (mac >> 32) & 0xFF, b5 = (mac >> 40) & 0xFF;
  snprintf(s_name, sizeof(s_name), BLE_NAME_PREFIX "%02X%02X", b4, b5);

  NimBLEDevice::init(s_name);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEServer *srv = s_server = NimBLEDevice::createServer();
  srv->setCallbacks(&s_serverCb, false);

  NimBLEService *svc = srv->createService(BLE_SVC_UUID);
  s_modeChr = svc->createCharacteristic(BLE_MODE_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *pos = svc->createCharacteristic(BLE_POS_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  s_autoChr = svc->createCharacteristic(BLE_AUTO_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  s_statusChr = svc->createCharacteristic(BLE_STATUS_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *cmd = svc->createCharacteristic(BLE_CMD_UUID, NIMBLE_PROPERTY::WRITE);

  for (NimBLECharacteristic *c : {s_modeChr, pos, s_autoChr, cmd}) c->setCallbacks(&s_writeCb);
  s_modeChr->setValue((uint8_t)MODE_OFF);
  s_autoChr->setValue((const uint8_t *)&initial, sizeof(initial));

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
  Serial.printf("[ble] advertising as \"%s\"\n", s_name);
}

// NimBLE-Arduino 2.x does NOT restart advertising after a disconnect by
// default (advertiseOnDisconnect is off), which left the board invisible after
// any page refresh until a reset. Keep advertising whenever a link slot is
// free (3 by default), so a leftover link from a refreshed page can't lock
// the next client out either.
void poll() {
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
  return out.hasMode || out.hasPos || out.hasAuto || out.cmd != CMD_NONE;
}

void publishMode(uint8_t mode) {
  s_modeChr->setValue(mode);
  if (s_connected) s_modeChr->notify();
}

void publishAuto(const AutoParams &p) { s_autoChr->setValue((const uint8_t *)&p, sizeof(p)); }

void publishStatus(const Status &s) {
  s_statusChr->setValue((const uint8_t *)&s, sizeof(s));
  if (s_connected) s_statusChr->notify();
}

}  // namespace ble
