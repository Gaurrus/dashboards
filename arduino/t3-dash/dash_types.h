#pragma once

// Portable dashboard values shared by the firmware and the PC simulator.
// No Arduino/ESP32 dependencies here.
struct Values {
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

  float odometerKm = 0.0f;

  bool ignitionOn = true;  // false while the key is off: show the goodbye screen

  bool oilWarning = false;
  bool chargeWarning = false;
  bool highBeam = false;
  bool lowBeam = false;
  bool fogLight = false;
  bool turnLeft = false;
  bool turnRight = false;
  bool glowPlug = false;  // diesel glow plugs heating
  bool parkingBrake = false;
  bool brakeFault = false;  // low brake fluid or failed brake circuit

  bool tempWarning = false;
  bool fuelWarning = false;
};
