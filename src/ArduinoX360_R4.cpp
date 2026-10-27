// SPDX-License-Identifier: MIT
// ArduinoX360-tinyusb-R4 — USB transport backend for Uno R4 Minima / Nano R4
// on the UnoR4-tinyusb stack (UnoR4 TinyUSB Boards package, vendored TinyUSB
// master whose vendor class ships the interrupt-EP pair + the RA4M1 core).
//
// Reports go out on the vendor interrupt IN endpoint (0x81) via
// tud_vendor_n_int_write(); host rumble/LED packets arrive on the vendor
// interrupt OUT endpoint (0x01) via tud_vendor_int_rx_cb(). The pair is
// enabled by the sketch-local tusb_config.h next to every example
// (CFG_TUD_VENDOR_EP_INT_IN/OUT = 1): the package's platform.txt adds
// -iquote{build.source.path}, searched before -I, so that copy wins for
// every translation unit — the core's vendored vendor_device.c included —
// and no custom compiler flags are needed (Arduino IDE 2.x builds work).
//
// The core's descriptor callbacks are weak (UnoR4 TinyUSB Boards package),
// so the library's strong tud_descriptor_*_cb in xinput_descriptors.cpp own
// enumeration and hide the core's default CDC identity — library-owned so
// each sketch stays one .ino + tusb_config.h.
//
// Loop-pumped: the sketch calls tud_arduino_task() plus pollRumble()/send()
// from loop(). No hardware timer is used.

#include <UnoR4TinyUSB.h>
#include "ArduinoX360_R4.h"

// Configs that carry the vendor interrupt-EP pair (verified at compile time)
// get the full backend; everything else fails at build time with an
// actionable #error — this should never happen in this repo, since every
// example ships the config. It catches two mistakes: a sketch built without a
// tusb_config.h, and a sketch built against the STOCK arduino:renesas_uno core,
// whose platform.txt has no -iquote{build.source.path} so the sketch-local
// config never reaches any TU. That core also defines ARDUINO_ARCH_RENESAS, so
// the header guard cannot tell the two cores apart — this check can.
#if defined(CFG_TUD_VENDOR_EP_INT_IN) && CFG_TUD_VENDOR_EP_INT_IN && \
    defined(CFG_TUD_VENDOR_EP_INT_OUT) && CFG_TUD_VENDOR_EP_INT_OUT
#define X360_INT_EP 1
#else
#define X360_INT_EP 0
#endif

#if X360_INT_EP

extern uint16_t g_x360_vid;
extern uint16_t g_x360_pid;

static uint8_t g_x360_report[XINPUT_REPORT_SIZE] __attribute__((aligned(4)));
static bool s_txPending = false;

// One OUT packet is 8 bytes; depth 4 costs 80 B of BSS and covers a burst of
// host rumble/LED packets between two loop() iterations.
#define OUT_QUEUE_DEPTH 4
static uint8_t s_outPackets[OUT_QUEUE_DEPTH][XINPUT_REPORT_SIZE] __attribute__((aligned(4)));

void ArduinoX360Class::handleOutput(const uint8_t *data, uint32_t len) {
  if (len < 1) return;
  _receivedAnyOutput = true;
  // Latch state unconditionally; callbacks are optional notifications.
  if (data[0] == 0x00 && len >= 5) {
    _lastRumbleL = data[3]; _lastRumbleR = data[4];
    if (_onRumbleCb) _onRumbleCb(_lastRumbleL, _lastRumbleR);
  } else if (data[0] == 0x01 && len >= 3) {
    _lastLedIndex = data[2];
    if (_onLedCb) _onLedCb(_lastLedIndex);
  }
}

// XInput vendor control requests: GET_REPORT (wValue 0x0100, 20 bytes) and
// capabilities (wValue 0x0000, 8 bytes). Weak in the core/usbd.c.
extern "C" bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                           tusb_control_request_t const *request) {
  if (stage != CONTROL_STAGE_SETUP) return true;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
      request->bmRequestType_bit.direction == TUSB_DIR_IN &&
      request->bRequest == 0x01) {
    if (request->wValue == 0x0100) {
      static uint8_t buf[XINPUT_REPORT_SIZE];
      memcpy(buf, g_x360_report, sizeof(buf));
      return tud_control_xfer(rhport, request, buf, XINPUT_REPORT_SIZE);
    }
    if (request->wValue == 0x0000) {
      static uint8_t caps[8] = {0x00, 0x08, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00};
      return tud_control_xfer(rhport, request, caps, sizeof(caps));
    }
  }
  return false;
}

// OUT packets are QUEUED here, never handled here: this callback runs inside
// the USB ISR, and the R4 console's Serial1.write() blocks on the FSP SCI
// TX-done interrupt, which cannot preempt the USB ISR — a sketch that prints
// from an onRumble/onLed callback would deadlock (observed on the bench: PC
// stuck in UART::write with IPSR=010). pollRumble(), called from loop(),
// drains the slot and invokes the callbacks where printing is legal.
static uint8_t s_outLens[OUT_QUEUE_DEPTH];
static volatile uint8_t s_outHead = 0;   // written by the ISR
static volatile uint8_t s_outTail = 0;   // read by loop()

