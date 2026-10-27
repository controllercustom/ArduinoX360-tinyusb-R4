// SPDX-License-Identifier: MIT
// ArduinoX360-tinyusb-R4 — board-agnostic state machine (no USB calls here;
// transport lives in ArduinoX360_R4.cpp). Identical to the g2 project's.

#include "ArduinoX360_R4.h"

ArduinoX360Class ArduinoX360;

void ArduinoX360Class::press(Button btn) {
    if (btn >= BUTTON_COUNT) return;
    _report.wButtons |= (1U << static_cast<uint8_t>(btn));
    _markDirty();
}

void ArduinoX360Class::release(Button btn) {
    if (btn >= BUTTON_COUNT) return;
    _report.wButtons &= ~(1U << static_cast<uint8_t>(btn));
    _markDirty();
}

void ArduinoX360Class::setButton(Button btn, bool pressed) {
    if (pressed) press(btn); else release(btn);
}

bool ArduinoX360Class::getButton(Button btn) const {
    if (btn >= BUTTON_COUNT) return false;
    return (_report.wButtons & (1U << static_cast<uint8_t>(btn))) != 0;
}

void ArduinoX360Class::setLeftTrigger(uint16_t value) {
    _report.bLeftTrigger = static_cast<uint8_t>((value > 32768U) ? 255U : (uint8_t)(value * 255U / 32768U));
    _markDirty();
}

void ArduinoX360Class::setRightTrigger(uint16_t value) {
    _report.bRightTrigger = static_cast<uint8_t>((value > 32768U) ? 255U : (uint8_t)(value * 255U / 32768U));
    _markDirty();
}

void ArduinoX360Class::setStickLeft(int16_t x, int16_t y) {
    _report.sThumbLX = x; _report.sThumbLY = y; _markDirty();
}

void ArduinoX360Class::setStickRight(int16_t x, int16_t y) {
    _report.sThumbRX = x; _report.sThumbRY = y; _markDirty();
}

void ArduinoX360Class::setHat(uint8_t hat) {
    _report.wButtons &= ~0x000FU;
    if (hat < 8) {
        static const uint8_t hatToButtons[8] = {0x01,0x09,0x08,0x0A,0x02,0x06,0x04,0x05};
        _report.wButtons |= hatToButtons[hat];
    }
    _markDirty();
}

void ArduinoX360Class::setDpad(uint8_t dir) { setHat(dir); }

uint8_t ArduinoX360Class::getHat() const {
    uint8_t btns = _report.wButtons & 0x000FU;
    bool up=btns&0x01, down=btns&0x02, left=btns&0x04, right=btns&0x08;
    if (!up && !down && !left && !right) return 8;
    if ( up && !down && !left && !right) return 0;
    if ( up && !down &&  right && !left) return 1;
    if (!up && !down &&  right && !left) return 2;
    if (!up &&  down &&  right && !left) return 3;
    if (!up &&  down && !left && !right) return 4;
    if (!up &&  down &&  left && !right) return 5;
    if (!up && !down &&  left && !right) return 6;
    if ( up && !down &&  left && !right) return 7;
    return 8;
}

void ArduinoX360Class::onRumble(RumbleCallback cb) { _onRumbleCb = std::move(cb); }
void ArduinoX360Class::onLed(LedCallback cb) { _onLedCb = std::move(cb); }

uint8_t ArduinoX360Class::getLastRumbleLeft() const { return _lastRumbleL; }
uint8_t ArduinoX360Class::getLastRumbleRight() const { return _lastRumbleR; }
uint8_t ArduinoX360Class::getLastLedIndex() const { return _lastLedIndex; }
bool ArduinoX360Class::hasReceivedOutput() const { return _receivedAnyOutput; }
const ArduinoX360Class::XInputReport& ArduinoX360Class::getReport() const { return _report; }
