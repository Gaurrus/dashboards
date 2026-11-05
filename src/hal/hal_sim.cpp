#include "hal/hal.h"

#include <cmath>

#include "config.h"

namespace {

float potFraction(int pin) {
  return (float)analogRead(pin) / 4095.0f;
}

}  // namespace

void Hal::begin() {
  analogReadResolution(12);

  pinMode(cfg::PIN_BTN_OIL, INPUT_PULLUP);
  pinMode(cfg::PIN_BTN_BEAM, INPUT_PULLUP);
  pinMode(cfg::PIN_BTN_TURN_L, INPUT_PULLUP);

  pinMode(cfg::PIN_BUZZER, OUTPUT);
  digitalWrite(cfg::PIN_BUZZER, LOW);
}

void Hal::poll(RawInputs& out) {
  out.speedKmh = potFraction(cfg::PIN_ADC_SPEED) * cfg::SPEED_MAX_KMH;
  out.speedValid = true;

  out.rpm = potFraction(cfg::PIN_ADC_RPM) * cfg::RPM_MAX;
  out.rpmValid = true;

  out.coolantC =
      cfg::TEMP_MIN_C + potFraction(cfg::PIN_ADC_TEMP) * (cfg::TEMP_MAX_C - cfg::TEMP_MIN_C);
  out.coolantValid = true;

  out.fuelPct = potFraction(cfg::PIN_ADC_FUEL) * cfg::FUEL_MAX_PCT;
  out.fuelValid = true;

  // Simulated charging voltage with a slow ripple.
  out.batteryV = 13.8f + 0.12f * sinf((float)millis() / 3000.0f * 2.0f * PI);
  out.batteryValid = true;

  // Buttons are active-low (INPUT_PULLUP).
  out.oilWarning = digitalRead(cfg::PIN_BTN_OIL) == LOW;
  out.highBeam = digitalRead(cfg::PIN_BTN_BEAM) == LOW;
  out.turnLeft = digitalRead(cfg::PIN_BTN_TURN_L) == LOW;

  out.chargeWarning = false;
  out.turnRight = false;
  out.glowPlug = false;
}
