#pragma once

#include <cmath>

#include "dash_types.h"

// Animated demo data used by the PC simulator and the hardware bring-up build.
namespace demo {

inline float wrap(float x, float period) {
  return x - period * floorf(x / period);
}

inline Values make(float t) {
  Values v;
  v.speedValid = true;
  v.rpmValid = true;
  v.coolantValid = true;
  v.fuelValid = true;
  v.batteryValid = true;

  v.speedKmh = 90.0f + 70.0f * sinf(t * 0.50f);

  const float targetRpm = 900.0f + v.speedKmh * 28.0f + 400.0f * sinf(t * 1.7f);
  v.rpm = targetRpm < 0.0f ? 0.0f : targetRpm;

  v.coolantC = 88.0f + 14.0f * sinf(t * 0.18f);
  v.fuelPct = 62.0f + 18.0f * sinf(t * 0.07f);
  v.batteryV = 13.8f + 0.25f * sinf(t * 0.9f);

  v.odometerKm = 1234.5f + t * v.speedKmh / 3600.0f;

  v.tempWarning = v.coolantC >= 110.0f;
  v.fuelWarning = v.fuelPct <= 12.0f;

  v.oilWarning = wrap(t, 20.0f) < 2.0f;
  v.chargeWarning = wrap(t, 30.0f) > 28.0f;
  v.lowBeam = wrap(t, 12.0f) < 5.0f;
  v.highBeam = !v.lowBeam;
  v.fogLight = wrap(t, 9.0f) < 4.0f;
  v.turnLeft = wrap(t, 8.0f) < 1.5f;
  v.turnRight = wrap(t, 11.0f) < 1.5f;
  v.glowPlug = wrap(t, 15.0f) < 3.0f;
  v.parkingBrake = wrap(t, 13.0f) < 4.0f;

  return v;
}

}  // namespace demo
