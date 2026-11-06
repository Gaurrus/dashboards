#pragma once

#include "lvgl.h"

namespace ui {
namespace internal {

void byeInit(lv_obj_t* parent);
bool byeUpdate(bool ignitionOn);
bool byeActive();

}  // namespace internal
}  // namespace ui
