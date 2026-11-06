#pragma once

#include "lvgl.h"

namespace ui {
namespace internal {

constexpr int CLOCK_SIZE = 210;
constexpr int CLOCK_X = 14;
constexpr int CLOCK_Y = 125;

void clockInit(lv_obj_t* parent);
void clockSettingsInit();
void clockUpdate();
void clockShowSettings();
void clockHideSettings();

}  // namespace internal
}  // namespace ui
