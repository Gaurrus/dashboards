#include "ui_clock.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "lvgl.h"
#include "ui_lvgl.h"
#include "ui_theme.h"

namespace {

using namespace ui::theme;

lv_obj_t* g_clockTime = nullptr;
uint32_t g_clockSeconds = 0;
uint32_t g_clockLastTick = 0;
bool g_clockChanged = false;
lv_obj_t* g_clockSet = nullptr;
lv_obj_t* g_clockHourRoller = nullptr;
lv_obj_t* g_clockMinuteRoller = nullptr;
lv_obj_t* g_hourLine = nullptr;
lv_obj_t* g_minuteLine = nullptr;
lv_obj_t* g_secondLine = nullptr;
lv_point_t g_hourHand[2];
lv_point_t g_minuteHand[2];
lv_point_t g_secondHand[2];


void setHandPoints(lv_point_t* points, int size, float angleDeg, int length) {
  const float rad = angleDeg * 3.14159265f / 180.0f;
  const int center = size / 2;
  points[0].x = center;
  points[0].y = center;
  points[1].x = center + (lv_coord_t)(sinf(rad) * length);
  points[1].y = center - (lv_coord_t)(cosf(rad) * length);
}

void createClock(lv_obj_t* parent, int x, int y, int size) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, size, size);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* ring = lv_obj_create(box);
  lv_obj_remove_style_all(ring);
  lv_obj_set_size(ring, size - 4, size - 4);
  lv_obj_center(ring);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(ring, 6, 0);
  lv_obj_set_style_border_color(ring, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

  const int dotRadius = size / 2 - 18;
  for (int i = 0; i < 12; ++i) {
    const float rad = (i * 30.0f) * 3.14159265f / 180.0f;
    const int dx = (int)(cosf(rad) * dotRadius);
    const int dy = (int)(sinf(rad) * dotRadius);

    lv_obj_t* dot = lv_obj_create(box);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, (i % 3 == 0) ? 8 : 5, (i % 3 == 0) ? 8 : 5);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(COL_DIM), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_CENTER, dx, dy);
  }

  // Digital block below the centre.
  lv_obj_t* panel = lv_obj_create(box);
  lv_obj_set_size(panel, 108, 38);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x11161B), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(panel, 6, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);

  g_clockTime = makeLabel(panel, "--:--", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(g_clockTime, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(g_clockTime, lv_color_hex(0x11161B), 0);
  lv_obj_set_style_bg_opa(g_clockTime, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(g_clockTime, 4, 0);
  lv_obj_set_style_pad_ver(g_clockTime, 1, 0);

  auto createHand = [&](uint32_t color, int width) {
    lv_obj_t* line = lv_line_create(box);
    lv_obj_set_size(line, size, size);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return line;
  };

  g_hourLine = createHand(COL_TEXT, 5);
  g_minuteLine = createHand(COL_TEXT, 3);
  g_secondLine = createHand(COL_ORANGE, 2);

  setHandPoints(g_hourHand, size, 0.0f, 42);
  setHandPoints(g_minuteHand, size, 0.0f, 62);
  setHandPoints(g_secondHand, size, 0.0f, 72);
  lv_line_set_points(g_hourLine, g_hourHand, 2);
  lv_line_set_points(g_minuteLine, g_minuteHand, 2);
  lv_line_set_points(g_secondLine, g_secondHand, 2);

  // Keep the digital block above the hands so it is always readable.
  lv_obj_move_foreground(panel);

  lv_obj_t* hub = lv_obj_create(box);
  lv_obj_remove_style_all(hub);
  lv_obj_set_size(hub, 10, 10);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
  lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
}


void advanceClock() {
  const uint32_t now = lv_tick_get();
  const uint32_t elapsed = now - g_clockLastTick;
  if (elapsed >= 1000U) {
    g_clockSeconds = (g_clockSeconds + elapsed / 1000U) % 86400U;
    g_clockLastTick += (elapsed / 1000U) * 1000U;
  }
}

void updateClock() {
  static uint32_t lastSecond = 0xFFFFFFFFU;

  advanceClock();
  if (g_clockSeconds == lastSecond) {
    return;
  }
  lastSecond = g_clockSeconds;

  lv_obj_clear_flag(g_hourLine, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_minuteLine, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_secondLine, LV_OBJ_FLAG_HIDDEN);

  const int hour = (int)(g_clockSeconds / 3600U);
  const int minute = (int)((g_clockSeconds / 60U) % 60U);
  const int second = (int)(g_clockSeconds % 60U);

  const float hourAngle = ((hour % 12) + minute / 60.0f) * 30.0f;
  const float minuteAngle = (minute + second / 60.0f) * 6.0f;
  const float secondAngle = second * 6.0f;

  setHandPoints(g_hourHand, ui::internal::CLOCK_SIZE, hourAngle, 42);
  setHandPoints(g_minuteHand, ui::internal::CLOCK_SIZE, minuteAngle, 62);
  setHandPoints(g_secondHand, ui::internal::CLOCK_SIZE, secondAngle, 72);
  lv_line_set_points(g_hourLine, g_hourHand, 2);
  lv_line_set_points(g_minuteLine, g_minuteHand, 2);
  lv_line_set_points(g_secondLine, g_secondHand, 2);

  char text[8];
  snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
  lv_label_set_text(g_clockTime, text);
}

void hideClockSettings() {
  if (g_clockSet != nullptr) {
    lv_obj_add_flag(g_clockSet, LV_OBJ_FLAG_HIDDEN);
  }
}

void showClockSettings() {
  if (g_clockSet == nullptr) {
    return;
  }
  const uint32_t seconds = ui::clockSeconds();
  lv_roller_set_selected(g_clockHourRoller, (uint16_t)(seconds / 3600U), LV_ANIM_OFF);
  lv_roller_set_selected(g_clockMinuteRoller, (uint16_t)((seconds / 60U) % 60U),
                         LV_ANIM_OFF);
  lv_obj_clear_flag(g_clockSet, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_clockSet);
}

void clockBackdropCb(lv_event_t* /*e*/) {
  hideClockSettings();
}

void clockCancelCb(lv_event_t* /*e*/) {
  hideClockSettings();
}

void clockSetCb(lv_event_t* /*e*/) {
  const int hour = (int)lv_roller_get_selected(g_clockHourRoller);
  const int minute = (int)lv_roller_get_selected(g_clockMinuteRoller);
  ui::setClock(hour, minute, 0);
  hideClockSettings();
}

lv_obj_t* createClockSettings() {
  static char hourOptions[24 * 3 + 1];
  static char minuteOptions[60 * 3 + 1];

  int pos = 0;
  for (int i = 0; i < 24 && pos < (int)sizeof(hourOptions) - 4; ++i) {
    pos += snprintf(hourOptions + pos, sizeof(hourOptions) - pos, "%02d\n", i);
  }
  if (pos > 0) {
    hourOptions[pos - 1] = '\0';
  }
  pos = 0;
  for (int i = 0; i < 60 && pos < (int)sizeof(minuteOptions) - 4; ++i) {
    pos += snprintf(minuteOptions + pos, sizeof(minuteOptions) - pos, "%02d\n", i);
  }
  if (pos > 0) {
    minuteOptions[pos - 1] = '\0';
  }

  lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(backdrop);
  lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(backdrop, 0, 0);
  lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(backdrop, (lv_opa_t)170, 0);
  lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(backdrop, clockBackdropCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* panel = lv_obj_create(backdrop);
  lv_obj_remove_style_all(panel);
  lv_obj_set_size(panel, 440, 300);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x141A20), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, 16, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(COL_TRACK), 0);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = makeLabel(panel, "SET TIME", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

  g_clockHourRoller = lv_roller_create(panel);
  lv_obj_set_size(g_clockHourRoller, 120, 130);
  lv_obj_set_pos(g_clockHourRoller, 60, 62);
  lv_roller_set_options(g_clockHourRoller, hourOptions, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(g_clockHourRoller, 3);
  lv_roller_set_selected(g_clockHourRoller, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_clockHourRoller, lv_color_hex(0x0B0E11), 0);
  lv_obj_set_style_bg_opa(g_clockHourRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_clockHourRoller, 1, 0);
  lv_obj_set_style_border_color(g_clockHourRoller, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_radius(g_clockHourRoller, 10, 0);
  lv_obj_set_style_text_font(g_clockHourRoller, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(g_clockHourRoller, lv_color_hex(COL_DIM), 0);
  lv_obj_set_style_bg_color(g_clockHourRoller, lv_color_hex(COL_TRACK),
                            LV_PART_SELECTED);
  lv_obj_set_style_text_color(g_clockHourRoller, lv_color_hex(COL_TEXT),
                              LV_PART_SELECTED);
  lv_obj_set_style_text_font(g_clockHourRoller, &lv_font_montserrat_24,
                             LV_PART_SELECTED);

  g_clockMinuteRoller = lv_roller_create(panel);
  lv_obj_set_size(g_clockMinuteRoller, 120, 130);
  lv_obj_set_pos(g_clockMinuteRoller, 260, 62);
  lv_roller_set_options(g_clockMinuteRoller, minuteOptions, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(g_clockMinuteRoller, 3);
  lv_roller_set_selected(g_clockMinuteRoller, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_clockMinuteRoller, lv_color_hex(0x0B0E11), 0);
  lv_obj_set_style_bg_opa(g_clockMinuteRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_clockMinuteRoller, 1, 0);
  lv_obj_set_style_border_color(g_clockMinuteRoller, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_radius(g_clockMinuteRoller, 10, 0);
  lv_obj_set_style_text_font(g_clockMinuteRoller, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(g_clockMinuteRoller, lv_color_hex(COL_DIM), 0);
  lv_obj_set_style_bg_color(g_clockMinuteRoller, lv_color_hex(COL_TRACK),
                            LV_PART_SELECTED);
  lv_obj_set_style_text_color(g_clockMinuteRoller, lv_color_hex(COL_TEXT),
                              LV_PART_SELECTED);
  lv_obj_set_style_text_font(g_clockMinuteRoller, &lv_font_montserrat_24,
                             LV_PART_SELECTED);

  lv_obj_t* colon = makeLabel(panel, ":", &lv_font_montserrat_32, COL_TEXT);
  lv_obj_set_pos(colon, 210, 102);

  lv_obj_t* hint = makeLabel(panel, "SWIPE TO CHANGE", &lv_font_montserrat_12, COL_DIM);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 198);

  lv_obj_t* cancel = lv_btn_create(panel);
  lv_obj_set_size(cancel, 120, 46);
  lv_obj_set_pos(cancel, 60, 232);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_radius(cancel, 10, 0);
  lv_obj_add_event_cb(cancel, clockCancelCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* cancelLabel = makeLabel(cancel, "CANCEL", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_center(cancelLabel);

  lv_obj_t* set = lv_btn_create(panel);
  lv_obj_set_size(set, 120, 46);
  lv_obj_set_pos(set, 260, 232);
  lv_obj_set_style_bg_color(set, lv_color_hex(COL_CYAN), 0);
  lv_obj_set_style_radius(set, 10, 0);
  lv_obj_add_event_cb(set, clockSetCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* setLabel = makeLabel(set, "SET", &lv_font_montserrat_20, COL_BG);
  lv_obj_center(setLabel);

  lv_obj_move_foreground(backdrop);
  return backdrop;
}



}  // namespace

namespace ui {
namespace internal {

void clockInit(lv_obj_t* parent) {
  const time_t now = time(nullptr);
  if (now >= 1600000000) {
    struct tm* tmv = localtime(&now);
    if (tmv != nullptr) {
      setClock(tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
    } else {
      setClock(0, 0, 0);
    }
  } else {
    setClock(0, 0, 0);
  }
  createClock(parent, CLOCK_X, CLOCK_Y, ui::internal::CLOCK_SIZE);
}

void clockSettingsInit() {
  g_clockSet = createClockSettings();
}

void clockUpdate() {
  updateClock();
}

void clockShowSettings() {
  showClockSettings();
}

void clockHideSettings() {
  hideClockSettings();
}

}  // namespace internal
}  // namespace ui

namespace ui {

void setClock(int hour, int minute, int second) {
  hour %= 24;
  if (hour < 0) {
    hour += 24;
  }
  if (minute < 0) {
    minute = 0;
  } else if (minute > 59) {
    minute = 59;
  }
  if (second < 0) {
    second = 0;
  } else if (second > 59) {
    second = 59;
  }
  g_clockSeconds = (uint32_t)(hour * 3600 + minute * 60 + second);
  g_clockLastTick = lv_tick_get();
  g_clockChanged = true;
}

uint32_t clockSeconds() {
  advanceClock();
  return g_clockSeconds;
}

bool consumeClockChanged() {
  const bool changed = g_clockChanged;
  g_clockChanged = false;
  return changed;
}

}  // namespace ui
