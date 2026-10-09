// RSSI beacon: advertise at a fixed power/rate, report what we hear, and the
// chip's internal temperature. Same everything on every module, so the only
// differences left are the modules themselves.
//
// Output (serial, every 2 s):
//   S <self> <peer> <count> <mean dBm> <min> <max>   one line per peer heard
//   T <self> <die temperature C> <radio on|off>
// Serial commands: "radio on" (advertise + listen, default) / "radio off"
// (BLE stopped - separates radio heat from the module's own heat).
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <map>
#include <string>

static char s_name[16];
static bool s_radio = true;

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

static void radio(bool on) {
  s_radio = on;
  NimBLEScan *scan = NimBLEDevice::getScan();
  if (on) {
    NimBLEDevice::getAdvertising()->start();
    scan->start(0, false, true);
  } else {
    scan->stop();
    NimBLEDevice::getAdvertising()->stop();
  }
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);   // never block on an unread USB port
#endif
  setCpuFrequencyMhz(80);     // same as the Baby Shaker firmware
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

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&s_scanCb, true);   // every advertisement, not just new devices
  scan->setActiveScan(false);
  scan->setInterval(97);                     // ~60 ms window every 60 ms: listen continuously
  scan->setWindow(97);
  radio(true);
}

void loop() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c != '\n' && c != '\r') { line += c; continue; }
    line.trim();
    if (line == "radio off") radio(false);
    else if (line == "radio on") radio(true);
    line = "";
  }

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
  Serial.printf("T %s %.1f %s\n", s_name, temperatureRead(), s_radio ? "on" : "off");
  if (s_radio && !NimBLEDevice::getScan()->isScanning()) NimBLEDevice::getScan()->start(0, false, true);
}
