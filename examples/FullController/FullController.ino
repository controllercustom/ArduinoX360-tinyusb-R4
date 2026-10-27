// SPDX-License-Identifier: MIT
// FullController — physical gamepad conversion for Uno R4 Minima / Nano R4.
//
// Buttons (INPUT_PULLUP, active-low) on the digital pins; D0/D1 are the
// Serial1 console and stay untouched. D7 doubles as the built-in RGB LED
// data line — unused here, so it is safe as a button.
//
//   D2=A  D3=B  D4=X  D5=Y  D6=LB  D7=RB  D8=BACK  D9=START
//   D10=DPAD_UP  D11=DPAD_DOWN  D12=DPAD_LEFT  D13=DPAD_RIGHT
//
// Sticks + triggers from the 14-bit ADC (analogReadResolution(14), 0..16383
// @5V). A0 doubles as the 12-bit DAC (unused here); A4/A5 double as I2C
// (unused here). Both boards have A0..A5, so this sketch is identical for
// Minima and Nano R4.
//
//   A1=LX  A2=LY  A3=RX  A4=RY  A5=TRIG_L  A0=TRIG_R
//
// Console: Serial1 USB-UART bridge on Rx/Tx/GND (the R4's USB connector is
// the gamepad port).

#define UNOR4_TINYUSB_BAUD 230400UL
#include <UnoR4TinyUSB.h>
#include <ArduinoX360_R4.h>

#define TELE UNOR4_TINYUSB_CONSOLE

static const struct { uint8_t pin; ArduinoX360Class::Button btn; } btn_pins[] = {
  {2,  ArduinoX360Class::A},
  {3,  ArduinoX360Class::B},
  {4,  ArduinoX360Class::X},
  {5,  ArduinoX360Class::Y},
  {6,  ArduinoX360Class::LEFT_SHOULDER},
  {7,  ArduinoX360Class::RIGHT_SHOULDER},
  {8,  ArduinoX360Class::BACK},
  {9,  ArduinoX360Class::START},
};

#define PIN_DPAD_UP    10
#define PIN_DPAD_DOWN  11
#define PIN_DPAD_LEFT  12
#define PIN_DPAD_RIGHT 13

#define PIN_LX      A1
#define PIN_LY      A2
#define PIN_RX      A3
#define PIN_RY      A4
#define PIN_TRIG_L  A5
#define PIN_TRIG_R  A0

// 14-bit ADC (0..16383) -> signed stick range (-32767..32767).
static inline int16_t stick14(uint16_t v) {
  return (int16_t)((int32_t)v * 65534 / 16383 - 32767);
}
// 14-bit ADC -> trigger range 0..32768 (library scales to 0..255).
static inline uint16_t trigger14(uint16_t v) {
  return (uint16_t)((int32_t)v * 2);
}

void setup() {
  for (size_t i = 0; i < sizeof(btn_pins) / sizeof(btn_pins[0]); i++) {
    pinMode(btn_pins[i].pin, INPUT_PULLUP);
  }
  pinMode(PIN_DPAD_UP, INPUT_PULLUP);
  pinMode(PIN_DPAD_DOWN, INPUT_PULLUP);
  pinMode(PIN_DPAD_LEFT, INPUT_PULLUP);
  pinMode(PIN_DPAD_RIGHT, INPUT_PULLUP);

  analogReadResolution(14);

  TELE.begin(UNOR4_TINYUSB_BAUD);
  ArduinoX360.begin();
  delay(500);
}

void loop() {
  tud_arduino_task();
  ArduinoX360.pollRumble();

  for (size_t i = 0; i < sizeof(btn_pins) / sizeof(btn_pins[0]); i++) {
    ArduinoX360.setButton(btn_pins[i].btn, !digitalRead(btn_pins[i].pin));
  }

  uint8_t dpad = 8; // centered
  bool up    = !digitalRead(PIN_DPAD_UP);
  bool down  = !digitalRead(PIN_DPAD_DOWN);
  bool left  = !digitalRead(PIN_DPAD_LEFT);
  bool right = !digitalRead(PIN_DPAD_RIGHT);

  if (up && !down && !left && !right) dpad = 0;
  else if (up && !down && right && !left) dpad = 1;
  else if (!up && !down && right && !left) dpad = 2;
  else if (!up && down && right && !left) dpad = 3;
  else if (!up && down && !left && !right) dpad = 4;
  else if (!up && down && left && !right) dpad = 5;
  else if (!up && !down && left && !right) dpad = 6;
  else if (up && !down && left && !right) dpad = 7;
  ArduinoX360.setHat(dpad);

  ArduinoX360.setStickLeft(stick14(analogRead(PIN_LX)), stick14(analogRead(PIN_LY)));
  ArduinoX360.setStickRight(stick14(analogRead(PIN_RX)), stick14(analogRead(PIN_RY)));
  ArduinoX360.setLeftTrigger(trigger14(analogRead(PIN_TRIG_L)));
  ArduinoX360.setRightTrigger(trigger14(analogRead(PIN_TRIG_R)));

  ArduinoX360.send();
  delay(8);
}
