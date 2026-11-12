#pragma once

#include "dash_types.h"
#include "lvgl.h"

// Fourth layout: two circular wells with needles (speed left, RPM right),
// status lamps around the well centres, TEMP/FUEL between the wells.
namespace ui {
namespace internal {

void designDInit(lv_obj_t* parent);
void designDSetVisible(bool visible);
void designDUpdate(const Values& v);

}  // namespace internal
}  // namespace ui
