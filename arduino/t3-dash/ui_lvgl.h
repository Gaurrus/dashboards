#pragma once

#include "dash_types.h"

// LVGL 8.4 dashboard, designed for 800x480.
// init() requires lv_init() to have been called already.
namespace ui {

void init();
void update(const Values& v);

}  // namespace ui
