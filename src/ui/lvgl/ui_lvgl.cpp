#include "ui_lvgl.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "lvgl.h"

namespace {

constexpr uint32_t COL_BG = 0x0B0E11;
constexpr uint32_t COL_TRACK = 0x2A2F35;
constexpr uint32_t COL_TEXT = 0xE6E6E6;
constexpr uint32_t COL_DIM = 0x6B7280;
constexpr uint32_t COL_CYAN = 0x22D3EE;
constexpr uint32_t COL_ORANGE = 0xF59E0B;
constexpr uint32_t COL_RED = 0xEF4444;
constexpr uint32_t COL_GREEN = 0x22C55E;
constexpr uint32_t COL_YELLOW = 0xEAB308;
constexpr uint32_t COL_BLUE = 0x3B82F6;
constexpr uint32_t COL_TICK = 0x9CA3AF;
constexpr uint32_t COL_TURN = 0x2ECC71;

constexpr int ARROW_W = 54;
constexpr int ARROW_H = 32;
constexpr int ICON_W = 30;
constexpr int ICON_H = 30;
constexpr int SIDE_SIZE = 210;
constexpr int SPEED_SIZE = 315;
constexpr int CLOCK_SIZE = 210;

struct Gauge {
  lv_obj_t* arc = nullptr;
  lv_obj_t* value = nullptr;
  int32_t max = 0;
  bool div1000 = false;
};

Gauge g_speed;
Gauge g_rpm;

// lv_line_set_points() keeps the pointer, so the point arrays must live forever.
constexpr int kMaxTicks = 128;
lv_point_t g_tickPoints[kMaxTicks][2];
int g_tickPointCount = 0;

lv_obj_t* g_tempBar = nullptr;
lv_obj_t* g_tempLabel = nullptr;
lv_obj_t* g_fuelBar = nullptr;
lv_obj_t* g_fuelLabel = nullptr;
lv_obj_t* g_voltage = nullptr;
lv_obj_t* g_odo = nullptr;

lv_obj_t* g_iconOil = nullptr;
lv_obj_t* g_iconChg = nullptr;
lv_obj_t* g_iconGlow = nullptr;
lv_obj_t* g_iconBeam = nullptr;
lv_color_t g_oilIconBuf[ICON_W * ICON_H];
lv_color_t g_chgIconBuf[ICON_W * ICON_H];
lv_color_t g_glowIconBuf[ICON_W * ICON_H];
lv_color_t g_beamIconBuf[ICON_W * ICON_H];

lv_obj_t* g_turnLeft = nullptr;
lv_obj_t* g_turnRight = nullptr;
lv_color_t g_leftArrowBuf[ARROW_W * ARROW_H];
lv_color_t g_rightArrowBuf[ARROW_W * ARROW_H];

lv_obj_t* g_clockTime = nullptr;
lv_obj_t* g_clockSeconds = nullptr;

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

void addTick(lv_obj_t* parent, int size, int cx, int cy, float angleDeg, int rInner,
             int rOuter, int width, uint32_t color) {
  if (g_tickPointCount >= kMaxTicks) {
    return;
  }

  const float rad = angleDeg * 3.14159265f / 180.0f;
  lv_point_t* points = g_tickPoints[g_tickPointCount++];
  points[0].x = (lv_coord_t)(cx + cosf(rad) * rInner);
  points[0].y = (lv_coord_t)(cy + sinf(rad) * rInner);
  points[1].x = (lv_coord_t)(cx + cosf(rad) * rOuter);
  points[1].y = (lv_coord_t)(cy + sinf(rad) * rOuter);

  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_size(line, size, size);
  lv_obj_set_pos(line, 0, 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

Gauge createGauge(lv_obj_t* parent, int x, int y, int size, const char* title,
                  const char* unit, int32_t max, uint32_t accent, float redlineFrac,
                  float tickStep, float labelStep, bool div1000, int arcWidth,
                  const lv_font_t* valueFont, const lv_font_t* titleFont) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, size, size);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* arc = lv_arc_create(box);
  lv_obj_set_size(arc, size, size);
  lv_obj_center(arc);
  lv_arc_set_bg_angles(arc, 135, 45);
  lv_arc_set_range(arc, 0, max);
  lv_arc_set_value(arc, 0);
  lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(COL_TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(accent), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

  if (redlineFrac > 0.0f) {
    lv_obj_t* red = lv_arc_create(box);
    lv_obj_set_size(red, size, size);
    lv_obj_center(red);
    lv_arc_set_bg_angles(red, (int32_t)(135.0f + 270.0f * redlineFrac), 45);
    lv_arc_set_range(red, 0, 100);
    lv_arc_set_value(red, 0);
    lv_obj_set_style_arc_width(red, arcWidth, LV_PART_MAIN);
    lv_obj_set_style_arc_color(red, lv_color_hex(COL_RED), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(red, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_opa(red, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(red, LV_OBJ_FLAG_CLICKABLE);
  }

  const int tickCount = (int)((float)max / tickStep + 0.5f);
  const int ratio = (tickStep > 0.0f) ? (int)(labelStep / tickStep + 0.5f) : 1;
  const int rOuter = size / 2 - arcWidth / 2 - 2;

  for (int i = 0; i <= tickCount; ++i) {
    const float tickValue = i * tickStep;
    const float frac = tickValue / (float)max;
    const float angle = 135.0f + 270.0f * frac;
    const bool major = ratio > 0 && (i % ratio) == 0;
    const bool red = redlineFrac > 0.0f && frac >= redlineFrac;
    const uint32_t color = red ? COL_RED : (major ? COL_TICK : COL_DIM);

    addTick(box, size, size / 2, size / 2, angle, rOuter - (major ? 13 : 7), rOuter,
            major ? 3 : 2, color);

    if (major) {
      const float rad = angle * 3.14159265f / 180.0f;
      const int rLabel = rOuter - 24;
      const int dx = (int)(cosf(rad) * rLabel);
      const int dy = (int)(sinf(rad) * rLabel);

      char text[16];
      if (div1000) {
        snprintf(text, sizeof(text), "%.1f", tickValue / 1000.0f);
      } else {
        snprintf(text, sizeof(text), "%d", (int)(tickValue + 0.5f));
      }
      lv_obj_t* tick = makeLabel(box, text, &lv_font_montserrat_14,
                                 red ? COL_RED : COL_DIM);
      lv_obj_align(tick, LV_ALIGN_CENTER, dx, dy);
    }
  }

  lv_obj_t* titleLabel = makeLabel(box, title, titleFont, COL_DIM);
  lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -(int)(size * 0.27f));

  lv_obj_t* valueLabel = makeLabel(box, "--", valueFont, accent);
  lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* unitLabel = makeLabel(box, unit, &lv_font_montserrat_20, COL_DIM);
  lv_obj_align(unitLabel, LV_ALIGN_CENTER, 0, (int)(size * 0.17f));

  Gauge g;
  g.arc = arc;
  g.value = valueLabel;
  g.max = max;
  g.div1000 = div1000;
  return g;
}

void createClock(lv_obj_t* parent, int x, int y, int size) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, size, size);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

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

  lv_obj_t* caption = makeLabel(box, "TIME", &lv_font_montserrat_14, COL_DIM);
  lv_obj_align(caption, LV_ALIGN_CENTER, 0, -62);

  g_clockTime = makeLabel(box, "--:--", &lv_font_montserrat_48, COL_TEXT);
  lv_obj_align(g_clockTime, LV_ALIGN_CENTER, 0, -6);

  g_clockSeconds = makeLabel(box, "--", &lv_font_montserrat_20, COL_DIM);
  lv_obj_align(g_clockSeconds, LV_ALIGN_CENTER, 0, 44);
}

void updateGauge(Gauge& g, float value, bool valid, uint32_t accent) {
  int32_t arcValue = valid ? (int32_t)(value + 0.5f) : 0;
  if (arcValue < 0) {
    arcValue = 0;
  }
  if (arcValue > g.max) {
    arcValue = g.max;
  }
  lv_arc_set_value(g.arc, arcValue);
  lv_obj_set_style_arc_color(g.arc, lv_color_hex(valid ? accent : COL_TRACK),
                             LV_PART_INDICATOR);

  char text[24];
  if (!valid) {
    snprintf(text, sizeof(text), "--");
  } else if (g.div1000) {
    snprintf(text, sizeof(text), "%.1f", value / 1000.0f);
  } else {
    snprintf(text, sizeof(text), "%d", (int)(value + 0.5f));
  }
  lv_label_set_text(g.value, text);
}

void setIconState(lv_obj_t* icon, bool on) {
  lv_obj_set_style_img_opa(icon, on ? LV_OPA_COVER : LV_OPA_20, 0);
}

void iconFillRect(lv_obj_t* canvas, int x, int y, int w, int h, int radius,
                  uint32_t color) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = lv_color_hex(color);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = radius;
  dsc.border_width = 0;
  lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

void iconStrokeRect(lv_obj_t* canvas, int x, int y, int w, int h, int radius,
                    int borderWidth, uint32_t color) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_TRANSP;
  dsc.radius = radius;
  dsc.border_width = borderWidth;
  dsc.border_color = lv_color_hex(color);
  dsc.border_opa = LV_OPA_COVER;
  lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

void iconPolygon(lv_obj_t* canvas, const lv_point_t* points, uint32_t count,
                 uint32_t color) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = lv_color_hex(color);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = 0;
  dsc.border_width = 0;
  lv_canvas_draw_polygon(canvas, points, count, &dsc);
}

void iconLine(lv_obj_t* canvas, const lv_point_t* points, uint32_t count, int width,
              uint32_t color) {
  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  dsc.color = lv_color_hex(color);
  dsc.width = width;
  dsc.opa = LV_OPA_COVER;
  dsc.round_start = 1;
  dsc.round_end = 1;
  lv_canvas_draw_line(canvas, points, count, &dsc);
}

lv_obj_t* createIconCanvas(lv_obj_t* parent, int x, int y, lv_color_t* buffer) {
  lv_obj_t* canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buffer, ICON_W, ICON_H, LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(canvas, lv_color_hex(COL_BG), LV_OPA_COVER);
  lv_obj_set_pos(canvas, x, y);
  lv_obj_set_style_img_opa(canvas, LV_OPA_20, 0);
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  return canvas;
}

void drawOilIcon(lv_obj_t* canvas) {
  const uint32_t col = COL_RED;
  static const lv_point_t spout[] = {{7, 14}, {11, 14}, {5, 5}, {2, 7}};
  static const lv_point_t handle[] = {{10, 14}, {14, 9}, {18, 14}};

  iconPolygon(canvas, spout, 4, col);
  iconFillRect(canvas, 7, 14, 17, 11, 3, col);
  iconLine(canvas, handle, 3, 2, col);
  iconFillRect(canvas, 6, 26, 19, 2, 1, col);
}

void drawChargeIcon(lv_obj_t* canvas) {
  const uint32_t col = COL_RED;
  static const lv_point_t bolt[] = {{16, 7}, {9, 17}, {13, 17},
                                    {11, 25}, {20, 14}, {15, 14}, {18, 7}};

  iconStrokeRect(canvas, 4, 9, 20, 13, 2, 2, col);
  iconFillRect(canvas, 25, 13, 3, 5, 1, col);
  iconPolygon(canvas, bolt, 7, col);
}

void drawGlowIcon(lv_obj_t* canvas) {
  static const lv_point_t coil[] = {{7, 6},  {23, 9},  {7, 12}, {23, 15},
                                    {7, 18}, {23, 21}, {7, 24}};
  iconLine(canvas, coil, 7, 3, COL_YELLOW);
}

void drawBeamIcon(lv_obj_t* canvas) {
  const uint32_t col = COL_BLUE;
  static const lv_point_t lamp[] = {{3, 8}, {11, 5}, {15, 8}, {15, 22}, {11, 25}, {3, 22}};
  static const lv_point_t ray1[] = {{18, 9}, {27, 6}};
  static const lv_point_t ray2[] = {{18, 15}, {28, 15}};
  static const lv_point_t ray3[] = {{18, 21}, {27, 24}};

  iconPolygon(canvas, lamp, 6, col);
  iconLine(canvas, ray1, 2, 2, col);
  iconLine(canvas, ray2, 2, 2, col);
  iconLine(canvas, ray3, 2, 2, col);
}

lv_obj_t* createTurnArrow(lv_obj_t* parent, int x, int y, bool left,
                          lv_color_t* buffer) {
  static const lv_point_t leftHead[] = {{7, 16}, {47, 2}, {47, 30}};
  static const lv_point_t rightHead[] = {{46, 16}, {6, 2}, {6, 30}};

  lv_obj_t* canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buffer, ARROW_W, ARROW_H, LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(canvas, lv_color_hex(COL_BG), LV_OPA_COVER);

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = lv_color_hex(COL_TURN);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = 0;
  dsc.border_width = 0;

  lv_canvas_draw_polygon(canvas, left ? leftHead : rightHead, 3, &dsc);

  lv_obj_set_pos(canvas, x, y);
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  return canvas;
}

void setBlinking(lv_obj_t* arrow, bool active, bool blinkOn) {
  if (active && blinkOn) {
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t* createBar(lv_obj_t* parent, int x, int y, int width, int32_t lo, int32_t hi,
                    uint32_t color) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_size(bar, width, 16);
  lv_obj_set_pos(bar, x, y);
  lv_bar_set_range(bar, lo, hi);
  lv_bar_set_value(bar, lo, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(COL_TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 8, LV_PART_INDICATOR);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  return bar;
}

void updateBar(lv_obj_t* bar, lv_obj_t* label, const char* name, float value,
               bool valid, bool warn, uint32_t color) {
  char text[32];
  if (valid) {
    snprintf(text, sizeof(text), "%s %d", name, (int)(value + 0.5f));
  } else {
    snprintf(text, sizeof(text), "%s --", name);
  }
  lv_label_set_text(label, text);

  lv_bar_set_value(bar, valid ? (int32_t)(value + 0.5f) : 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(valid ? (warn ? COL_RED : color) : COL_TRACK),
                            LV_PART_INDICATOR);
}

void updateClock() {
  static int lastSecond = -1;

  const time_t now = time(nullptr);
  struct tm* tmv = localtime(&now);
  if (tmv == nullptr || now < 1600000000) {
    if (lastSecond != -1) {
      lv_label_set_text(g_clockTime, "--:--");
      lv_label_set_text(g_clockSeconds, "--");
      lastSecond = -1;
    }
    return;
  }

  if (tmv->tm_sec == lastSecond) {
    return;
  }
  lastSecond = tmv->tm_sec;

  char text[8];
  snprintf(text, sizeof(text), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
  lv_label_set_text(g_clockTime, text);

  snprintf(text, sizeof(text), "%02d", tmv->tm_sec);
  lv_label_set_text(g_clockSeconds, text);
}

}  // namespace

namespace ui {

void init() {
  g_tickPointCount = 0;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Status icons are grouped in the middle of the top row.
  g_iconOil = createIconCanvas(screen, 310, 4, g_oilIconBuf);
  drawOilIcon(g_iconOil);

  g_iconChg = createIconCanvas(screen, 360, 4, g_chgIconBuf);
  drawChargeIcon(g_iconChg);

  g_iconGlow = createIconCanvas(screen, 410, 4, g_glowIconBuf);
  drawGlowIcon(g_iconGlow);

  g_iconBeam = createIconCanvas(screen, 460, 4, g_beamIconBuf);
  drawBeamIcon(g_iconBeam);

  // Classic VW turn signal arrows: left on the left edge, right on the right edge.
  g_turnLeft = createTurnArrow(screen, 20, 2, true, g_leftArrowBuf);
  g_turnRight = createTurnArrow(screen, 726, 2, false, g_rightArrowBuf);

  // Left: clock. Center: large speedometer. Right: tachometer.
  createClock(screen, 14, 125, CLOCK_SIZE);

  g_speed = createGauge(screen, 242, 75, SPEED_SIZE, "SPEED", "km/h", 180, COL_CYAN,
                        -1.0f, 10.0f, 20.0f, false, 22, &lv_font_montserrat_48,
                        &lv_font_montserrat_24);

  g_rpm = createGauge(screen, 576, 125, SIDE_SIZE, "RPM x1000", "rpm", 7000, COL_ORANGE,
                      5500.0f / 7000.0f, 500.0f, 1000.0f, true, 14,
                      &lv_font_montserrat_32, &lv_font_montserrat_20);

  g_voltage = makeLabel(screen, "--.-V", &lv_font_montserrat_32, COL_GREEN);
  lv_obj_align(g_voltage, LV_ALIGN_CENTER, 0, 168);

  g_odo = makeLabel(screen, "0.0 km", &lv_font_montserrat_20, COL_DIM);
  lv_obj_align(g_odo, LV_ALIGN_CENTER, 0, 202);

  g_tempLabel = makeLabel(screen, "TEMP --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_tempLabel, 14, 348);
  g_tempBar = createBar(screen, 14, 376, 210, 40, 130, COL_GREEN);

  g_fuelLabel = makeLabel(screen, "FUEL --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_fuelLabel, 576, 348);
  g_fuelBar = createBar(screen, 576, 376, 210, 0, 100, COL_YELLOW);
}

void update(const Values& v) {
  updateGauge(g_speed, v.speedKmh, v.speedValid, COL_CYAN);
  updateGauge(g_rpm, v.rpm, v.rpmValid, COL_ORANGE);

  updateBar(g_tempBar, g_tempLabel, "TEMP C", v.coolantC, v.coolantValid, v.tempWarning,
            COL_GREEN);
  updateBar(g_fuelBar, g_fuelLabel, "FUEL %", v.fuelPct, v.fuelValid, v.fuelWarning,
            COL_YELLOW);

  char text[24];
  if (v.batteryValid) {
    snprintf(text, sizeof(text), "%.1fV", v.batteryV);
  } else {
    snprintf(text, sizeof(text), "--.-V");
  }
  lv_label_set_text(g_voltage, text);
  const bool batteryOk = v.batteryValid && v.batteryV >= 11.0f && v.batteryV <= 15.0f;
  lv_obj_set_style_text_color(g_voltage,
                              lv_color_hex(v.batteryValid ? (batteryOk ? COL_GREEN : COL_RED)
                                                          : COL_DIM),
                              0);

  snprintf(text, sizeof(text), "%.1f km", v.odometerKm);
  lv_label_set_text(g_odo, text);

  setIconState(g_iconOil, v.oilWarning);
  setIconState(g_iconChg, v.chargeWarning);
  setIconState(g_iconGlow, v.glowPlug);
  setIconState(g_iconBeam, v.highBeam);

  const bool blinkOn = ((lv_tick_get() / 350U) & 1U) == 0U;
  setBlinking(g_turnLeft, v.turnLeft, blinkOn);
  setBlinking(g_turnRight, v.turnRight, blinkOn);

  updateClock();
}

}  // namespace ui
