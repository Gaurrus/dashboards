#pragma once
#include <Arduino.h>

// Resistance -> physical value calibration.
// Tables are sorted by resistance ascending; values may be descending (NTC).
namespace calib {

struct Point {
  float x;  // resistance, ohm
  float y;  // physical value (deg C or percent)
};

float interpolate(const Point* table, size_t count, float x);

// R = Rref * Vnode / (Vexcitation - Vnode)
float resistanceFromDivider(float vNode, float vExc, float rRef);

float temperatureC(float resistanceOhm);
float fuelPercent(float resistanceOhm);

}  // namespace calib
