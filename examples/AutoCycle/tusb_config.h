/*
 * tusb_config.h — sketch-local TinyUSB config for ArduinoX360-tinyusb-R4.
 *
 * This file next to the .ino is the Arduino-IDE-2.x route: the UnoR4 TinyUSB
 * board package (unor4tinyusb:renesas_uno) adds -iquote{build.source.path} to
 * its platform.txt, and -iquote directories are searched BEFORE -I — so this
 * copy wins for every translation unit, including the core's own vendored
 * TinyUSB TUs (class/vendor/vendor_device.c is one of them). The EP_INT gates
 * in that header are plain #ifndef defaults, so the two lines below flip them
 * on. No custom compiler flags anywhere; the IDE builds exactly what the CLI
 * gate builds.
 *
 * Role: DEVICE only (vendor class, interrupt-EP pair). CFG_TUH_ENABLED 0
 * drops the ~8 KB host stack. CFG_TUD_CDC must stay >= 1: the core's USB.cpp
 * builds a TUD_CDC_DESCRIPTOR outside any #if and SerialUSB.cpp calls the
 * CDC API unconditionally — 0 fails to compile the CORE, not just the sketch.
 *
 * The two EP_INT lines are the whole point of this file: they turn on the
 * vendor interrupt-EP pair (IN 0x81 = XInput reports out / OUT 0x01 = rumble
 * + LED packets in). ArduinoX360_R4.cpp checks the same macros in its own TU
 * (compiled with the same -iquote flags) and picks the full backend.
 *
 * SELF-CONTAINED by necessity: a config that #includes a helper header
 * resolves for the sketch and fails with "No such file or directory" inside
 * the core's class TUs. The shared body below is kept verbatim from the
 * UnoR4-tinyusb generated configs so it cannot drift.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// ==== BEGIN GENERATED SECTION (ArduinoX360-tinyusb-R4: device + vendor) ====
// Board check. board_auto.h is NOT reachable from here: the core's TinyUSB
// translation units get no -I for the library, so derive the board from the
// core's own command-line macro, which is present in every TU. The library's
// own sources get the same value from UnoR4TinyUSB/board_auto.h, and an
// identical redefinition is benign.
#if !defined(ARDUINO_ARCH_RENESAS)
  #error "Unsupported board: this config is for the Arduino Renesas RA4M1 core (renesas_uno). No ARDUINO_ARCH_RENESAS on the command line."
#endif
#ifndef UNOR4_TINYUSB_BOARD_R4
  #define UNOR4_TINYUSB_BOARD_R4 1
#endif

// ---- Device stack ----
#define CFG_TUD_ENABLED                      1  // required by the core: USB.cpp / SerialUSB.cpp
#define CFG_TUD_CDC                          1  // core requires >= 1 (USB.cpp / SerialUSB.cpp)
#define CFG_TUD_VENDOR                       1  // XInput = vendor-class device

// The vendor interrupt-EP pair: IN 0x81 (reports out) / OUT 0x01 (rumble+LED in).
#define CFG_TUD_VENDOR_EP_INT_IN             1
#define CFG_TUD_VENDOR_EP_INT_OUT            1

// ---- Host stack ----
#define CFG_TUH_ENABLED                      0  // device role: UnoR4TinyUSB.h role guard
// ==== END GENERATED SECTION ====

// ---------------------------------------------------------------------------
// Everything below is IDENTICAL in every copy in this repo: one tusb_config.h
// per sketch folder, 9 in total, all byte-identical. There is no generator
// script here, so edit one and copy it over the others (md5sum them to be
// sure they did not drift).
// ---------------------------------------------------------------------------

// ---- MCU / RHPort -----------------------------------------------------------
// CFG_TUSB_MCU is NOT defined here: boards.txt passes -DCFG_TUSB_MCU=OPT_MCU_RAXXX
// on the command line for every renesas_uno target, and redefining it would be
// a (harmless but noisy) duplicate definition.
//
// CFG_TUSB_RHPORT0_MODE must stay (OPT_MODE_DEVICE). Do NOT "helpfully" add
// OPT_MODE_FULL_SPEED or OPT_MODE_HOST:
//  - CFG_TUH_ENABLED above is what actually switches the host stack on, and it
//    is also what the UnoR4TinyUSB.h role guards key off.
//  - The RUSB2 HCD is full-speed only and picks its PHY setup from
//    rusb2_is_highspeed_rhport(), not from this macro.
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE)

// R4 core uses a bare-metal loop, no RTOS.
#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS           OPT_OS_NONE
#endif

// rhport 0 is the only RUSB2 port. The core's USB.cpp uses BOARD_TUD_RHPORT
// unconditionally and guards its host block on BOARD_TUH_RHPORT, which must stay
// UNDEFINED: defining it would make the core call tuh_rhport_init() during
// __USBStart(), before the sketch has chosen a role, and device-mode init hangs.
#ifndef BOARD_TUD_RHPORT
  #define BOARD_TUD_RHPORT     0
#endif

// EP0. The R4's USBFS endpoint-0 max packet size is 64 in both directions.
#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE    64
#endif

// ---- Event queues -----------------------------------------------------------
// Deep, and NOT to be trimmed. Enumeration on the R4 is bursty enough to
// overflow the upstream default of 16, which silently drops SETUP /
// xfer-complete events: the device ACKs setups in hardware, software never sees
// them, and the host sees endless NAKs on DATA INs.
// 64 entries costs ~768 B of the 32 KB SRAM. Keep it.
#define CFG_TUD_TASK_QUEUE_SZ   64
#define CFG_TUH_TASK_QUEUE_SZ   64

// ---- Host limits (hardware, not preference) --------------------------------
// The RUSB2 HCD hard-asserts dev_addr < 6 and has 5 bulk pipes (1..5; 6..9 are
// interrupt-only). These caps are structural:
//  - CFG_TUH_DEVICE_MAX 5: a hub counts as one, so a hub plus N devices cannot
//    exceed 5 addresses. Measured: 2 CDC bridges through a hub both open; a
//    third enumerates but cannot open.
//  - Do NOT raise CFG_TUH_DEVICE_MAX past 4-5. At 8 enumeration dies entirely
//    (root cause unknown, suspected static RAM: the CDC and device slot arrays
//    are linked for every sketch regardless of use).
#ifndef CFG_TUH_DEVICE_MAX
  #define CFG_TUH_DEVICE_MAX        4
#endif

// Multi-LUN readers (Realtek 0bda:0306 puts the card on LUN 1 and leaves LUN 0
// empty). The msc_host multi-LUN walk costs +4 B static per extra LUN.
#ifndef CFG_TUH_MSC_MAXLUN
  #define CFG_TUH_MSC_MAXLUN        4
#endif

#if CFG_TUD_ENABLED
// Device buffer sizes
#define CFG_TUD_CDC_RX_BUFSIZE   256
#define CFG_TUD_CDC_TX_BUFSIZE   256
#define CFG_TUD_MSC_BUFSIZE      512
#define CFG_TUD_MSC_EP_BUFSIZE   512
#define CFG_TUD_MIDI_RX_BUFSIZE  256
#define CFG_TUD_MIDI_TX_BUFSIZE  256
#define CFG_TUD_VENDOR_RX_BUFSIZE 512
#define CFG_TUD_VENDOR_TX_BUFSIZE 512

// Printer device FIFOs. Macro-only: zero cost unless CFG_TUD_PRINTER > 0, but
// printer_device.c references them unconditionally inside its own guard, so
// they must exist. The HOST printer driver needs no equivalent (it streams on
// the bulk endpoints).
#define CFG_TUD_PRINTER_RX_BUFSIZE 256
#define CFG_TUD_PRINTER_TX_BUFSIZE 256
#endif

// ---- Audio device -----------------------------------------------------------
// Mono 16-bit 48 kHz IN only. Macro-only: zero cost unless CFG_TUD_AUDIO > 0.
// FS isochronous EP size = (48000/1000 + 1) * 2 B * 1 ch = 98 B; the SW FIFO is
// 4 frames.
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE            48000
#define CFG_TUD_AUDIO_ENABLE_EP_IN                  1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX  2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX          1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX           98
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ        392
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX          98
#define CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX           98
#define CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX          98
#define CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX           98
#define CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX          98
#define CFG_TUD_AUDIO_EP_SZ_IN                      98

#if CFG_TUH_ENABLED
// Host buffer sizes. 512 B measured 11.2 KB/s of real UART throughput on
// CP2102 and 11.3 KB/s on FT232R (J-Link + PL2303 console), zero errors over
// 1.29 MB. Going larger does not raise that: the RUSB2 FIFO depth, not the
// software buffer, is the limit.
#define CFG_TUH_CDC_RX_BUFSIZE   512
#define CFG_TUH_CDC_TX_BUFSIZE   512
#define CFG_TUH_MIDI_RX_BUFSIZE  256
#define CFG_TUH_MIDI_TX_BUFSIZE  256
#define CFG_TUH_MSC_EP_BUFSIZE   512
#define CFG_TUH_HUB_BUFSIZE      64

// CFG_TUH_CDC_LINE_CODING_ON_ENUM is deliberately UNDEFINED. Setting it makes
// every enumerated CDC device receive a SET_LINE_CODING during host config, and
// a control OUT transfer with data is exactly the case the RUSB2 HCD needs its
// DIR / PID=NAK fixes for. The port still mounts without it; the sketch sets
// the baudrate once the device is up (and cdc_host.c now initialises line
// coding to 8N1, so a bare tuh_cdc_set_baudrate() is not silently rejected by
// the PL2303 driver's sanity gate).

// Host audio config (macro-only; ~1.3 KB static when CFG_TUH_AUDIO > 0).
#define CFG_TUH_AUDIO_PROTOCOLS  (TUH_AUDIO_PROTOCOL_UAC1 | TUH_AUDIO_PROTOCOL_UAC2)
#define CFG_TUH_AUDIO_MAX        1
#define CFG_TUH_AUDIO_MAX_SAM_FREQ 5
#define CFG_TUH_AUDIO_MAX_AS     2
#define CFG_TUH_AUDIO_EPIN_BUFSIZE   256
#define CFG_TUH_AUDIO_STREAM_BUFSIZE 512
#endif // CFG_TUH_ENABLED

// ---- Debug ------------------------------------------------------------------
// Off by default (upstream default). TU_LOG runs inside the USB ISR, and a
// ~40-char line at 115200 baud blocks for ~3.5 ms — longer than a 1 ms
// isochronous audio frame.
// #ifndef-guarded, so -DCFG_TUSB_DEBUG=2 re-enables it for bring-up without
// editing files. CFG_TUSB_DEBUG_PRINTF stays outside the guard:
// arduino_debug_printf() (src/UnoR4TinyUSB/bsp_common.cpp) must resolve even
// while logging is off, or the opt-in fails at link.
//
// When debug IS enabled, the ring-buffered printf is what keeps it safe: it is
// drained from loop() rather than written inline, because Serial1.write()
// blocks on the FSP SCI TX-done interrupt that cannot preempt the USB ISR.
#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG           0
#endif
#define CFG_TUSB_DEBUG_PRINTF    arduino_debug_printf

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
