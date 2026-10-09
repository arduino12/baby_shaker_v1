// Firmware update over BLE.
//
// The app sends CMD_OTA_BEGIN (size), then the image as chunks on the OTA
// characteristic - each chunk is u32 offset + data - then CMD_OTA_END. Chunks
// are written straight to the idle OTA partition (Arduino Update library); a
// chunk with the wrong offset (one was lost) is dropped and the next report
// tells the app where to resume. END verifies the image and switches the boot
// partition; the board then restarts into the new firmware.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ota {

enum State : uint8_t { IDLE = 0, READY = 1, RECEIVING = 2, DONE = 3, FAILED = 4 };

struct __attribute__((packed)) Report {   // OTA characteristic value / notification, 12 bytes
  uint8_t  state;
  uint8_t  error;      // Update library error code when FAILED
  uint16_t mtu;        // negotiated ATT MTU: the app sizes its chunks from this
  uint32_t size;       // image size from BEGIN
  uint32_t received;   // bytes written = the offset the next chunk must have
};

bool   active();                              // READY or RECEIVING
bool   begin(uint32_t size);                  // main loop
void   onChunk(const uint8_t *p, size_t n);   // BLE host task
bool   finish();                              // main loop; true = verified, boot partition switched
void   abort();
Report report(uint16_t mtu);
bool   reportDue();                           // a report is worth sending (state change / progress)

}  // namespace ota
