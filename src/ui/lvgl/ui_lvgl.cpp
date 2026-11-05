#include "ui_lvgl.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "icons.h"
#include "lvgl.h"
#include "vw_gif.h"

namespace {

constexpr uint32_t COL_BG = 0x0B0E11;
constexpr uint32_t COL_TRACK = 0x2A2F35;
constexpr uint32_t COL_TEXT = 0xE6E6E6;
constexpr uint32_t COL_DIM = 0x6B7280;
constexpr uint32_t COL_CYAN = 0x22D3EE;
constexpr uint32_t COL_ORANGE = 0xF59E0B;
constexpr uint32_t COL_RED = 0xEF4444;
constexpr uint32_t COL_GREEN = 0x22C55E;
constexpr uint32_t COL_DARK_GREEN = 0x15803D;
constexpr uint32_t COL_YELLOW = 0xEAB308;
constexpr uint32_t COL_BLUE = 0x3B82F6;
constexpr uint32_t COL_TICK = 0x9CA3AF;
constexpr uint32_t COL_TURN = 0x2ECC71;

constexpr int ARROW_W = 54;
constexpr int ARROW_H = 32;
constexpr int SIDE_SIZE = 210;
constexpr int SPEED_SIZE = 315;
constexpr int CLOCK_SIZE = 210;

struct Gauge {
  lv_obj_t* arc = nullptr;
  lv_obj_t* value = nullptr;
  int32_t max = 0;
  bool div1000 = false;
  int segmentCount = 0;
  float segmentStep = 0.0f;
  lv_obj_t* segments[80] = {};
};

Gauge g_speed;
Gauge g_rpm;

// lv_line_set_points() keeps the pointer, so the point arrays must live forever.
constexpr int kMaxTicks = 128;
lv_point_t g_tickPoints[kMaxTicks][2];
int g_tickPointCount = 0;

lv_obj_t* g_tempGradient = nullptr;
lv_obj_t* g_tempCover = nullptr;
lv_obj_t* g_tempOutline = nullptr;
lv_obj_t* g_tempLabel = nullptr;
lv_obj_t* g_fuelGradient = nullptr;
lv_obj_t* g_fuelCover = nullptr;
lv_obj_t* g_fuelOutline = nullptr;
lv_obj_t* g_fuelLabel = nullptr;
lv_obj_t* g_voltage = nullptr;
lv_obj_t* g_odo = nullptr;

lv_obj_t* g_iconOil = nullptr;
lv_obj_t* g_iconChg = nullptr;
lv_obj_t* g_iconGlow = nullptr;
lv_obj_t* g_iconBeam = nullptr;
lv_obj_t* g_iconLowBeam = nullptr;
lv_obj_t* g_turnLeft = nullptr;
lv_obj_t* g_turnRight = nullptr;
lv_color_t g_leftArrowBuf[ARROW_W * ARROW_H];
lv_color_t g_rightArrowBuf[ARROW_W * ARROW_H];

lv_obj_t* g_clockTime = nullptr;
lv_obj_t* g_hourLine = nullptr;
lv_obj_t* g_minuteLine = nullptr;
lv_obj_t* g_secondLine = nullptr;
lv_point_t g_hourHand[2];
lv_point_t g_minuteHand[2];
lv_point_t g_secondHand[2];

constexpr uint32_t BOOT_SPLASH_MS = 1800;
constexpr uint32_t BOOT_UP_MS = 900;
constexpr uint32_t BOOT_HOLD_MS = 300;
constexpr uint32_t BOOT_DOWN_MS = 1000;
constexpr uint32_t BOOT_TOTAL_MS =
    BOOT_SPLASH_MS + BOOT_UP_MS + BOOT_HOLD_MS + BOOT_DOWN_MS;

lv_obj_t* g_splash = nullptr;
bool g_bootStarted = false;
bool g_bootSkipped = false;
uint32_t g_bootStartMs = 0;
bool g_selfTestActive = false;

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

uint32_t lerpColor(uint32_t from, uint32_t to, float t) {
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }
  const int r1 = (from >> 16) & 0xFF;
  const int g1 = (from >> 8) & 0xFF;
  const int b1 = from & 0xFF;
  const int r2 = (to >> 16) & 0xFF;
  const int g2 = (to >> 8) & 0xFF;
  const int b2 = to & 0xFF;
  const int r = r1 + (int)((r2 - r1) * t);
  const int g = g1 + (int)((g2 - g1) * t);
  const int b = b1 + (int)((b2 - b1) * t);
  return (uint32_t)((r << 16) | (g << 8) | b);
}

