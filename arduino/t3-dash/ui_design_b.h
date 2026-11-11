#pragma once

#include "dash_types.h"
#include "lvgl.h"

// Alternate dashboard layout, toggled from the gear button in the bottom-left
// corner. Kept in its own module so the two designs can be built and updated
// independently.
namespace ui {
namespace internal {

void designBInit(lv_obj_t* parent);
void designBSetVisible(bool visible);
void designBUpdate(const Values& v);

}  // namespace internal
}  // namespace ui
