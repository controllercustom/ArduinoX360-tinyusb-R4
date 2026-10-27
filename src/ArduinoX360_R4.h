// SPDX-License-Identifier: MIT
// ArduinoX360-tinyusb-R4 — Xbox 360 (XInput) gamepad emulation for the
// Arduino Uno R4 Minima and Arduino Nano R4, on the UnoR4-tinyusb stack.
//
// Public API matches the sibling library ArduinoX360-tinyusb (Button enum,
// triggers, sticks, hat, rumble/LED callbacks) EXCEPT for setPollInterval,
// which is deliberately absent: this backend has no internal poll timer, so
// reports move only when the sketch calls send()/update() from loop(). A
// method that stored an unused interval would have been a silent no-op. The
// only addition here is handleOutput(), public so the OUT path can be driven
// from pollRumble(). Transport is the
// TinyUSB vendor interrupt-EP pair (IN 0x81 / OUT 0x01) provided by the
// UnoR4 TinyUSB Boards package; the sketch-local tusb_config.h beside
// every example enables it (CFG_TUD_VENDOR_EP_INT_IN/OUT = 1), which is
// what makes plain Arduino IDE 2.x builds work.
//
// The header is named ArduinoX360_R4.h, deliberately NOT ArduinoX360.h: the
// sibling projects ArduinoX360-tinyusb and ArduinoX360-tinyusb-g2 both provide
// <ArduinoX360.h>, and with the user's libraries dir searched first, sharing a
// name would make include resolution order-dependent and silently build the
// wrong backend when two of these libraries are installed side by side.

#pragma once

#include <Arduino.h>
#include <functional>

#if !defined(ARDUINO_ARCH_RENESAS)
#error "ArduinoX360-tinyusb-R4 is Uno R4 Minima / Nano R4 only (UnoR4 TinyUSB Boards, renesas_uno core). Use ArduinoX360-tinyusb for other boards."
#endif

#define XINPUT_REPORT_SIZE 20U
#define XINPUT_VID_DEFAULT 0x045EU
#define XINPUT_PID_DEFAULT 0x028EU

class ArduinoX360Class {
public:
    struct __attribute__((packed)) XInputReport {
        uint8_t  bMessageType;      // 0x00
        uint8_t  bMessageSize;      // 0x14
        uint16_t wButtons;
        uint8_t  bLeftTrigger;
        uint8_t  bRightTrigger;
        int16_t  sThumbLX;
        int16_t  sThumbLY;
        int16_t  sThumbRX;
        int16_t  sThumbRY;
        uint32_t dwReserved0;
        uint16_t wReserved1;
    };

    enum Button : uint8_t {
        DPAD_UP = 0,
        DPAD_DOWN,
        DPAD_LEFT,
        DPAD_RIGHT,
        START,
        BACK,
        LEFT_THUMB,
        RIGHT_THUMB,
        LEFT_SHOULDER,
        RIGHT_SHOULDER,
        XBOX,
        A = 12,
        B,
        X,
        Y,
        BUTTON_COUNT = 16U
    };

    void begin(uint16_t vid = XINPUT_VID_DEFAULT, uint16_t pid = XINPUT_PID_DEFAULT);
    bool isConnected() const;

    void press(Button btn);
    void release(Button btn);
    void setButton(Button btn, bool pressed);
    bool getButton(Button btn) const;

    void setLeftTrigger(uint16_t value);
    void setRightTrigger(uint16_t value);
    void setStickLeft(int16_t x, int16_t y);
    void setStickRight(int16_t x, int16_t y);

    void setHat(uint8_t hat);
    void setDpad(uint8_t dir);
    uint8_t getHat() const;

    using RumbleCallback = std::function<void(uint8_t leftMotor, uint8_t rightMotor)>;
    using LedCallback    = std::function<void(uint8_t ledIndex)>;
    void onRumble(RumbleCallback cb);
    void onLed(LedCallback cb);

    void update();
    void send();
    bool ready();
    void releaseAll();
    void pollRumble();
    uint8_t getLastRumbleLeft() const;
    uint8_t getLastRumbleRight() const;
    uint8_t getLastLedIndex() const;
    bool hasReceivedOutput() const;
    const XInputReport& getReport() const;

    // Parse one host-to-device OUT packet (rumble 0x00.. / LED 0x01..).
    // pollRumble() calls this in loop() context with a queued packet; the
    // interrupt-OUT callback only queues packets and never parses here,
    // because printing from that ISR deadlocks the R4 console.
    void handleOutput(const uint8_t *data, uint32_t len);

private:
    void _sendReport();
    bool _isDirty() const { return _dirtyFlag; }
    void _markDirty() { _dirtyFlag = true; }
    bool _canSend() const;

    XInputReport _report{0x00, 0x14};
    volatile bool _dirtyFlag{false};

    bool _connected = false;
    mutable volatile bool _usbReady = false;
    mutable unsigned long _mountedAt = 0;

    RumbleCallback _onRumbleCb = nullptr;
    LedCallback    _onLedCb   = nullptr;

    uint8_t _lastRumbleL  = 0;
    uint8_t _lastRumbleR  = 0;
    uint8_t _lastLedIndex = 0xFF;

    bool _receivedAnyOutput = false;
};

static_assert(sizeof(ArduinoX360Class::XInputReport) == XINPUT_REPORT_SIZE,
    "XInputReport must be exactly 20 bytes");

extern ArduinoX360Class ArduinoX360;
