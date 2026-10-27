// SPDX-License-Identifier: MIT
// RumbleFeedback — onRumble/onLed callbacks print host-to-device packets
// (rumble/LED) arriving on the vendor interrupt OUT endpoint. Presses cycle
// A/B/X/Y; a host (xpad via a game, or the DUMP/ffplay path in scripts/)
// sends FF_RUMBLE packets back to us. Console: Serial1 USB-UART bridge.

#define UNOR4_TINYUSB_BAUD 230400UL
#include <UnoR4TinyUSB.h>
#include <ArduinoX360_R4.h>

#define TELE UNOR4_TINYUSB_CONSOLE

void onRumble(uint8_t leftMotor, uint8_t rightMotor) {
    TELE.print("Rumble received - Left motor: ");
    TELE.print(leftMotor);
    TELE.print(", Right motor: ");
    TELE.println(rightMotor);
}

void onLedChange(uint8_t ledIndex) {
    TELE.print("LED change requested, index: ");
    TELE.println(ledIndex);
}

void setup() {
    TELE.begin(UNOR4_TINYUSB_BAUD);

    ArduinoX360.onRumble(onRumble);
    ArduinoX360.onLed(onLedChange);

    ArduinoX360.begin();
}

void loop() {
    tud_arduino_task();
    ArduinoX360.pollRumble();

    static uint8_t buttonIdx = 0;
    const char* names[] = {"A", "B", "X", "Y"};

    TELE.print("Pressing ");
    TELE.println(names[buttonIdx]);

    switch (buttonIdx) {
        case 0: ArduinoX360.press(ArduinoX360.Button::A); break;
        case 1: ArduinoX360.press(ArduinoX360.Button::B); break;
        case 2: ArduinoX360.press(ArduinoX360.Button::X); break;
        case 3: ArduinoX360.press(ArduinoX360.Button::Y); break;
    }
    ArduinoX360.send();   // the g2 backend is explicit: press() only marks dirty

    delay(1000);

    switch (buttonIdx) {
        case 0: ArduinoX360.release(ArduinoX360.Button::A); break;
        case 1: ArduinoX360.release(ArduinoX360.Button::B); break;
        case 2: ArduinoX360.release(ArduinoX360.Button::X); break;
        case 3: ArduinoX360.release(ArduinoX360.Button::Y); break;
    }
    ArduinoX360.send();

    buttonIdx = (buttonIdx + 1) % 4;
}
