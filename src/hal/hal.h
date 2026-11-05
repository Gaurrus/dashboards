#pragma once
#include <Arduino.h>

// Physical signals as measured by the hardware layer.
// `valid` is false when the sensor is missing, out of range or not ready.
struct RawInputs {
  float speedKmh = 0.0f;
  bool speedValid = false;

  float rpm = 0.0f;
  bool rpmValid = false;

  float coolantC = 0.0f;
  bool coolantValid = false;

  float fuelPct = 0.0f;
  bool fuelValid = false;

  float batteryV = 0.0f;
  bool batteryValid = false;

  bool oilWarning = false;
  bool chargeWarning = false;
  bool highBeam = false;
  bool turnLeft = false;
  bool turnRight = false;
  bool glowPlug = false;
};

class Hal {
 public:
  void begin();
  void poll(RawInputs& out);

 private:
#ifndef SIMULATION
  uint32_t rpmLastCount_ = 0;
  uint32_t rpmWindowMs_ = 0;
  float rpmMeasured_ = 0.0f;
  bool rpmReady_ = false;
#endif
};