uint32_t rpmColor(float value) {
  if (value <= 3000.0f) {
    return COL_DARK_GREEN;
  }
  if (value >= 5000.0f) {
    return COL_RED;
  }
  if (value < 3500.0f) {
    return lerpColor(COL_DARK_GREEN, COL_GREEN, (value - 3000.0f) / 500.0f);
  }
  if (value < 4000.0f) {
    return lerpColor(COL_GREEN, COL_YELLOW, (value - 3500.0f) / 500.0f);
  }
  if (value < 4500.0f) {
    return lerpColor(COL_YELLOW, COL_ORANGE, (value - 4000.0f) / 500.0f);
  }
  return lerpColor(COL_ORANGE, COL_RED, (value - 4500.0f) / 500.0f);
}

Gauge createGauge(lv_obj_t* parent, int x, int y, int size, const char* title,
                  const char* unit, int32_t max, uint32_t accent, float redlineFrac,
                  float tickStep, float labelStep, bool div1000, int arcWidth,
                  const lv_font_t* valueFont, const lv_font_t* titleFont,
                  const lv_font_t* unitFont, bool segmentedFill = false,
                  float segStart = 0.0f, float segEnd = 0.0f, uint32_t segFrom = 0,
                  uint32_t segTo = 0, float segStep = 0.0f,
                  uint32_t (*colorFn)(float) = nullptr) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, size, size);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

  Gauge g;
  g.max = max;
  g.div1000 = div1000;

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
  g.arc = arc;

  if (segmentedFill) {
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
  }

  if (!segmentedFill && redlineFrac > 0.0f) {
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

  if (segmentedFill && segStep > 0.0f) {
    const int total = (int)((float)max / segStep + 0.5f);
    for (int i = 0; i < total && g.segmentCount < 80; ++i) {
      const float startValue = i * segStep;
      const float endValue = fminf((i + 1) * segStep, (float)max);
      const float startAngle = 135.0f + 270.0f * (startValue / (float)max);
      const float endAngle = 135.0f + 270.0f * (endValue / (float)max);
      const float midValue = (startValue + endValue) * 0.5f;

      uint32_t color = segTo;
      if (colorFn != nullptr) {
        color = colorFn(midValue);
      } else if (midValue <= segStart) {
        color = segFrom;
      } else if (midValue < segEnd) {
        color = lerpColor(segFrom, segTo, (midValue - segStart) / (segEnd - segStart));
      }

      lv_obj_t* segment = lv_arc_create(box);
      lv_obj_set_size(segment, size, size);
      lv_obj_center(segment);
      lv_arc_set_bg_angles(segment, (int32_t)startAngle, (int32_t)endAngle);
      lv_obj_set_style_arc_width(segment, arcWidth, LV_PART_MAIN);
      lv_obj_set_style_arc_color(segment, lv_color_hex(color), LV_PART_MAIN);
      lv_obj_set_style_arc_rounded(segment, true, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(segment, LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_opa(segment, LV_OPA_TRANSP, LV_PART_KNOB);
      lv_obj_clear_flag(segment, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
      g.segments[g.segmentCount++] = segment;
    }
    g.segmentStep = segStep;
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
        snprintf(text, sizeof(text), "%d", (int)(tickValue / 1000.0f + 0.5f));
      } else {
        snprintf(text, sizeof(text), "%d", (int)(tickValue + 0.5f));
      }
      lv_obj_t* tick = makeLabel(box, text, &lv_font_montserrat_14,
                                 red ? COL_RED : COL_DIM);
      lv_obj_align(tick, LV_ALIGN_CENTER, dx, dy);
    }
  }

  if (title != nullptr && title[0] != '\0') {
    lv_obj_t* titleLabel = makeLabel(box, title, titleFont, COL_DIM);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -(int)(size * 0.22f));
  }

  lv_obj_t* valueLabel = makeLabel(box, "--", valueFont, accent);
  lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, 0);

  if (unit != nullptr && unit[0] != '\0') {
    lv_obj_t* unitLabel = makeLabel(box, unit, unitFont, COL_DIM);
    lv_obj_align(unitLabel, LV_ALIGN_CENTER, 0, (int)(size * 0.17f));
  }

  g.value = valueLabel;
  return g;
}

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

