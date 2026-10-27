// SPDX-License-Identifier: MIT
// JoystickTest — stick/trigger sweeps, telemetry on the Serial1 console.

#define UNOR4_TINYUSB_BAUD 230400UL
#include <UnoR4TinyUSB.h>
#include <ArduinoX360_R4.h>

#define TELE UNOR4_TINYUSB_CONSOLE

void setup() {
    TELE.begin(UNOR4_TINYUSB_BAUD);

    ArduinoX360.begin();
}

void loop() {
    tud_arduino_task();
    ArduinoX360.pollRumble();   // OUT packets are queued in the USB ISR; they are delivered here

    static uint8_t phase = 0;

    switch (phase) {
        case 0: // Left stick sweep right
            for (int x = -32767; x <= 32767; x += 1000) {
                ArduinoX360.setStickLeft(x, 0);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 1: // Left stick sweep up
            for (int y = -32767; y <= 32767; y += 1000) {
                ArduinoX360.setStickLeft(0, y);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 2: // Right stick sweep right
            for (int x = -32767; x <= 32767; x += 1000) {
                ArduinoX360.setStickRight(x, 0);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 3: // Right stick sweep up
            for (int y = -32767; y <= 32767; y += 1000) {
                ArduinoX360.setStickRight(0, y);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 4: // Left trigger sweep
            for (uint16_t t = 0; t <= 32768; t += 1000) {
                ArduinoX360.setLeftTrigger(t);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 5: // Right trigger sweep
            for (uint16_t t = 0; t <= 32768; t += 1000) {
                ArduinoX360.setRightTrigger(t);
                ArduinoX360.send();
                delay(5);
            }
            phase++;
            break;

        case 6: // Reset to center/zero
            ArduinoX360.setStickLeft(0, 0);
            ArduinoX360.setStickRight(0, 0);
            ArduinoX360.setLeftTrigger(0);
            ArduinoX360.setRightTrigger(0);
            ArduinoX360.send();
            phase = 0;
            delay(500);
            break;
    }
}
