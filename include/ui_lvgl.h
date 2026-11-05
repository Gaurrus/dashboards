#pragma once

#include "dash_types.h"

// LVGL 8.4 dashboard, designed for 800x480.
// init() requires lv_init() to have been called already.
namespace ui {

void init();
void update(const Values& v);
void skipBoot();

// Screen brightness in percent (0..100). Implemented as a black overlay so it
// works on panels whose backlight is switch-only (Waveshare 7").
void setBrightness(int percent);
int brightness();

}  // namespace ui
