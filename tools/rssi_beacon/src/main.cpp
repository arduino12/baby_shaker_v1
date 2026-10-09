// RSSI beacon: advertise at a fixed power/rate and report what we hear.
// Output (serial, every 2 s), one line per peer heard in that window:
//   S <self> <peer> <count> <mean dBm> <min> <max>
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <map>
#include <string>

static char s_name[16];

struct Acc { int n = 0, sum = 0, mn = 0, mx = -200; };
static std::map<std::string, Acc> s_acc;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

class ScanCb : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *d) override {
    const std::string name = d->getName();
    if (name.rfind("RSSI ", 0) != 0 && name.rfind("Baby Shaker", 0) != 0) return;
    const int rssi = d->getRSSI();
    portENTER_CRITICAL(&s_mux);
    Acc &a = s_acc[name];
    if (a.n == 0) a.mn = rssi;
    a.n++;
    a.sum += rssi;
    a.mn = min(a.mn, rssi);
    a.mx = max(a.mx, rssi);
    portEXIT_CRITICAL(&s_mux);
  }
};
static ScanCb s_scanCb;

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // never block on an unread USB port
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(s_name, sizeof(s_name), "RSSI %02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

  NimBLEDevice::init(s_name);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // same as the Baby Shaker firmware

  NimBLEAdvertisementData adv;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  adv.setName(s_name);
  NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->setMinInterval(160);   // 100 ms
  a->setMaxInterval(160);
  a->start();

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&s_scanCb, true);   // every advertisement, not just new devices
  scan->setActiveScan(false);
  scan->setInterval(97);                     // ~60 ms window every 60 ms: listen continuously
  scan->setWindow(97);
  scan->start(0, false, true);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  std::map<std::string, Acc> snap;
  portENTER_CRITICAL(&s_mux);
  snap.swap(s_acc);
  portEXIT_CRITICAL(&s_mux);
  for (auto &kv : snap)
    Serial.printf("S %s %s %d %.1f %d %d\n", s_name, kv.first.c_str(), kv.second.n,
                  (float)kv.second.sum / kv.second.n, kv.second.mn, kv.second.mx);
  if (!NimBLEDevice::getScan()->isScanning()) NimBLEDevice::getScan()->start(0, false, true);
}
