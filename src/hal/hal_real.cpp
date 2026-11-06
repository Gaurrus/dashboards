#include "hal/hal.h"

#include <Adafruit_ADS1X15.h>
#include <TinyGPSPlus.h>
#include <Wire.h>

#include "calibration.h"
#include "config.h"

// Wiring notes
// ------------
// ADS1115 (I2C, powered from 3.3V):
//   A0 <- battery divider node (100k / 22k, plus 3.3V zener + 100nF)
//   A1 <- coolant/CHT sender node (sender to GND, 2.2k to excitation)
//   A2 <- fuel sender node (sender to GND, 150R to excitation)
//   A3 <- excitation rail (3.3V) for ratiometric resistance math
//
// PC817 optocouplers for 12V inputs (RPM, oil, charge, beam, turns, glow,
// handbrake, brake fault):
//   emitter -> GND, collector -> GPIO, 12V via 1k/2W + reverse diode.
//   Active input pulls the GPIO LOW.
//   NOTE: GPIO34..39 have NO internal pull-ups, so the RPM, ignition and brake
//   collectors need an external 10k resistor to 3.3V. The other inputs use
//   INPUT_PULLUP.
// Brake inputs are make-to-ground switches (handbrake lever, reservoir float,
// pressure differential). Feed each one through its own opto channel:
//   +12V -> 1k -> LED anode, LED cathode -> switch line -> GND when active.
// This keeps the factory BRAKE lamp working; the ESP32 only monitors it.
// RPM from the ignition coil needs extra protection: 10k/2W + 1N4007 + 15V zener.
//
// GPS: NEO-6M / ATGM336H TX -> ESP32 GPIO16 (RX2), 9600 baud.
//
// TODO: if the factory air-cooled CHT sender is a thermocouple, add a MAX6675
//       and bypass the resistive path.

namespace {

Adafruit_ADS1115 ads;
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

volatile uint32_t rpmPulseCount = 0;
volatile uint32_t rpmLastPulseUs = 0;

void IRAM_ATTR onRpmPulse() {
  const uint32_t now = micros();
  if (now - rpmLastPulseUs > 500) {  // debounce, max ~2 kHz
    rpmLastPulseUs = now;
    rpmPulseCount++;
  }
}

float readVolts(int channel) {
  return ads.computeVolts(ads.readADC_SingleEnded(channel));
}

}  // namespace

void Hal::begin() {
  Wire.begin(cfg::I2C_SDA, cfg::I2C_SCL);
  ads.begin();
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);

  pinMode(cfg::PIN_OIL, INPUT_PULLUP);
  pinMode(cfg::PIN_CHARGE, INPUT_PULLUP);
  pinMode(cfg::PIN_BEAM, INPUT_PULLUP);
  pinMode(cfg::PIN_LOW_BEAM, INPUT_PULLUP);
  pinMode(cfg::PIN_TURN_L, INPUT_PULLUP);
  pinMode(cfg::PIN_TURN_R, INPUT_PULLUP);
  pinMode(cfg::PIN_GLOW, INPUT_PULLUP);

  // Input-only pins: rely on external 10k pull-ups to 3.3V.
  pinMode(cfg::PIN_RPM_IN, INPUT);
  pinMode(cfg::PIN_IGNITION, INPUT);
  pinMode(cfg::PIN_BRAKE, INPUT);
  pinMode(cfg::PIN_BRAKE_FAULT, INPUT);
  attachInterrupt(digitalPinToInterrupt(cfg::PIN_RPM_IN), onRpmPulse, FALLING);

  pinMode(cfg::PIN_BUZZER, OUTPUT);
  digitalWrite(cfg::PIN_BUZZER, LOW);

  gpsSerial.begin(cfg::GPS_BAUD, SERIAL_8N1, cfg::GPS_RX, cfg::GPS_TX);

  rpmWindowMs_ = millis();
  noInterrupts();
  rpmLastCount_ = rpmPulseCount;
  interrupts();
}

void Hal::poll(RawInputs& out) {
  const float vBatteryNode = readVolts(0);
  const float vTempNode = readVolts(1);
  const float vFuelNode = readVolts(2);
  const float vExcitation = readVolts(3);

  // --- Battery voltage ---
  out.batteryV =
      vBatteryNode * (cfg::R_BAT_TOP + cfg::R_BAT_BOTTOM) / cfg::R_BAT_BOTTOM;
  out.batteryValid = out.batteryV > 6.0f && out.batteryV < 20.0f;

  // --- Coolant / CHT ---
  if (vExcitation > 0.2f && vTempNode > 0.0f && vTempNode < vExcitation) {
    const float resistance =
        calib::resistanceFromDivider(vTempNode, vExcitation, cfg::R_REF_TEMP);
    if (resistance > 20.0f && resistance < 100000.0f) {
      out.coolantC = calib::temperatureC(resistance);
      out.coolantValid = true;
    }
  }

  // --- Fuel level ---
  if (vExcitation > 0.2f && vFuelNode > 0.0f && vFuelNode < vExcitation) {
    const float resistance =
        calib::resistanceFromDivider(vFuelNode, vExcitation, cfg::R_REF_FUEL);
    if (resistance > 2.0f && resistance < 1000.0f) {
      out.fuelPct = calib::fuelPercent(resistance);
      out.fuelValid = true;
    }
  }

  // --- RPM from ignition pulses ---
  noInterrupts();
  const uint32_t count = rpmPulseCount;
  interrupts();

  const uint32_t nowMs = millis();
  const uint32_t elapsed = nowMs - rpmWindowMs_;
  if (elapsed >= 250) {
    const float seconds = elapsed / 1000.0f;
    const uint32_t delta = count - rpmLastCount_;
    const float revsPerSecond = (delta / seconds) / cfg::RPM_PULSES_PER_REV;
    rpmMeasured_ = revsPerSecond * 60.0f;
    rpmLastCount_ = count;
    rpmWindowMs_ = nowMs;
    rpmReady_ = true;
  }
  out.rpm = rpmMeasured_;
  out.rpmValid = rpmReady_;

  // --- Ignition (active-low via optocoupler) ---
  out.ignitionOn = digitalRead(cfg::PIN_IGNITION) == LOW;

  // --- Warning / indicator inputs (active-low) ---
  out.oilWarning = digitalRead(cfg::PIN_OIL) == LOW;
  out.chargeWarning = digitalRead(cfg::PIN_CHARGE) == LOW;
  out.highBeam = digitalRead(cfg::PIN_BEAM) == LOW;
  out.lowBeam = digitalRead(cfg::PIN_LOW_BEAM) == LOW;
  out.turnLeft = digitalRead(cfg::PIN_TURN_L) == LOW;
  out.turnRight = digitalRead(cfg::PIN_TURN_R) == LOW;
  out.glowPlug = digitalRead(cfg::PIN_GLOW) == LOW;
  out.parkingBrake = digitalRead(cfg::PIN_BRAKE) == LOW;
  out.brakeFault = digitalRead(cfg::PIN_BRAKE_FAULT) == LOW;

  // --- GPS speed ---
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  if (gps.speed.isValid() && gps.speed.age() < 2000) {
    out.speedKmh = gps.speed.kmph();
    out.speedValid = true;
  }
}
