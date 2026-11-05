#include "calibration.h"

#include <cmath>

#include "config.h"

namespace calib {
namespace {

// Typical VW waterboxer coolant NTC curve.
// VERIFY against your sender with a multimeter in boiling water / ice.
const Point kWaterboxerNtc[] = {
    {120.0f, 115.0f},
    {140.0f, 110.0f},
    {180.0f, 100.0f},
    {240.0f, 90.0f},
    {330.0f, 80.0f},
    {600.0f, 60.0f},
    {1100.0f, 40.0f},
    {2500.0f, 20.0f},
    {5000.0f, 0.0f},
    {9000.0f, -15.0f},
};

// Typical air-cooled CHT sender curve (resistive variant).
// If yours is a thermocouple, this table does not apply — use MAX6675.
const Point kAirCooledCht[] = {
    {80.0f, 200.0f},
    {150.0f, 150.0f},
    {400.0f, 100.0f},
    {1500.0f, 50.0f},
    {5000.0f, 20.0f},
};

// PLACEHOLDER T3 fuel sender curve (resistance decreases with fuel level).
// Measure your sender at empty/full and replace these points.
// If your sender works the opposite way, swap the y values.
const Point kFuelT3[] = {
    {10.0f, 100.0f},
    {20.0f, 85.0f},
    {35.0f, 65.0f},
    {55.0f, 40.0f},
    {70.0f, 15.0f},
    {80.0f, 0.0f},
};

template <size_t N>
float lookup(const Point (&table)[N], float x) {
  return interpolate(table, N, x);
}

}  // namespace

float interpolate(const Point* table, size_t count, float x) {
  if (count == 0) {
    return NAN;
  }
  if (x <= table[0].x) {
    return table[0].y;
  }
  if (x >= table[count - 1].x) {
    return table[count - 1].y;
  }
  for (size_t i = 1; i < count; ++i) {
    if (x <= table[i].x) {
      const float x0 = table[i - 1].x;
      const float x1 = table[i].x;
      const float y0 = table[i - 1].y;
      const float y1 = table[i].y;
      const float t = (x - x0) / (x1 - x0);
      return y0 + t * (y1 - y0);
    }
  }
  return table[count - 1].y;
}

float resistanceFromDivider(float vNode, float vExc, float rRef) {
  if (vExc <= 0.0f || vNode <= 0.0f || vNode >= vExc) {
    return NAN;
  }
  return rRef * vNode / (vExc - vNode);
}

float temperatureC(float resistanceOhm) {
  return cfg::TEMP_USE_CHT ? lookup(kAirCooledCht, resistanceOhm)
                           : lookup(kWaterboxerNtc, resistanceOhm);
}

float fuelPercent(float resistanceOhm) {
  return lookup(kFuelT3, resistanceOhm);
}

}  // namespace calib
