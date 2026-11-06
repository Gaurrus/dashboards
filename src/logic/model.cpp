#include "logic/model.h"

#include <cmath>

#include "config.h"

namespace {

void applyFilter(bool valid, float next, float alpha, bool& seeded, float& value,
                 bool& outValid) {
  outValid = valid;
  if (!valid) {
    return;
  }
  if (!seeded) {
    value = next;
    seeded = true;
    return;
  }
  value += alpha * (next - value);
}

}  // namespace

void DashboardModel::update(const RawInputs& raw, float dt) {
  const float aFast = 1.0f - expf(-dt / cfg::TAU_FAST);
  const float aSlow = 1.0f - expf(-dt / cfg::TAU_SLOW);

  applyFilter(raw.speedValid, raw.speedKmh, aFast, speedSeeded_, v_.speedKmh, v_.speedValid);
  applyFilter(raw.rpmValid, raw.rpm, aFast, rpmSeeded_, v_.rpm, v_.rpmValid);
  applyFilter(raw.coolantValid, raw.coolantC, aSlow, coolantSeeded_, v_.coolantC,
              v_.coolantValid);
  applyFilter(raw.fuelValid, raw.fuelPct, aSlow, fuelSeeded_, v_.fuelPct, v_.fuelValid);
  applyFilter(raw.batteryValid, raw.batteryV, aSlow, batterySeeded_, v_.batteryV,
              v_.batteryValid);

  v_.ignitionOn = raw.ignitionOn;
  v_.oilWarning = raw.oilWarning;
  v_.chargeWarning = raw.chargeWarning;
  v_.highBeam = raw.highBeam;
  v_.lowBeam = raw.lowBeam;
  v_.turnLeft = raw.turnLeft;
  v_.turnRight = raw.turnRight;
  v_.glowPlug = raw.glowPlug;
  v_.parkingBrake = raw.parkingBrake;
  v_.brakeFault = raw.brakeFault;

  v_.tempWarning = v_.coolantValid && v_.coolantC >= cfg::TEMP_WARN_C;
  v_.fuelWarning = v_.fuelValid && v_.fuelPct <= cfg::FUEL_WARN_PCT;

  if (raw.speedValid && dt > 0.0f && dt < 1.0f) {
    v_.odometerKm += v_.speedKmh * dt / 3600.0f;
  }
}
