#pragma once
#include <Arduino.h>

namespace cfg {

// ===================== Simulation (Wokwi) pins =====================
constexpr int PIN_ADC_SPEED = 34;
constexpr int PIN_ADC_RPM   = 35;
constexpr int PIN_ADC_TEMP  = 36;
constexpr int PIN_ADC_FUEL  = 39;

constexpr int PIN_BTN_OIL    = 25;
constexpr int PIN_BTN_BEAM   = 26;
constexpr int PIN_BTN_TURN_L = 27;

// Shared between simulation and real board.
constexpr int PIN_BUZZER = 33;

// ===================== Real board pins =====================
// TFT_eSPI pins are configured through build_flags in platformio.ini:
// CS=15, DC=2, RST=4, MOSI=23, SCK=18, MISO=19, BL=32

// GPIO34..39 are input-only and have no internal pull-ups: add an external 10k to 3.3V.
constexpr int PIN_RPM_IN = 34;  // PC817 collector, active-low pulse

constexpr int PIN_OIL     = 25;
constexpr int PIN_CHARGE  = 26;
constexpr int PIN_BEAM    = 27;
constexpr int PIN_LOW_BEAM = 5;
constexpr int PIN_TURN_L  = 13;
constexpr int PIN_TURN_R  = 14;
constexpr int PIN_GLOW    = 12;  // diesel glow plug indicator (active-low via optocoupler)

constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

constexpr int GPS_RX = 16;
constexpr int GPS_TX = 17;
constexpr uint32_t GPS_BAUD = 9600;

// ===================== Gauge ranges =====================
constexpr float SPEED_MAX_KMH = 180.0f;
constexpr float RPM_MAX       = 7000.0f;
constexpr float RPM_REDLINE   = 5500.0f;

constexpr float TEMP_MIN_C  = 40.0f;
constexpr float TEMP_MAX_C  = 130.0f;
constexpr float TEMP_WARN_C = 110.0f;

constexpr float FUEL_MIN_PCT  = 0.0f;
constexpr float FUEL_MAX_PCT  = 100.0f;
constexpr float FUEL_WARN_PCT = 12.0f;

constexpr float BATTERY_MIN_V = 11.0f;
constexpr float BATTERY_MAX_V = 15.0f;

// ===================== Sensor hardware =====================
// Pulses on the coil negative per crank revolution:
// 4-cylinder 4-stroke => 2 ignition events per revolution.
constexpr float RPM_PULSES_PER_REV = 2.0f;

// Reference resistors for the resistive divider measurement.
constexpr float R_REF_TEMP = 2200.0f;   // ohm, pull-up to excitation
constexpr float R_REF_FUEL = 150.0f;    // ohm, pull-up to excitation

// Battery voltage divider (Vnode = Vbat * R_BAT_BOTTOM / (TOP + BOTTOM)).
constexpr float R_BAT_TOP    = 100000.0f;
constexpr float R_BAT_BOTTOM = 22000.0f;

// true  -> air-cooled engine, CHT sender curve
// false -> waterboxer, coolant NTC curve
constexpr bool TEMP_USE_CHT = false;

// ===================== Display =====================
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

// ===================== Filter time constants, seconds =====================
constexpr float TAU_FAST = 0.12f;
constexpr float TAU_SLOW = 0.60f;

}  // namespace cfg