void updateGauge(Gauge& g, float value, bool valid, uint32_t accent) {
  int32_t arcValue = valid ? (int32_t)(value + 0.5f) : 0;
  if (arcValue < 0) {
    arcValue = 0;
  }
  if (arcValue > g.max) {
    arcValue = g.max;
  }
  if (g.segmentCount > 0) {
    int visible = 0;
    if (valid && g.segmentStep > 0.0f) {
      visible = (int)(value / g.segmentStep);
    }
    if (visible < 0) {
      visible = 0;
    }
    if (visible > g.segmentCount) {
      visible = g.segmentCount;
    }
    for (int i = 0; i < g.segmentCount; ++i) {
      if (i < visible) {
        lv_obj_clear_flag(g.segments[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(g.segments[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
  } else {
    lv_arc_set_value(g.arc, arcValue);
    lv_obj_set_style_arc_color(g.arc, lv_color_hex(valid ? accent : COL_TRACK),
                               LV_PART_INDICATOR);
  }

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

lv_obj_t* createStatusIcon(lv_obj_t* parent, const lv_img_dsc_t* src, int x,
                           uint32_t color) {
  lv_obj_t* icon = lv_img_create(parent);
  lv_img_set_src(icon, src);
  lv_obj_set_pos(icon, x, 4);
  lv_obj_set_style_img_recolor(icon, lv_color_hex(color), 0);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
  lv_obj_set_style_img_opa(icon, LV_OPA_20, 0);
  return icon;
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

lv_obj_t* createGradientSegment(lv_obj_t* parent, int x, int width, uint32_t from,
                                 uint32_t to) {
  lv_obj_t* segment = lv_obj_create(parent);
  lv_obj_remove_style_all(segment);
  lv_obj_set_pos(segment, x, 0);
  lv_obj_set_size(segment, width, 16);
  lv_obj_set_style_bg_color(segment, lv_color_hex(from), 0);
  lv_obj_set_style_bg_grad_color(segment, lv_color_hex(to), 0);
  lv_obj_set_style_bg_grad_dir(segment, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, 0);
  lv_obj_clear_flag(segment, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(segment, LV_OBJ_FLAG_CLICKABLE);
  return segment;
}

void updateTempBar(const Values& v) {
  char text[32];
  if (v.coolantValid) {
    snprintf(text, sizeof(text), "TEMP C %d", (int)(v.coolantC + 0.5f));
  } else {
    snprintf(text, sizeof(text), "TEMP C --");
  }
  lv_label_set_text(g_tempLabel, text);
  lv_obj_set_style_text_color(g_tempLabel, lv_color_hex(v.tempWarning ? COL_RED : COL_TEXT),
                              0);

  float frac = 0.0f;
  if (v.coolantValid) {
    frac = (v.coolantC - 40.0f) / 90.0f;
    if (frac < 0.0f) {
      frac = 0.0f;
    }
    if (frac > 1.0f) {
      frac = 1.0f;
    }
  }

  const int filled = (int)(210.0f * frac);
  lv_obj_set_pos(g_tempCover, filled, 0);
  lv_obj_set_size(g_tempCover, 210 - filled, 16);
}

void updateFuelBar(const Values& v) {
  char text[32];
  if (v.fuelValid) {
    snprintf(text, sizeof(text), "FUEL %% %d", (int)(v.fuelPct + 0.5f));
  } else {
    snprintf(text, sizeof(text), "FUEL %% --");
  }
  lv_label_set_text(g_fuelLabel, text);
  lv_obj_set_style_text_color(g_fuelLabel, lv_color_hex(v.fuelWarning ? COL_RED : COL_TEXT),
                              0);

  float frac = 0.0f;
  if (v.fuelValid) {
    frac = v.fuelPct / 100.0f;
    if (frac < 0.0f) {
      frac = 0.0f;
    }
    if (frac > 1.0f) {
      frac = 1.0f;
    }
  }

  const int filled = (int)(210.0f * frac);
  lv_obj_set_pos(g_fuelCover, filled, 0);
  lv_obj_set_size(g_fuelCover, 210 - filled, 16);
}

void updateClock();

lv_obj_t* createSplash(lv_obj_t* parent) {
  lv_obj_t* splash = lv_obj_create(parent);
  lv_obj_remove_style_all(splash);
  lv_obj_set_size(splash, 800, 480);
  lv_obj_set_pos(splash, 0, 0);
  lv_obj_set_style_bg_color(splash, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
  lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(splash, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* gif = lv_gif_create(splash);
  lv_gif_set_src(gif, &vw_gif);
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

  lv_obj_move_foreground(splash);
  return splash;
}

void applyValues(const Values& v) {
  updateGauge(g_speed, v.speedKmh, v.speedValid, COL_CYAN);
  updateGauge(g_rpm, v.rpm, v.rpmValid, COL_ORANGE);

  updateTempBar(v);
  updateFuelBar(v);

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

  if (v.highBeam) {
    lv_obj_set_style_img_opa(g_iconLowBeam, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(g_iconBeam, LV_OPA_COVER, 0);
  } else if (v.lowBeam) {
    lv_obj_set_style_img_opa(g_iconLowBeam, LV_OPA_COVER, 0);
    lv_obj_set_style_img_opa(g_iconBeam, LV_OPA_0, 0);
  } else {
    lv_obj_set_style_img_opa(g_iconLowBeam, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(g_iconBeam, LV_OPA_20, 0);
  }

  const bool blinkOn = g_selfTestActive ? true : (((lv_tick_get() / 350U) & 1U) == 0U);
  setBlinking(g_turnLeft, v.turnLeft, blinkOn);
  setBlinking(g_turnRight, v.turnRight, blinkOn);

  updateClock();
}

void updateClock() {
  static int lastSecond = -1;

  const time_t now = time(nullptr);
  struct tm* tmv = localtime(&now);
  if (tmv == nullptr || now < 1600000000) {
    if (lastSecond != -1) {
      lv_label_set_text(g_clockTime, "--:--");
      lv_obj_add_flag(g_hourLine, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_minuteLine, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_secondLine, LV_OBJ_FLAG_HIDDEN);
      lastSecond = -1;
    }
    return;
  }

  if (tmv->tm_sec == lastSecond) {
    return;
  }
  lastSecond = tmv->tm_sec;

  lv_obj_clear_flag(g_hourLine, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_minuteLine, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_secondLine, LV_OBJ_FLAG_HIDDEN);

  const float hourAngle = ((tmv->tm_hour % 12) + tmv->tm_min / 60.0f) * 30.0f;
  const float minuteAngle = (tmv->tm_min + tmv->tm_sec / 60.0f) * 6.0f;
  const float secondAngle = tmv->tm_sec * 6.0f;

  setHandPoints(g_hourHand, CLOCK_SIZE, hourAngle, 42);
  setHandPoints(g_minuteHand, CLOCK_SIZE, minuteAngle, 62);
  setHandPoints(g_secondHand, CLOCK_SIZE, secondAngle, 72);
  lv_line_set_points(g_hourLine, g_hourHand, 2);
  lv_line_set_points(g_minuteLine, g_minuteHand, 2);
  lv_line_set_points(g_secondLine, g_secondHand, 2);

  char text[8];
  snprintf(text, sizeof(text), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
  lv_label_set_text(g_clockTime, text);
}

}  // namespace

namespace ui {

void init() {
  g_tickPointCount = 0;
  g_bootStarted = false;
  g_bootSkipped = false;
  g_selfTestActive = false;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Status icons are grouped in the middle of the top row.
  g_iconOil = createStatusIcon(screen, &icon_oil, 310, COL_RED);
  g_iconChg = createStatusIcon(screen, &icon_charge, 360, COL_RED);
  g_iconGlow = createStatusIcon(screen, &icon_glow, 410, COL_YELLOW);
  g_iconLowBeam = createStatusIcon(screen, &icon_beam_low, 460, COL_GREEN);
  g_iconBeam = createStatusIcon(screen, &icon_beam_high, 460, COL_BLUE);

  // Classic VW turn signal arrows: left on the left edge, right on the right edge.
  g_turnLeft = createTurnArrow(screen, 20, 2, true, g_leftArrowBuf);
  g_turnRight = createTurnArrow(screen, 726, 2, false, g_rightArrowBuf);

  // Left: clock. Center: large speedometer. Right: tachometer.
  createClock(screen, 14, 125, CLOCK_SIZE);

  g_speed = createGauge(screen, 242, 75, SPEED_SIZE, "", "km/h", 180, COL_CYAN,
                        -1.0f, 10.0f, 20.0f, false, 22, &lv_font_montserrat_48,
                        &lv_font_montserrat_24, &lv_font_montserrat_20);

  g_rpm = createGauge(screen, 576, 125, SIDE_SIZE, "", "RPM x1000", 7000, COL_ORANGE,
                      5000.0f / 7000.0f, 500.0f, 1000.0f, true, 14,
                      &lv_font_montserrat_32, &lv_font_montserrat_12,
                      &lv_font_montserrat_12, true, 3000.0f, 5000.0f, COL_DARK_GREEN,
                      COL_RED, 100.0f, rpmColor);

  g_voltage = makeLabel(screen, "--.-V", &lv_font_montserrat_32, COL_GREEN);
  lv_obj_align(g_voltage, LV_ALIGN_CENTER, 0, 168);

  g_odo = makeLabel(screen, "0.0 km", &lv_font_montserrat_20, COL_DIM);
  lv_obj_align(g_odo, LV_ALIGN_CENTER, 0, 202);

  g_tempLabel = makeLabel(screen, "TEMP --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_tempLabel, 14, 348);

  g_tempGradient = lv_obj_create(screen);
  lv_obj_remove_style_all(g_tempGradient);
  lv_obj_set_pos(g_tempGradient, 14, 376);
  lv_obj_set_size(g_tempGradient, 210, 16);
  lv_obj_set_style_radius(g_tempGradient, 8, 0);
  lv_obj_set_style_clip_corner(g_tempGradient, true, 0);
  lv_obj_set_style_bg_opa(g_tempGradient, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_tempGradient, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_tempGradient, LV_OBJ_FLAG_CLICKABLE);

  // 40..80: blue -> green, 80..100: green -> orange,
  // 100..115: orange -> red, 115..130: solid red.
  createGradientSegment(g_tempGradient, 0, 93, COL_BLUE, COL_GREEN);
  createGradientSegment(g_tempGradient, 93, 47, COL_GREEN, COL_ORANGE);
  createGradientSegment(g_tempGradient, 140, 35, COL_ORANGE, COL_RED);
  createGradientSegment(g_tempGradient, 175, 35, COL_RED, COL_RED);

  g_tempCover = lv_obj_create(g_tempGradient);
  lv_obj_remove_style_all(g_tempCover);
  lv_obj_set_pos(g_tempCover, 0, 0);
  lv_obj_set_size(g_tempCover, 210, 16);
  lv_obj_set_style_radius(g_tempCover, 8, 0);
  lv_obj_set_style_bg_color(g_tempCover, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_tempCover, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_tempCover, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_tempCover, LV_OBJ_FLAG_CLICKABLE);

  g_tempOutline = lv_obj_create(screen);
  lv_obj_remove_style_all(g_tempOutline);
  lv_obj_set_pos(g_tempOutline, 14, 376);
  lv_obj_set_size(g_tempOutline, 210, 16);
  lv_obj_set_style_radius(g_tempOutline, 8, 0);
  lv_obj_set_style_bg_opa(g_tempOutline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_tempOutline, 1, 0);
  lv_obj_set_style_border_color(g_tempOutline, lv_color_hex(COL_TRACK), 0);
  lv_obj_clear_flag(g_tempOutline, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_tempOutline, LV_OBJ_FLAG_CLICKABLE);

  g_fuelLabel = makeLabel(screen, "FUEL --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_fuelLabel, 576, 348);

  g_fuelGradient = lv_obj_create(screen);
  lv_obj_remove_style_all(g_fuelGradient);
  lv_obj_set_pos(g_fuelGradient, 576, 376);
  lv_obj_set_size(g_fuelGradient, 210, 16);
  lv_obj_set_style_radius(g_fuelGradient, 8, 0);
  lv_obj_set_style_clip_corner(g_fuelGradient, true, 0);
  lv_obj_set_style_bg_opa(g_fuelGradient, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_fuelGradient, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_fuelGradient, LV_OBJ_FLAG_CLICKABLE);

  // 0..10%: red, 10..30%: red -> orange, 30..50%: orange -> yellow (20% span),
  // 50..100%: yellow -> green.
  createGradientSegment(g_fuelGradient, 0, 21, COL_RED, COL_RED);
  createGradientSegment(g_fuelGradient, 21, 42, COL_RED, COL_ORANGE);
  createGradientSegment(g_fuelGradient, 63, 42, COL_ORANGE, COL_YELLOW);
  createGradientSegment(g_fuelGradient, 105, 105, COL_YELLOW, COL_GREEN);

  g_fuelCover = lv_obj_create(g_fuelGradient);
  lv_obj_remove_style_all(g_fuelCover);
  lv_obj_set_pos(g_fuelCover, 0, 0);
  lv_obj_set_size(g_fuelCover, 210, 16);
  lv_obj_set_style_radius(g_fuelCover, 8, 0);
  lv_obj_set_style_bg_color(g_fuelCover, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_fuelCover, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_fuelCover, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_fuelCover, LV_OBJ_FLAG_CLICKABLE);

  g_fuelOutline = lv_obj_create(screen);
  lv_obj_remove_style_all(g_fuelOutline);
  lv_obj_set_pos(g_fuelOutline, 576, 376);
  lv_obj_set_size(g_fuelOutline, 210, 16);
  lv_obj_set_style_radius(g_fuelOutline, 8, 0);
  lv_obj_set_style_bg_opa(g_fuelOutline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_fuelOutline, 1, 0);
  lv_obj_set_style_border_color(g_fuelOutline, lv_color_hex(COL_TRACK), 0);
  lv_obj_clear_flag(g_fuelOutline, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_fuelOutline, LV_OBJ_FLAG_CLICKABLE);

  g_splash = createSplash(screen);
}

void update(const Values& v) {
  if (!g_bootStarted) {
    g_bootStarted = true;
    g_bootStartMs = lv_tick_get();
  }

  const uint32_t elapsed =
      g_bootSkipped ? (BOOT_TOTAL_MS + 1) : (lv_tick_get() - g_bootStartMs);

  if (elapsed < BOOT_SPLASH_MS) {
    updateClock();
    return;
  }

  if (g_splash != nullptr && !lv_obj_has_flag(g_splash, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(g_splash, LV_OBJ_FLAG_HIDDEN);
  }

  if (elapsed < BOOT_TOTAL_MS) {
    const uint32_t phase = elapsed - BOOT_SPLASH_MS;
    float sweep = 0.0f;
    if (phase < BOOT_UP_MS) {
      sweep = (float)phase / (float)BOOT_UP_MS;
    } else if (phase < BOOT_UP_MS + BOOT_HOLD_MS) {
      sweep = 1.0f;
    } else {
      sweep = 1.0f - (float)(phase - BOOT_UP_MS - BOOT_HOLD_MS) / (float)BOOT_DOWN_MS;
      if (sweep < 0.0f) {
        sweep = 0.0f;
      }
    }

    Values selfTest = v;
    selfTest.speedKmh = 180.0f * sweep;
    selfTest.speedValid = true;
    selfTest.rpm = 7000.0f * sweep;
    selfTest.rpmValid = true;
    selfTest.coolantC = 40.0f + 90.0f * sweep;
    selfTest.coolantValid = true;
    selfTest.tempWarning = false;
    selfTest.fuelPct = 100.0f * sweep;
    selfTest.fuelValid = true;
    selfTest.fuelWarning = false;
    selfTest.oilWarning = true;
    selfTest.chargeWarning = true;
    selfTest.glowPlug = true;
    selfTest.highBeam = true;
    selfTest.lowBeam = false;
    selfTest.turnLeft = true;
    selfTest.turnRight = true;

    g_selfTestActive = true;
    applyValues(selfTest);
    return;
  }

  g_selfTestActive = false;
  applyValues(v);
}

void skipBoot() {
  g_bootSkipped = true;
}

}  // namespace ui
