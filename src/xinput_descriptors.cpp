// SPDX-License-Identifier: MIT
// ArduinoX360-tinyusb-R4 — XInput USB descriptors (single vendor interface).
//
// Served by the standard tud_descriptor_device/configuration/string_cb. The
// UnoR4 TinyUSB Boards package defines these WEAK in its USB.cpp, so the
// strong definitions here own enumeration and hide the core's default CDC
// identity — same pattern as each UnoR4-tinyusb Device_* example, but
// library-owned so every sketch stays one .ino + tusb_config.h.

#include <UnoR4TinyUSB.h>
#include "ArduinoX360_R4.h"
#include "x360r4_descriptor.h"

// VID:PID assigned by ArduinoX360Class::begin() before tud_arduino_init().
// Descriptors are only read during enumeration, which happens after begin().
uint16_t g_x360_vid = XINPUT_VID_DEFAULT;
uint16_t g_x360_pid = XINPUT_PID_DEFAULT;

static tusb_desc_device_t desc_device =
{
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = 64,
  .idVendor = XINPUT_VID_DEFAULT,
  .idProduct = XINPUT_PID_DEFAULT,
  .bcdDevice = 0x0114,
  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,
  .bNumConfigurations = 0x01
};

#define X360_ITF_NUM        0
#define X360_ITF_STR_INDEX  0x04

static uint8_t desc_fs_configuration[XINPUT_CONFIG_DESC_LEN];

static bool desc_config_built = false;

static void build_configuration(void) {
  if (desc_config_built) return;
  uint8_t cfg[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, XINPUT_CONFIG_DESC_LEN, 0xA0, 250),
  };
  memcpy(desc_fs_configuration, cfg, sizeof(cfg));
  memcpy(desc_fs_configuration + sizeof(cfg), g_xinputIfaceDesc, XINPUT_IFACE_DESC_LEN);
  // Patch interface number + string index into the golden block copy.
  desc_fs_configuration[sizeof(cfg) + 2] = X360_ITF_NUM;
  desc_fs_configuration[sizeof(cfg) + 8] = X360_ITF_STR_INDEX;
  desc_config_built = true;
}

static char const *string_desc_arr[] = {
  (const char[]){ 0x09, 0x04 },  // 0: English
  "Microsoft",                   // 1: Manufacturer
  "XInput Controller",           // 2: Product
  "123456",                      // 3: Serial
  "XInput Controller",           // 4: Interface
};

static uint16_t _desc_str[32];

extern "C" uint8_t const *tud_descriptor_device_cb(void) {
  desc_device.idVendor = g_x360_vid;
  desc_device.idProduct = g_x360_pid;
  return (uint8_t const *) &desc_device;
}

extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  build_configuration();
  return desc_fs_configuration;
}

extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  uint8_t chr_count;
  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
    const char *str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);
    if (chr_count > 31) chr_count = 31;
    for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
  }
  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
  return _desc_str;
}
