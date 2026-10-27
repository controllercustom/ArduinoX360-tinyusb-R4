// SPDX-License-Identifier: MIT
// BasicButtons — press/release A in a loop, telemetry on the Serial1 console
// (USB-UART bridge on Rx/Tx/GND; the R4's USB connector is the gamepad port).

#define UNOR4_TINYUSB_BAUD 230400UL
#include <UnoR4TinyUSB.h>
#include <ArduinoX360_R4.h>

#define TELE UNOR4_TINYUSB_CONSOLE

void setup() {
    TELE.begin(UNOR4_TINYUSB_BAUD);

    ArduinoX360.begin();

    while (!TELE) { tud_arduino_task(); delay(10); }
}

void loop() {
    tud_arduino_task();
    ArduinoX360.pollRumble();   // OUT packets are queued in the USB ISR; they are delivered here

    static bool pressed = false;
    pressed = !pressed;

    if (pressed) {
        ArduinoX360.press(ArduinoX360.Button::A);
        TELE.println("Pressing A");
    } else {
        ArduinoX360.release(ArduinoX360.Button::A);
        TELE.println("Releasing A");
    }
    ArduinoX360.send();

    delay(500);
}
