#include "ota.h"

#include <Arduino.h>
#include <Update.h>

namespace ota {

static volatile State    s_state = IDLE;
static volatile uint8_t  s_error = 0;
static volatile uint32_t s_size = 0, s_received = 0, s_reportedAt = 0;
static volatile bool     s_reportDue = false;

bool active() { return s_state == READY || s_state == RECEIVING; }

bool begin(uint32_t size) {
  if (active()) Update.abort();
  s_received = 0;
  s_reportedAt = 0;
  s_size = size;
  s_error = 0;
  if (size == 0 || !Update.begin(size)) {   // too big for the OTA slot, or no OTA partition
    s_error = Update.getError();
    s_state = FAILED;
    Serial.printf("[ota] begin(%lu) failed: %s\n", (unsigned long)size, Update.errorString());
  } else {
    s_state = READY;
    Serial.printf("[ota] ready for %lu bytes\n", (unsigned long)size);
  }
  s_reportDue = true;
  return s_state == READY;
}

void onChunk(const uint8_t *p, size_t n) {
  if (!active() || n < 5) return;
  const uint32_t offset = p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
  if (offset != s_received) {   // a chunk got lost: drop this one; the app resumes from s_received
    s_reportDue = true;
    return;
  }
  const size_t len = n - 4;
  if (s_received + len > s_size || Update.write(const_cast<uint8_t *>(p + 4), len) != len) {
    s_error = Update.getError();
    s_state = FAILED;
    Update.abort();
    s_reportDue = true;
    return;
  }
  s_state = RECEIVING;
  s_received = s_received + len;
  if (s_received - s_reportedAt >= 4096 || s_received == s_size) {   // the app allows 4 KB unacknowledged
    s_reportedAt = s_received;
    s_reportDue = true;
  }
}

bool finish() {
  if (!active() || s_received != s_size) {
    Serial.printf("[ota] end refused: %lu of %lu bytes\n", (unsigned long)s_received, (unsigned long)s_size);
    s_state = FAILED;
    if (Update.isRunning()) Update.abort();
  } else if (!Update.end()) {   // verifies the image, then sets it as the boot partition
    s_error = Update.getError();
    s_state = FAILED;
    Serial.printf("[ota] verify failed: %s\n", Update.errorString());
  } else {
    s_state = DONE;
    Serial.println("[ota] image verified - restarting into it");
  }
  s_reportDue = true;
  return s_state == DONE;
}

void abort() {
  if (Update.isRunning()) Update.abort();
  s_state = IDLE;
  s_reportDue = true;
}

Report report(uint16_t mtu) {
  return {(uint8_t)s_state, s_error, mtu, s_size, s_received};
}

bool reportDue() {
  const bool due = s_reportDue;
  s_reportDue = false;
  return due;
}

}  // namespace ota
