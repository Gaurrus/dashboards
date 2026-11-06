#pragma once

#include <cmath>

#include "dash_types.h"

// Stress-test demo data used by the PC simulator and the hardware bring-up build.
// - Speed/rpm/fuel hold their base value, sweep to the extreme every 3 s and back.
// - Temp holds 90, falls to the scale bottom, rises to max and returns.
// - Turn signals blink in hazard mode constantly.
// - Status lamps light one by one every 2 s, ping-pong along the orbit.
namespace demo {

inline float wrap(float x, float period) {
  return x - period * floorf(x / period);
}

inline float smooth01(float x) {
  if (x < 0.0f) {
    x = 0.0f;
  }
  if (x > 1.0f) {
    x = 1.0f;
  }
  return x * x * (3.0f - 2.0f * x);
}

// 0..0.6 s hold, 0.6..1.6 s out, 1.6..2.6 s back, 2.6..3.0 s hold.
inline float sweep3(float t) {
  const float p = wrap(t, 3.0f);
  if (p < 0.6f) {
    return 0.0f;
  }
  if (p < 1.6f) {
    return smooth01((p - 0.6f) / 1.0f);
  }
  if (p < 2.6f) {
    return smooth01((2.6f - p) / 1.0f);
  }
  return 0.0f;
}

inline Values make(float t, bool ignitionOn = true) {
  Values v;
  v.ignitionOn = ignitionOn;

  v.speedValid = true;
  v.rpmValid = true;
  v.coolantValid = true;
  v.fuelValid = true;
  v.batteryValid = true;

  // Speed 60 -> 150 -> 60, tacho 1500 -> max -> 1500, fuel full -> 0 -> full.
  const float sweep = sweep3(t);
  v.speedKmh = 60.0f + 90.0f * sweep;
  v.rpm = 1500.0f + 4000.0f * sweep;
  v.fuelPct = 100.0f - 100.0f * sweep;

  // Temp: hold 90 -> fall to scale bottom (40) -> rise to max (130) -> back to 90.
  const float tp = wrap(t, 3.0f);
  if (tp < 0.6f) {
    v.coolantC = 90.0f;
  } else if (tp < 1.2f) {
    v.coolantC = 90.0f - 50.0f * smooth01((tp - 0.6f) / 0.6f);
  } else if (tp < 2.4f) {
    v.coolantC = 40.0f + 90.0f * smooth01((tp - 1.2f) / 1.2f);
  } else {
    v.coolantC = 130.0f - 40.0f * smooth01((tp - 2.4f) / 0.6f);
  }

  v.batteryV = 13.8f + 0.2f * sinf(t * 0.9f);
  v.odometerKm = 1234.5f + t * v.speedKmh / 3600.0f;

  // Hazard mode: both turn signals blink constantly.
  v.turnLeft = true;
  v.turnRight = true;

  // Status lamps light one by one with a 2 s step, ping-pong along the orbit:
  // oil -> charge -> brake -> glow -> fog -> beam, then back.
  constexpr int kLampSpan = 5;
  const int steps = (int)(t / 2.0f);
  int lamp = steps % (2 * kLampSpan);
  if (lamp > kLampSpan) {
    lamp = 2 * kLampSpan - lamp;
  }
  v.oilWarning = (lamp == 0);
  v.chargeWarning = (lamp == 1);
  v.parkingBrake = (lamp == 2);
  v.glowPlug = (lamp == 3);
  v.fogLight = (lamp == 4);
  if (lamp == kLampSpan) {
    // The low/high beam pair shares one orbit position: alternate green/blue.
    if ((steps / (2 * kLampSpan)) % 2 == 0) {
      v.lowBeam = true;
    } else {
      v.highBeam = true;
    }
  }

  v.tempWarning = v.coolantC >= 110.0f;
  v.fuelWarning = v.fuelPct <= 12.0f;

  return v;
}

}  // namespace demo
