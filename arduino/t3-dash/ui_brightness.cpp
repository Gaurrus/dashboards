#include "ui_brightness.h"

#include <cstdio>

#include "lvgl.h"
#include "ui_lvgl.h"
#include "ui_theme.h"

namespace {

using namespace ui::theme;

// Brightness is emulated with a black overlay: the Waveshare 7" backlight is
// wired to a switch-only IO expander pin and cannot be PWM-dimmed.
constexpr int BRIGHTNESS_SWIPE_ZONE_WIDTH = 100;
constexpr float BRIGHTNESS_SWIPE_RANGE_FRAC = 0.8f;
constexpr uint32_t BRIGHTNESS_INDICATOR_HIDE_MS = 1200;
constexpr int BRIGHTNESS_TRACK_WIDTH = 12;
constexpr int BRIGHTNESS_TRACK_HEIGHT = 150;

lv_obj_t* g_dimmer = nullptr;
lv_obj_t* g_brightnessZone = nullptr;
lv_obj_t* g_brightnessIndicator = nullptr;
lv_obj_t* g_brightnessValueLabel = nullptr;
lv_obj_t* g_brightnessTrack = nullptr;
lv_obj_t* g_brightnessFill = nullptr;
lv_timer_t* g_brightnessHideTimer = nullptr;

int g_brightness = 100;
int g_brightnessStartValue = 100;
lv_coord_t g_brightnessStartY = 0;
bool g_brightnessDragging = false;
uint32_t g_brightnessShownMs = 0;


void applyBrightness(int percent) {
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  g_brightness = percent;

  if (g_dimmer != nullptr) {
    const lv_opa_t opa = (lv_opa_t)((100 - g_brightness) * 255 / 100);
    lv_obj_set_style_bg_opa(g_dimmer, opa, 0);
  }
  if (g_brightnessFill != nullptr) {
    const int height = BRIGHTNESS_TRACK_HEIGHT * g_brightness / 100;
    lv_obj_set_height(g_brightnessFill, height);
    lv_obj_align(g_brightnessFill, LV_ALIGN_BOTTOM_MID, 0, 0);
  }
  if (g_brightnessValueLabel != nullptr) {
    char text[8];
    snprintf(text, sizeof(text), "%d%%", g_brightness);
    lv_label_set_text(g_brightnessValueLabel, text);
  }
}

void showBrightnessIndicator() {
  if (g_brightnessIndicator == nullptr) {
    return;
  }
  lv_obj_clear_flag(g_brightnessIndicator, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_brightnessIndicator);
  g_brightnessShownMs = lv_tick_get();
}

void brightnessEventCb(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    lv_indev_t* indev = lv_indev_get_act();
    if (indev == nullptr) {
      return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    g_brightnessDragging = true;
    g_brightnessStartY = p.y;
    g_brightnessStartValue = g_brightness;
    showBrightnessIndicator();
  } else if (code == LV_EVENT_PRESSING) {
    if (!g_brightnessDragging) {
      return;
    }
    lv_indev_t* indev = lv_indev_get_act();
    if (indev == nullptr) {
      return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    const int range =
        (int)(LV_VER_RES * BRIGHTNESS_SWIPE_RANGE_FRAC);
    const int delta = (int)(g_brightnessStartY - p.y) * 100 / range;
    applyBrightness(g_brightnessStartValue + delta);
    showBrightnessIndicator();
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    g_brightnessDragging = false;
  }
}

void brightnessHideTimerCb(lv_timer_t* /*timer*/) {
  if (g_brightnessIndicator == nullptr || g_brightnessDragging) {
    return;
  }
  if (lv_obj_has_flag(g_brightnessIndicator, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  if (lv_tick_elaps(g_brightnessShownMs) >= BRIGHTNESS_INDICATOR_HIDE_MS) {
    lv_obj_add_flag(g_brightnessIndicator, LV_OBJ_FLAG_HIDDEN);
  }
}

void createBrightnessControl() {
  lv_obj_t* top = lv_layer_top();

  g_dimmer = lv_obj_create(top);
  lv_obj_remove_style_all(g_dimmer);
  lv_obj_set_pos(g_dimmer, 0, 0);
  lv_obj_set_size(g_dimmer, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(g_dimmer, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(g_dimmer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(g_dimmer, 0, 0);
  lv_obj_clear_flag(g_dimmer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_dimmer, LV_OBJ_FLAG_CLICKABLE);

  g_brightnessIndicator = lv_obj_create(top);
  lv_obj_remove_style_all(g_brightnessIndicator);
  lv_obj_set_size(g_brightnessIndicator, 84, 240);
  lv_obj_align(g_brightnessIndicator, LV_ALIGN_RIGHT_MID, -16, 0);
  lv_obj_set_style_bg_color(g_brightnessIndicator, lv_color_hex(0x141A20), 0);
  lv_obj_set_style_bg_opa(g_brightnessIndicator, (lv_opa_t)230, 0);
  lv_obj_set_style_radius(g_brightnessIndicator, 12, 0);
  lv_obj_set_style_border_width(g_brightnessIndicator, 1, 0);
  lv_obj_set_style_border_color(g_brightnessIndicator, lv_color_hex(COL_TRACK), 0);
  lv_obj_clear_flag(g_brightnessIndicator, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_brightnessIndicator, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_brightnessIndicator, LV_OBJ_FLAG_HIDDEN);

  g_brightnessValueLabel =
      makeLabel(g_brightnessIndicator, "100%", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_align(g_brightnessValueLabel, LV_ALIGN_TOP_MID, 0, 12);

  g_brightnessTrack = lv_obj_create(g_brightnessIndicator);
  lv_obj_remove_style_all(g_brightnessTrack);
  lv_obj_set_size(g_brightnessTrack, BRIGHTNESS_TRACK_WIDTH, BRIGHTNESS_TRACK_HEIGHT);
  lv_obj_align(g_brightnessTrack, LV_ALIGN_BOTTOM_MID, 0, -16);
  lv_obj_set_style_bg_color(g_brightnessTrack, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_bg_opa(g_brightnessTrack, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_brightnessTrack, BRIGHTNESS_TRACK_WIDTH / 2, 0);
  lv_obj_clear_flag(g_brightnessTrack, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_brightnessTrack, LV_OBJ_FLAG_CLICKABLE);

  g_brightnessFill = lv_obj_create(g_brightnessTrack);
  lv_obj_remove_style_all(g_brightnessFill);
  lv_obj_set_width(g_brightnessFill, BRIGHTNESS_TRACK_WIDTH);
  lv_obj_set_height(g_brightnessFill, BRIGHTNESS_TRACK_HEIGHT);
  lv_obj_align(g_brightnessFill, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(g_brightnessFill, lv_color_hex(COL_CYAN), 0);
  lv_obj_set_style_bg_opa(g_brightnessFill, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_brightnessFill, BRIGHTNESS_TRACK_WIDTH / 2, 0);
  lv_obj_clear_flag(g_brightnessFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_brightnessFill, LV_OBJ_FLAG_CLICKABLE);

  g_brightnessZone = lv_obj_create(top);
  lv_obj_remove_style_all(g_brightnessZone);
  lv_obj_set_size(g_brightnessZone, BRIGHTNESS_SWIPE_ZONE_WIDTH, LV_VER_RES);
  lv_obj_align(g_brightnessZone, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_opa(g_brightnessZone, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(g_brightnessZone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_brightnessZone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_brightnessZone, brightnessEventCb, LV_EVENT_ALL, nullptr);

  g_brightnessHideTimer = lv_timer_create(brightnessHideTimerCb, 50, nullptr);
}


}  // namespace

namespace ui {
namespace internal {

void brightnessInit() {
  createBrightnessControl();
  applyBrightness(g_brightness);
}

}  // namespace internal
}  // namespace ui

namespace ui {

void setBrightness(int percent) {
  applyBrightness(percent);
}

int brightness() {
  return g_brightness;
}

}  // namespace ui
