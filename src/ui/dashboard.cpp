#include "ui/dashboard.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kStartDeg = 135.0f;
constexpr float kSweepDeg = 270.0f;

float valueToRad(float value, float max) {
  const float v = fminf(fmaxf(value, 0.0f), max);
  return (kStartDeg + kSweepDeg * (v / max)) * DEG_TO_RAD;
}

}  // namespace

void Dashboard::begin() {
  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);

  drawGauge(speedGauge_);
  drawGauge(rpmGauge_);
  drawGaugeValue(speedGauge_, 0.0f, "km/h", true);
  drawGaugeValue(rpmGauge_, 0.0f, "rpm", true);

  tft_.fillCircle(speedGauge_.cx, speedGauge_.cy, 4, speedGauge_.accent);
  tft_.fillCircle(rpmGauge_.cx, rpmGauge_.cy, 4, rpmGauge_.accent);
}

void Dashboard::render(const Values& v) {
  const float speedNeedle = v.speedValid ? v.speedKmh : 0.0f;
  const float rpmNeedle = v.rpmValid ? v.rpm : 0.0f;

  // Erase old needles, repaint gauge faces, draw new needles.
  drawNeedle(speedGauge_, prevSpeed_, TFT_BLACK);
  drawNeedle(rpmGauge_, prevRpm_, TFT_BLACK);

  drawGauge(speedGauge_);
  drawGauge(rpmGauge_);

  drawNeedle(speedGauge_, speedNeedle, v.speedValid ? speedGauge_.accent : TFT_DARKGREY);
  drawNeedle(rpmGauge_, rpmNeedle, v.rpmValid ? rpmGauge_.accent : TFT_DARKGREY);

  tft_.fillCircle(speedGauge_.cx, speedGauge_.cy, 4, speedGauge_.accent);
  tft_.fillCircle(rpmGauge_.cx, rpmGauge_.cy, 4, rpmGauge_.accent);

  drawGaugeValue(speedGauge_, v.speedKmh, "km/h", v.speedValid);
  drawGaugeValue(rpmGauge_, v.rpm, "rpm", v.rpmValid);

  drawIndicators(v);
  drawBars(v);

  prevSpeed_ = speedNeedle;
  prevRpm_ = rpmNeedle;
}

void Dashboard::drawGauge(const Gauge& g) {
  tft_.drawCircle(g.cx, g.cy, g.r, TFT_DARKGREY);

  constexpr int kTicks = 10;
  for (int i = 0; i <= kTicks; ++i) {
    const float frac = (float)i / kTicks;
    const float rad = (kStartDeg + kSweepDeg * frac) * DEG_TO_RAD;
    const float tickValue = g.max * frac;
    const bool red = g.redline > 0.0f && tickValue >= g.redline;
    const uint16_t color = red ? TFT_RED : TFT_LIGHTGREY;

    const int rOuter = g.r - 1;
    const int rInner = g.r - ((i % 2 == 0) ? 11 : 6);
    tft_.drawLine(g.cx + (int)(cosf(rad) * rOuter), g.cy + (int)(sinf(rad) * rOuter),
                  g.cx + (int)(cosf(rad) * rInner), g.cy + (int)(sinf(rad) * rInner), color);

    if (i % 2 == 0) {
      const int rl = g.r - 20;
      char label[8];
      if (g.labelDiv >= 1000.0f) {
        snprintf(label, sizeof(label), "%.1f", tickValue / g.labelDiv);
      } else {
        snprintf(label, sizeof(label), "%d", (int)tickValue);
      }
      tft_.setTextDatum(MC_DATUM);
      tft_.setTextFont(1);
      tft_.setTextColor(color, TFT_BLACK);
      tft_.drawString(label, g.cx + (int)(cosf(rad) * rl), g.cy + (int)(sinf(rad) * rl));
    }
  }
}

void Dashboard::drawNeedle(const Gauge& g, float value, uint16_t color) {
  const float rad = valueToRad(value, g.max);
  const int tip = g.r - 13;
  tft_.drawWideLine(g.cx, g.cy, g.cx + (int)(cosf(rad) * tip), g.cy + (int)(sinf(rad) * tip),
                    3, color);
}

void Dashboard::drawGaugeValue(const Gauge& g, float value, const char* unit, bool valid) {
  char buf[24];
  if (!valid) {
    snprintf(buf, sizeof(buf), "-- %s", unit);
  } else if (g.labelDiv >= 1000.0f) {
    snprintf(buf, sizeof(buf), "%.1fk %s", value / g.labelDiv, unit);
  } else {
    snprintf(buf, sizeof(buf), "%d %s", (int)(value + 0.5f), unit);
  }
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  tft_.setTextColor(valid ? g.accent : TFT_DARKGREY, TFT_BLACK);
  tft_.drawString(buf, g.cx, g.cy + g.r + 12);
}

void Dashboard::drawIndicators(const Values& v) {
  struct Indicator {
    const char* label;
    int x;
    bool on;
    uint16_t color;
  };

  const Indicator items[] = {
      {"OIL", 8, v.oilWarning, TFT_RED},
      {"CHG", 58, v.chargeWarning, TFT_RED},
      {"BEAM", 104, v.highBeam, TFT_BLUE},
      {"<", 174, v.turnLeft, TFT_ORANGE},
      {">", 194, v.turnRight, TFT_ORANGE},
  };

  tft_.setTextFont(2);
  tft_.setTextDatum(TL_DATUM);
  for (const auto& it : items) {
    tft_.setTextColor(it.on ? it.color : TFT_DARKGREY, TFT_BLACK);
    tft_.drawString(it.label, it.x, 4);
  }
}

void Dashboard::drawBars(const Values& v) {
  auto bar = [&](int x, const char* label, float value, bool valid, float lo, float hi,
                 uint16_t color) {
    char text[24];
    if (valid) {
      snprintf(text, sizeof(text), "%s %d", label, (int)(value + 0.5f));
    } else {
      snprintf(text, sizeof(text), "%s --", label);
    }
    tft_.setTextFont(1);
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawString(text, x, 190);

    tft_.drawRect(x, 202, 140, 10, TFT_DARKGREY);
    if (!valid) {
      tft_.fillRect(x + 1, 203, 138, 8, TFT_BLACK);
      return;
    }
    const float frac = fminf(fmaxf((value - lo) / (hi - lo), 0.0f), 1.0f);
    const int w = (int)(140 * frac);
    tft_.fillRect(x + 1, 203, w, 8, color);
    if (w < 138) {
      tft_.fillRect(x + 1 + w, 203, 138 - w, 8, TFT_BLACK);
    }
  };

  bar(10, "TEMP C", v.coolantC, v.coolantValid, cfg::TEMP_MIN_C, cfg::TEMP_MAX_C,
      v.tempWarning ? TFT_RED : TFT_GREEN);
  bar(170, "FUEL %", v.fuelPct, v.fuelValid, cfg::FUEL_MIN_PCT, cfg::FUEL_MAX_PCT,
      v.fuelWarning ? TFT_RED : TFT_YELLOW);

  char buf[16];
  if (v.batteryValid) {
    snprintf(buf, sizeof(buf), "%.1fV", v.batteryV);
  } else {
    snprintf(buf, sizeof(buf), "--.-V");
  }
  const bool batteryOk =
      v.batteryValid && v.batteryV >= cfg::BATTERY_MIN_V && v.batteryV <= cfg::BATTERY_MAX_V;
  tft_.setTextFont(2);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(batteryOk ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft_.drawString(buf, cfg::SCREEN_W / 2, 226);
}
