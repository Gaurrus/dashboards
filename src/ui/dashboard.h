#pragma once

#include <TFT_eSPI.h>

#include "config.h"
#include "logic/model.h"

class Dashboard {
 public:
  void begin();
  void render(const Values& v);

 private:
  struct Gauge {
    int cx;
    int cy;
    int r;
    float max;
    float redline;   // <= 0 means no redline
    uint16_t accent;
    float labelDiv;  // divisor for tick labels (1 or 1000)
  };

  void drawGauge(const Gauge& g);
  void drawNeedle(const Gauge& g, float value, uint16_t color);
  void drawGaugeValue(const Gauge& g, float value, const char* unit, bool valid);
  void drawIndicators(const Values& v);
  void drawBars(const Values& v);

  TFT_eSPI tft_;

  const Gauge speedGauge_{78, 104, 62, cfg::SPEED_MAX_KMH, -1.0f, TFT_CYAN, 1.0f};
  const Gauge rpmGauge_{242, 104, 62, cfg::RPM_MAX, cfg::RPM_REDLINE, TFT_ORANGE, 1000.0f};

  float prevSpeed_ = 0.0f;
  float prevRpm_ = 0.0f;
};