extern "C" void tud_vendor_int_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize) {
  (void) idx;
  uint8_t next = (uint8_t)((s_outHead + 1) % OUT_QUEUE_DEPTH);
  if (next != s_outTail && bufsize > 0) {      // drop only when the queue is full
    uint32_t n = bufsize > XINPUT_REPORT_SIZE ? XINPUT_REPORT_SIZE : bufsize;
    memcpy(s_outPackets[s_outHead], buffer, n);
    s_outLens[s_outHead] = (uint8_t)n;
    s_outHead = next;
  }
  tud_vendor_n_int_read_xfer(0);               // re-arm for the next packet
}

extern "C" void tud_vendor_int_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void) idx; (void) sent_bytes;
}

extern "C" void tud_mount_cb(void) {
  // Arm the interrupt OUT endpoint so host rumble/LED packets arrive.
  tud_vendor_n_int_read_xfer(0);
}

void ArduinoX360Class::begin(uint16_t vid, uint16_t pid) {
  g_x360_vid = vid;
  g_x360_pid = pid;
  memset(g_x360_report, 0, sizeof(g_x360_report));
  g_x360_report[0] = 0x00; g_x360_report[1] = 0x14;
  _connected = true;
  _report.bMessageType = 0x00; _report.bMessageSize = 0x14;
  s_txPending = false;
  // R4: bookkeeping only — the core already brought rhport 0 up as DEVICE
  // in _init().
  tud_arduino_init();
}

bool ArduinoX360Class::_canSend() const {
  if (!tud_mounted() || !tud_ready()) return false;
  if (!_usbReady) { _mountedAt = millis(); _usbReady = true; }
  return ((unsigned long)(millis() - _mountedAt)) >= 100UL;
}

bool ArduinoX360Class::isConnected() const {
  if (!_connected) return false;
  bool mounted = tud_mounted();
  if (!mounted && _usbReady) _usbReady = false;
  if (mounted && !_usbReady) { _mountedAt = millis(); _usbReady = true; }
  if (_usbReady && ((unsigned long)(millis() - _mountedAt)) < 100UL) return false;
  return mounted;
}

void ArduinoX360Class::_sendReport() {
  if (!_isDirty()) {
    // Retry a previously stalled report once the endpoint frees up.
    if (!s_txPending) return;
  }
  if (!_canSend()) return;
  if (tud_vendor_n_int_write_available(0) < XINPUT_REPORT_SIZE) {
    s_txPending = true;
    return;
  }
  memcpy(g_x360_report, &_report, sizeof(_report));
  uint32_t n = tud_vendor_n_int_write(0, g_x360_report, XINPUT_REPORT_SIZE);
  if (n == XINPUT_REPORT_SIZE) {
    s_txPending = false;
    _dirtyFlag = false;
  } else {
    s_txPending = true;
  }
}

void ArduinoX360Class::update() { _sendReport(); }
void ArduinoX360Class::send() { _sendReport(); }
bool ArduinoX360Class::ready() { return isConnected(); }

void ArduinoX360Class::releaseAll() {
  memset(&_report, 0, sizeof(_report));
  _report.bMessageType = 0x00; _report.bMessageSize = 0x14;
  _markDirty();
  _sendReport();
}

void ArduinoX360Class::pollRumble() {
  // Drain queued OUT packets HERE: loop() context, where callbacks may print.
  while (s_outTail != s_outHead) {
    uint8_t i = s_outTail;
    handleOutput(s_outPackets[i], s_outLens[i]);
    s_outTail = (uint8_t)((i + 1) % OUT_QUEUE_DEPTH);
  }
  if (!tud_mounted()) return;
  // Ensure the interrupt OUT endpoint is armed (covers the mount race
  // before tud_mount_cb runs; no-op while a transfer is ongoing).
  tud_vendor_n_int_read_xfer(0);
}

#else // !X360_INT_EP — no vendor interrupt-EP pair: fail at BUILD time.

// Two mistakes land here, and both need the same kind of fix:
//   * the sketch has no tusb_config.h next to it, or
//   * the sketch was built against the STOCK arduino:renesas_uno core, whose
//     platform.txt has no -iquote{build.source.path} — the sketch-local config
//     then reaches no TU at all. That core also passes -DARDUINO_ARCH_RENESAS,
//     so the header guard above cannot tell the two cores apart; this check can.
// A dead binary that prints one line on a console nobody wired up is worse than
// a compile error, so stop here. The message is what the IDE/CLI prints.
#error "ArduinoX360-tinyusb-R4 needs the vendor interrupt-EP pair: select UnoR4 TinyUSB Boards -> UNO R4 Minima (TinyUSB) or Nano R4 (TinyUSB), and keep the sketch-local tusb_config.h with CFG_TUD_VENDOR_EP_INT_IN/OUT = 1 next to the sketch"

#endif // X360_INT_EP
