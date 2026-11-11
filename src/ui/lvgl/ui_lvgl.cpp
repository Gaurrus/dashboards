#include "ui_lvgl.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

#include "icons.h"
#include "lvgl.h"
#include "ui_brightness.h"
#include "ui_bye.h"
#include "ui_clock.h"
#include "ui_design_b.h"
#include "ui_theme.h"
#include "version.h"
#include "vw_gif.h"

using namespace ui::theme;

namespace {

constexpr int SIDE_SIZE = 210;
constexpr int SPEED_SIZE = 315;

// Status icons sit on a circular orbit concentric with the speedometer.
constexpr float SPEED_CX = 242.0f + SPEED_SIZE / 2.0f;
constexpr float SPEED_CY = 75.0f + SPEED_SIZE / 2.0f;
constexpr float ICON_ORBIT_R = 205.0f;
constexpr float ICON_ORBIT_START_DEG = -40.0f;
constexpr float ICON_ORBIT_STEP_DEG = 16.0f;

constexpr int kGaugeTicks = 32;

struct Gauge {
  lv_obj_t* arc = nullptr;
  lv_obj_t* value = nullptr;
  lv_obj_t* cover = nullptr;
  uint32_t indicatorColor = 0xFFFFFFFFU;
  int32_t max = 0;
  bool div1000 = false;
  int segmentCount = 0;
  float segmentStep = 0.0f;
  lv_obj_t* segments[80] = {};
  int tickMarkCount = 0;
  int litCount = -1;
  lv_obj_t* tickMarks[kGaugeTicks] = {};
  lv_obj_t* tickLabels[kGaugeTicks] = {};
  float tickValues[kGaugeTicks] = {};
  uint32_t tickColors[kGaugeTicks] = {};
  uint32_t tickLabelColors[kGaugeTicks] = {};
};

Gauge g_speed;
Gauge g_rpm;

void setLabelText(lv_obj_t* label, const char* text);
void setTextColor(lv_obj_t* obj, uint32_t color);

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
lv_obj_t* g_iconFog = nullptr;
lv_obj_t* g_iconBrake = nullptr;
lv_obj_t* g_iconBrakeFault = nullptr;
lv_obj_t* g_turnLeft = nullptr;
lv_obj_t* g_turnRight = nullptr;

constexpr uint32_t BOOT_SPLASH_MS = 1800;
constexpr uint32_t BOOT_UP_MS = 900;
constexpr uint32_t BOOT_HOLD_MS = 300;
constexpr uint32_t BOOT_DOWN_MS = 1000;
constexpr uint32_t BOOT_TOTAL_MS =
    BOOT_SPLASH_MS + BOOT_UP_MS + BOOT_HOLD_MS + BOOT_DOWN_MS;

lv_obj_t* g_splash = nullptr;
lv_obj_t* g_splashGif = nullptr;
lv_obj_t* g_designA = nullptr;
lv_obj_t* g_gearBtn = nullptr;
bool g_bootStarted = false;
bool g_bootSkipped = false;
uint32_t g_bootStartMs = 0;
bool g_selfTestActive = false;
bool g_designBActive = false;
Values g_lastValues;

lv_obj_t* addTick(lv_obj_t* parent, int cx, int cy, float angleDeg, int rInner,
                  int rOuter, int width, uint32_t color) {
  if (g_tickPointCount >= kMaxTicks) {
    return nullptr;
  }

  const float rad = angleDeg * 3.14159265f / 180.0f;
  lv_point_t* points = g_tickPoints[g_tickPointCount++];
  points[0].x = (lv_coord_t)(cx + cosf(rad) * rInner);
  points[0].y = (lv_coord_t)(cy + sinf(rad) * rInner);
  points[1].x = (lv_coord_t)(cx + cosf(rad) * rOuter);
  points[1].y = (lv_coord_t)(cy + sinf(rad) * rOuter);

  // Keep each tick object as small as its two endpoints. A full-gauge line
  // object would make every color change invalidate the whole dial.
  const int pad = width / 2 + 2;
  const lv_coord_t x1 = LV_MIN(points[0].x, points[1].x) - pad;
  const lv_coord_t y1 = LV_MIN(points[0].y, points[1].y) - pad;
  const lv_coord_t x2 = LV_MAX(points[0].x, points[1].x) + pad;
  const lv_coord_t y2 = LV_MAX(points[0].y, points[1].y) + pad;
  points[0].x -= x1;
  points[0].y -= y1;
  points[1].x -= x1;
  points[1].y -= y1;

  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_size(line, x2 - x1 + 1, y2 - y1 + 1);
  lv_obj_set_pos(line, x1, y1);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  return line;
}

// A full-size gradient ring is pre-rendered into a canvas once. Updating the
// tachometer then only redraws a small angular slice instead of the dozens of
// overlapping arc objects it would take to build the gradient.
void* allocGradientBuffer(size_t bytes) {
#if defined(ARDUINO)
  // Prefer internal RAM when there is enough headroom left; fall back to PSRAM.
  constexpr size_t kInternalReserve = 64 * 1024;
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) > bytes + kInternalReserve) {
    void* buf = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buf != nullptr) {
      return buf;
    }
  }
  void* buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (buf != nullptr) {
    return buf;
  }
#endif
  return malloc(bytes);
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
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
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
    lv_obj_set_style_arc_rounded(red, false, LV_PART_MAIN);
  lv_obj_set_style_arc_color(red, lv_color_hex(COL_RED), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(red, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_opa(red, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(red, LV_OBJ_FLAG_CLICKABLE);
  }

  if (segmentedFill && segStep > 0.0f) {
    const size_t bufferSize =
        LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(size, size);
    void* ringBuf = allocGradientBuffer(bufferSize);

    if (ringBuf != nullptr) {
      lv_obj_t* ring = lv_canvas_create(box);
      lv_canvas_set_buffer(ring, ringBuf, size, size,
                           LV_IMG_CF_TRUE_COLOR_ALPHA);
      lv_obj_set_pos(ring, 0, 0);
      lv_canvas_fill_bg(ring, lv_color_black(), LV_OPA_TRANSP);

      lv_draw_arc_dsc_t arcDsc;
      lv_draw_arc_dsc_init(&arcDsc);
      arcDsc.width = arcWidth;
      arcDsc.rounded = 0;
      arcDsc.opa = LV_OPA_COVER;

      const int total = (int)((float)max / segStep + 0.5f);
      for (int i = 0; i < total; ++i) {
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
          color =
              lerpColor(segFrom, segTo, (midValue - segStart) / (segEnd - segStart));
        }

        arcDsc.color = lv_color_hex(color);
        lv_canvas_draw_arc(ring, size / 2, size / 2, size / 2,
                           (int32_t)startAngle, (int32_t)endAngle, &arcDsc);
      }

      // Mask the part of the gradient above the current value with the track
      // color. Only this arc changes while driving.
      lv_obj_t* cover = lv_arc_create(box);
      lv_obj_set_size(cover, size, size);
      lv_obj_center(cover);
      lv_arc_set_bg_angles(cover, 135, 45);
      lv_obj_set_style_arc_width(cover, arcWidth, LV_PART_MAIN);
      lv_obj_set_style_arc_color(cover, lv_color_hex(COL_TRACK), LV_PART_MAIN);
      lv_obj_set_style_arc_rounded(cover, false, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(cover, LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_opa(cover, LV_OPA_TRANSP, LV_PART_KNOB);
      lv_obj_clear_flag(cover, LV_OBJ_FLAG_CLICKABLE);
      g.cover = cover;
    } else {
      // Canvas allocation failed (e.g. no PSRAM): fall back to one arc per
      // segment. Correct, but redraws are much heavier.
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
          color =
              lerpColor(segFrom, segTo, (midValue - segStart) / (segEnd - segStart));
        }

        lv_obj_t* segment = lv_arc_create(box);
        lv_obj_set_size(segment, size, size);
        lv_obj_center(segment);
        lv_arc_set_bg_angles(segment, (int32_t)startAngle, (int32_t)endAngle);
        lv_obj_set_style_arc_width(segment, arcWidth, LV_PART_MAIN);
        lv_obj_set_style_arc_color(segment, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(segment, false, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(segment, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_opa(segment, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_clear_flag(segment, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
        g.segments[g.segmentCount++] = segment;
      }
      g.segmentStep = segStep;
    }
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

    lv_obj_t* mark = addTick(box, size / 2, size / 2, angle,
                             rOuter - (major ? 13 : 7), rOuter, major ? 3 : 2, color);

    const int markIndex = g.tickMarkCount;
    if (mark != nullptr && markIndex < kGaugeTicks) {
      g.tickMarks[markIndex] = mark;
      g.tickValues[markIndex] = tickValue;
      g.tickColors[markIndex] = color;
      g.tickLabels[markIndex] = nullptr;
      g.tickLabelColors[markIndex] = red ? COL_RED : COL_DIM;

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
        g.tickLabels[markIndex] = tick;
      }

      g.tickMarkCount = markIndex + 1;
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

void updateGauge(Gauge& g, float value, bool valid, uint32_t accent,
                 bool highlightReached = false) {
  int32_t arcValue = valid ? (int32_t)(value + 0.5f) : 0;
  if (arcValue < 0) {
    arcValue = 0;
  }
  if (arcValue > g.max) {
    arcValue = g.max;
  }
  if (g.cover != nullptr) {
    int32_t coverStart =
        135 + (int32_t)(270.0f * (float)arcValue / (float)g.max);
    coverStart %= 360;
    lv_arc_set_bg_angles(g.cover, (uint16_t)coverStart, 45);
  } else if (g.segmentCount > 0) {
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
    const uint32_t indicator = valid ? accent : COL_TRACK;
    // Style setters invalidate the whole object, so only touch the color when
    // it actually changes (normally never after the first frame).
    if (indicator != g.indicatorColor) {
      g.indicatorColor = indicator;
      lv_obj_set_style_arc_color(g.arc, lv_color_hex(indicator), LV_PART_INDICATOR);
    }
  }

  if (highlightReached && g.tickMarkCount > 0) {
    int lit = 0;
    if (valid) {
      while (lit < g.tickMarkCount && g.tickValues[lit] <= value + 0.5f) {
        ++lit;
      }
    }
    if (lit != g.litCount) {
      const int oldLit = g.litCount;
      g.litCount = lit;
      const int from = oldLit < 0 ? 0 : LV_MIN(oldLit, lit);
      const int to = oldLit < 0 ? g.tickMarkCount : LV_MAX(oldLit, lit);
      for (int i = from; i < to; ++i) {
        const bool on = i < lit;
        const uint32_t markColor = on ? COL_YELLOW_BRIGHT : g.tickColors[i];
        lv_obj_set_style_line_color(g.tickMarks[i], lv_color_hex(markColor), 0);
        if (g.tickLabels[i] != nullptr) {
          const uint32_t labelColor = on ? COL_YELLOW_BRIGHT : g.tickLabelColors[i];
          lv_obj_set_style_text_color(g.tickLabels[i], lv_color_hex(labelColor), 0);
        }
      }
    }
  }

  char text[24];
  if (!valid) {
    snprintf(text, sizeof(text), "--");
  } else if (g.div1000) {
    snprintf(text, sizeof(text), "%.1f", value / 1000.0f);
  } else {
    snprintf(text, sizeof(text), "%d", (int)(value + 0.5f));
  }
  setLabelText(g.value, text);
}

void setImgOpa(lv_obj_t* obj, lv_opa_t opa) {
  if (lv_obj_get_style_img_opa(obj, 0) == opa) {
    return;
  }
  lv_obj_set_style_img_opa(obj, opa, 0);
}

// lv_label_set_text() invalidates the object even when the string is unchanged.
void setLabelText(lv_obj_t* label, const char* text) {
  if (label == nullptr) {
    return;
  }
  const char* current = lv_label_get_text(label);
  if (current != nullptr && strcmp(current, text) == 0) {
    return;
  }
  lv_label_set_text(label, text);
}

// Style setters invalidate the whole object; compare the quantized color.
void setTextColor(lv_obj_t* obj, uint32_t color) {
  if (obj == nullptr) {
    return;
  }
  const lv_color_t target = lv_color_hex(color);
  if (lv_color_to32(lv_obj_get_style_text_color(obj, 0)) == lv_color_to32(target)) {
    return;
  }
  lv_obj_set_style_text_color(obj, target, 0);
}

void setIconState(lv_obj_t* icon, bool on) {
  setImgOpa(icon, on ? LV_OPA_COVER : LV_OPA_20);
}

lv_obj_t* createStatusIcon(lv_obj_t* parent, const lv_img_dsc_t* src, int x, int y) {
  lv_obj_t* icon = lv_img_create(parent);
  lv_img_set_src(icon, src);
  lv_obj_set_pos(icon, x, y);
  lv_obj_set_style_img_opa(icon, LV_OPA_20, 0);
  return icon;
}

lv_obj_t* createOrbitIcon(lv_obj_t* parent, const lv_img_dsc_t* src, int index) {
  const float deg = ICON_ORBIT_START_DEG + ICON_ORBIT_STEP_DEG * index;
  const float rad = deg * 3.14159265f / 180.0f;
  const int cx = (int)(SPEED_CX + ICON_ORBIT_R * sinf(rad) + 0.5f);
  const int cy = (int)(SPEED_CY - ICON_ORBIT_R * cosf(rad) + 0.5f);
  const int iw = (int)src->header.w;
  const int ih = (int)src->header.h;
  return createStatusIcon(parent, src, cx - iw / 2, cy - ih / 2);
}

lv_obj_t* createTurnArrow(lv_obj_t* parent, const lv_img_dsc_t* src, int x, int y) {
  lv_obj_t* arrow = lv_img_create(parent);
  lv_img_set_src(arrow, src);
  lv_obj_set_pos(arrow, x, y);
  lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  return arrow;
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
  setLabelText(g_tempLabel, text);
  setTextColor(g_tempLabel, v.tempWarning ? COL_RED : COL_TEXT);

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
  static int lastFilled = -1;
  if (filled != lastFilled) {
    lastFilled = filled;
    lv_obj_set_pos(g_tempCover, filled, 0);
    lv_obj_set_size(g_tempCover, 210 - filled, 16);
  }
}

void updateFuelBar(const Values& v) {
  char text[32];
  if (v.fuelValid) {
    snprintf(text, sizeof(text), "FUEL %% %d", (int)(v.fuelPct + 0.5f));
  } else {
    snprintf(text, sizeof(text), "FUEL %% --");
  }
  setLabelText(g_fuelLabel, text);
  setTextColor(g_fuelLabel, v.fuelWarning ? COL_RED : COL_TEXT);

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
  static int lastFilled = -1;
  if (filled != lastFilled) {
    lastFilled = filled;
    lv_obj_set_pos(g_fuelCover, filled, 0);
    lv_obj_set_size(g_fuelCover, 210 - filled, 16);
  }
}

void clockTapCb(lv_event_t* /*e*/) {
  if (ui::internal::byeActive()) {
    return;
  }
  if (!g_bootStarted) {
    return;
  }
  if (!g_bootSkipped && lv_tick_elaps(g_bootStartMs) < BOOT_TOTAL_MS) {
    return;
  }
  ui::internal::clockShowSettings();
}

// --- Speedometer arc color: hold 3 s inside the dial circle for the menu. ---
constexpr uint32_t kArcColors[] = {
    COL_CYAN,   COL_GREEN, COL_ORANGE, COL_YELLOW,
    COL_BLUE,   COL_RED,   COL_TEXT,   0xA855F7,
};
constexpr int kArcColorCount = 8;
constexpr uint32_t kDialHoldMs = 3000;
constexpr float kDialCenterX = 399.5f;
constexpr float kDialCenterY = 232.5f;
constexpr float kDialRadius = 155.0f;

lv_obj_t* g_colorMenu = nullptr;
lv_obj_t* g_colorSwatches[kArcColorCount] = {};
uint32_t g_speedArcColor = COL_CYAN;
int g_speedArcIndex = 0;
bool g_dialPressActive = false;
bool g_colorMenuOpenedForPress = false;
uint32_t g_dialPressMs = 0;

bool pointInDial(const lv_point_t& p) {
  const float dx = (float)p.x - kDialCenterX;
  const float dy = (float)p.y - kDialCenterY;
  return dx * dx + dy * dy <= kDialRadius * kDialRadius;
}

void setSpeedArcColor(uint32_t color, int index) {
  g_speedArcColor = color;
  g_speedArcIndex = index;
  if (g_speed.value != nullptr) {
    lv_obj_set_style_text_color(g_speed.value, lv_color_hex(color), 0);
  }
  for (int i = 0; i < kArcColorCount; ++i) {
    if (g_colorSwatches[i] == nullptr) {
      continue;
    }
    const bool selected = (i == index);
    lv_obj_set_style_border_width(g_colorSwatches[i], selected ? 3 : 1, 0);
    lv_obj_set_style_border_color(
        g_colorSwatches[i], lv_color_hex(selected ? COL_TEXT : COL_TRACK), 0);
  }
}

void colorMenuShow() {
  if (g_colorMenu == nullptr) {
    return;
  }
  lv_obj_clear_flag(g_colorMenu, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_colorMenu);
}

void colorMenuHide() {
  if (g_colorMenu != nullptr) {
    lv_obj_add_flag(g_colorMenu, LV_OBJ_FLAG_HIDDEN);
  }
}

void colorSwatchCb(lv_event_t* e) {
  const int index = (int)(intptr_t)lv_event_get_user_data(e);
  if (index >= 0 && index < kArcColorCount) {
    setSpeedArcColor(kArcColors[index], index);
  }
  colorMenuHide();
}

void colorBackdropCb(lv_event_t* /*e*/) {
  colorMenuHide();
}

void gaugeTapCb(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t* indev = lv_indev_get_act();
  lv_point_t p{0, 0};
  if (indev != nullptr) {
    lv_indev_get_point(indev, &p);
  }

  if (code == LV_EVENT_PRESSED) {
    g_dialPressActive = false;
    g_colorMenuOpenedForPress = false;
    if (pointInDial(p)) {
      g_dialPressActive = true;
      g_dialPressMs = lv_tick_get();
    }
  } else if (code == LV_EVENT_PRESSING) {
    if (!g_dialPressActive || g_colorMenuOpenedForPress) {
      return;
    }
    if (!pointInDial(p)) {
      g_dialPressActive = false;
      return;
    }
    if (lv_tick_elaps(g_dialPressMs) >= kDialHoldMs) {
      colorMenuShow();
      g_colorMenuOpenedForPress = true;
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    g_dialPressActive = false;
  }
}

lv_obj_t* createColorMenu() {
  lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(backdrop);
  lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(backdrop, 0, 0);
  lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(backdrop, (lv_opa_t)170, 0);
  lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(backdrop, colorBackdropCb, LV_EVENT_CLICKED, nullptr);
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

  lv_obj_t* title = makeLabel(panel, "ARC COLOR", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  for (int i = 0; i < kArcColorCount; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    lv_obj_t* swatch = lv_btn_create(panel);
    lv_obj_set_size(swatch, 70, 70);
    lv_obj_set_pos(swatch, 59 + col * 84, 84 + row * 84);
    lv_obj_set_style_bg_color(swatch, lv_color_hex(kArcColors[i]), 0);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(swatch, 12, 0);
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, lv_color_hex(COL_TRACK), 0);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(swatch, colorSwatchCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    g_colorSwatches[i] = swatch;
  }

  lv_obj_move_foreground(backdrop);
  setSpeedArcColor(g_speedArcColor, g_speedArcIndex);
  return backdrop;
}

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
  g_splashGif = gif;

  lv_obj_t* versionLabel =
      makeLabel(splash, "v" T3DASH_VERSION, &lv_font_montserrat_14, COL_DIM);
  lv_obj_align(versionLabel, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_text_opa(versionLabel, LV_OPA_80, 0);

  lv_obj_move_foreground(splash);
  return splash;
}

void applyValues(const Values& v) {
  updateGauge(g_speed, v.speedKmh, v.speedValid, g_speedArcColor, true);
  updateGauge(g_rpm, v.rpm, v.rpmValid, COL_ORANGE);

  updateTempBar(v);
  updateFuelBar(v);

  char text[24];
  if (v.batteryValid) {
    snprintf(text, sizeof(text), "%.1fV", v.batteryV);
  } else {
    snprintf(text, sizeof(text), "--.-V");
  }
  setLabelText(g_voltage, text);
  const bool batteryOk = v.batteryValid && v.batteryV >= 11.0f && v.batteryV <= 15.0f;
  setTextColor(g_voltage, v.batteryValid ? (batteryOk ? COL_GREEN : COL_RED) : COL_DIM);

  snprintf(text, sizeof(text), "%.1f km", v.odometerKm);
  setLabelText(g_odo, text);

  setIconState(g_iconOil, v.oilWarning);
  setIconState(g_iconChg, v.chargeWarning);
  setIconState(g_iconGlow, v.glowPlug);
  setIconState(g_iconFog, v.fogLight);

  // Parking brake and brake fault share one orbit slot. A fault wins; if both
  // are active, alternate so neither condition is hidden.
  bool showBrakeFault = v.brakeFault;
  if (v.brakeFault && v.parkingBrake && !g_selfTestActive) {
    showBrakeFault = ((lv_tick_get() / 700U) & 1U) == 0U;
  }
  setIconState(g_iconBrake, v.parkingBrake && !showBrakeFault);
  setIconState(g_iconBrakeFault, showBrakeFault);

  if (v.highBeam) {
    setImgOpa(g_iconLowBeam, LV_OPA_0);
    setImgOpa(g_iconBeam, LV_OPA_COVER);
  } else if (v.lowBeam) {
    setImgOpa(g_iconLowBeam, LV_OPA_COVER);
    setImgOpa(g_iconBeam, LV_OPA_0);
  } else {
    setImgOpa(g_iconLowBeam, LV_OPA_0);
    setImgOpa(g_iconBeam, LV_OPA_20);
  }

  const bool blinkOn = g_selfTestActive ? true : (((lv_tick_get() / 350U) & 1U) == 0U);
  setBlinking(g_turnLeft, v.turnLeft, blinkOn);
  setBlinking(g_turnRight, v.turnRight, blinkOn);

  ui::internal::clockUpdate();
}

void setGearVisible(bool visible) {
  if (g_gearBtn == nullptr) {
    return;
  }
  const bool hidden = lv_obj_has_flag(g_gearBtn, LV_OBJ_FLAG_HIDDEN);
  if (visible && hidden) {
    lv_obj_clear_flag(g_gearBtn, LV_OBJ_FLAG_HIDDEN);
  } else if (!visible && !hidden) {
    lv_obj_add_flag(g_gearBtn, LV_OBJ_FLAG_HIDDEN);
  }
}

void gearCb(lv_event_t* /*e*/) {
  if (ui::internal::byeActive()) {
    return;
  }
  g_designBActive = !g_designBActive;
  if (g_designBActive) {
    lv_obj_add_flag(g_designA, LV_OBJ_FLAG_HIDDEN);
    ui::internal::designBSetVisible(true);
    ui::internal::designBUpdate(g_lastValues);
  } else {
    ui::internal::designBSetVisible(false);
    lv_obj_clear_flag(g_designA, LV_OBJ_FLAG_HIDDEN);
    applyValues(g_lastValues);
  }
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

  // A transparent full-screen container holds the classic layout so the gear
  // button can switch between designs by hiding it as a whole.
  g_designA = lv_obj_create(screen);
  lv_obj_remove_style_all(g_designA);
  lv_obj_set_pos(g_designA, 0, 0);
  lv_obj_set_size(g_designA, 800, 480);
  lv_obj_set_style_bg_opa(g_designA, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_designA, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_designA, LV_OBJ_FLAG_CLICKABLE);

  // Status icons follow an arc concentric with the speedometer.
  g_iconOil = createOrbitIcon(g_designA, &icon_oil, 0);
  g_iconChg = createOrbitIcon(g_designA, &icon_charge, 1);
  g_iconBrake = createOrbitIcon(g_designA, &icon_brake, 2);
  g_iconBrakeFault = createOrbitIcon(g_designA, &icon_brake_fault, 2);
  lv_obj_set_style_img_opa(g_iconBrakeFault, LV_OPA_0, 0);
  g_iconGlow = createOrbitIcon(g_designA, &icon_glow, 3);
  g_iconFog = createOrbitIcon(g_designA, &icon_anti_fog, 4);
  g_iconLowBeam = createOrbitIcon(g_designA, &icon_beam_low, 5);
  g_iconBeam = createOrbitIcon(g_designA, &icon_beam_high, 5);

  // Classic VW turn signal arrows: left on the left edge, right on the right edge.
  g_turnLeft = createTurnArrow(g_designA, &icon_arrow_left, 20, 8);
  g_turnRight = createTurnArrow(g_designA, &icon_arrow_right, 726, 8);

  // Left: clock. Center: large speedometer. Right: tachometer.
  ui::internal::clockInit(g_designA);

  g_speed = createGauge(g_designA, 242, 75, SPEED_SIZE, "", "km/h", 180, COL_CYAN,
                        -1.0f, 10.0f, 20.0f, false, 22, &lv_font_montserrat_48,
                        &lv_font_montserrat_24, &lv_font_montserrat_20);

  g_rpm = createGauge(g_designA, 576, 125, SIDE_SIZE, "", "RPM x1000", 5500, COL_ORANGE,
                      4500.0f / 5500.0f, 500.0f, 1000.0f, true, 14,
                      &lv_font_montserrat_32, &lv_font_montserrat_12,
                      &lv_font_montserrat_12, true, 2500.0f, 4500.0f, COL_DARK_GREEN,
                      COL_RED, 100.0f, rpmColor);

  g_voltage = makeLabel(g_designA, "--.-V", &lv_font_montserrat_32, COL_GREEN);
  lv_obj_align(g_voltage, LV_ALIGN_CENTER, 0, 168);

  g_odo = makeLabel(g_designA, "0.0 km", &lv_font_montserrat_20, COL_DIM);
  lv_obj_align(g_odo, LV_ALIGN_CENTER, 0, 202);

  g_tempLabel = makeLabel(g_designA, "TEMP --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_tempLabel, 14, 348);

  g_tempGradient = lv_obj_create(g_designA);
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
  lv_obj_set_style_radius(g_tempCover, 0, 0);
  lv_obj_set_style_bg_color(g_tempCover, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_tempCover, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_tempCover, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_tempCover, LV_OBJ_FLAG_CLICKABLE);

  g_tempOutline = lv_obj_create(g_designA);
  lv_obj_remove_style_all(g_tempOutline);
  lv_obj_set_pos(g_tempOutline, 14, 376);
  lv_obj_set_size(g_tempOutline, 210, 16);
  lv_obj_set_style_radius(g_tempOutline, 8, 0);
  lv_obj_set_style_bg_opa(g_tempOutline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_tempOutline, 1, 0);
  lv_obj_set_style_border_color(g_tempOutline, lv_color_hex(COL_TRACK), 0);
  lv_obj_clear_flag(g_tempOutline, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_tempOutline, LV_OBJ_FLAG_CLICKABLE);

  g_fuelLabel = makeLabel(g_designA, "FUEL --", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_pos(g_fuelLabel, 576, 348);

  g_fuelGradient = lv_obj_create(g_designA);
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
  lv_obj_set_style_radius(g_fuelCover, 0, 0);
  lv_obj_set_style_bg_color(g_fuelCover, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_fuelCover, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_fuelCover, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_fuelCover, LV_OBJ_FLAG_CLICKABLE);

  g_fuelOutline = lv_obj_create(g_designA);
  lv_obj_remove_style_all(g_fuelOutline);
  lv_obj_set_pos(g_fuelOutline, 576, 376);
  lv_obj_set_size(g_fuelOutline, 210, 16);
  lv_obj_set_style_radius(g_fuelOutline, 8, 0);
  lv_obj_set_style_bg_opa(g_fuelOutline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_fuelOutline, 1, 0);
  lv_obj_set_style_border_color(g_fuelOutline, lv_color_hex(COL_TRACK), 0);
  lv_obj_clear_flag(g_fuelOutline, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_fuelOutline, LV_OBJ_FLAG_CLICKABLE);

  ui::internal::designBInit(screen);

  g_splash = createSplash(screen);
  ui::internal::byeInit(screen);

  // Gear button: switches between the classic and the compact layout. It lives
  // on the top layer, but is created before the brightness dimmer and the
  // settings overlays so they can cover it.
  g_gearBtn = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_gearBtn);
  lv_obj_set_size(g_gearBtn, 56, 56);
  lv_obj_set_pos(g_gearBtn, 16, 408);
  lv_obj_set_style_radius(g_gearBtn, 12, 0);
  lv_obj_set_style_bg_opa(g_gearBtn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(g_gearBtn, lv_color_hex(COL_TRACK), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(g_gearBtn, (lv_opa_t)140, LV_STATE_PRESSED);
  lv_obj_add_flag(g_gearBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_gearBtn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_gearBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(g_gearBtn, gearCb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* gearIcon = lv_img_create(g_gearBtn);
  lv_img_set_src(gearIcon, &icon_settings);
  lv_obj_center(gearIcon);
  lv_obj_clear_flag(gearIcon, LV_OBJ_FLAG_CLICKABLE);

  ui::internal::brightnessInit();

  lv_obj_t* clockHit = lv_obj_create(g_designA);
  lv_obj_remove_style_all(clockHit);
  lv_obj_set_size(clockHit, ui::internal::CLOCK_SIZE, ui::internal::CLOCK_SIZE);
  lv_obj_set_pos(clockHit, ui::internal::CLOCK_X, ui::internal::CLOCK_Y);
  lv_obj_set_style_bg_opa(clockHit, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(clockHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(clockHit, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(clockHit, clockTapCb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* dialHit = lv_obj_create(g_designA);
  lv_obj_remove_style_all(dialHit);
  lv_obj_set_size(dialHit, SPEED_SIZE, SPEED_SIZE);
  lv_obj_set_pos(dialHit, 242, 75);
  lv_obj_set_style_bg_opa(dialHit, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(dialHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(dialHit, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(dialHit, gaugeTapCb, LV_EVENT_ALL, nullptr);

  ui::internal::clockSettingsInit();
  g_colorMenu = createColorMenu();
}

void update(const Values& v) {
  if (ui::internal::byeUpdate(v.ignitionOn)) {
    ui::internal::clockHideSettings();
    colorMenuHide();
    setGearVisible(false);
    return;
  }

  if (!g_bootStarted) {
    g_bootStarted = true;
    g_bootStartMs = lv_tick_get();
  }

  const uint32_t elapsed =
      g_bootSkipped ? (BOOT_TOTAL_MS + 1) : (lv_tick_get() - g_bootStartMs);

  if (elapsed < BOOT_SPLASH_MS) {
    ui::internal::clockUpdate();
    setGearVisible(false);
    return;
  }

  if (g_splash != nullptr && !lv_obj_has_flag(g_splash, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(g_splash, LV_OBJ_FLAG_HIDDEN);
    if (g_splashGif != nullptr) {
      lv_timer_pause(((lv_gif_t*)g_splashGif)->timer);
    }
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
    selfTest.rpm = 5500.0f * sweep;
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
    selfTest.parkingBrake = sweep < 0.5f;
    selfTest.brakeFault = sweep >= 0.5f;
    selfTest.highBeam = true;
    selfTest.lowBeam = false;
    selfTest.fogLight = true;
    selfTest.turnLeft = true;
    selfTest.turnRight = true;

    g_selfTestActive = true;
    setGearVisible(false);
    applyValues(selfTest);
    return;
  }

  g_selfTestActive = false;
  setGearVisible(true);
  g_lastValues = v;
  if (g_designBActive) {
    ui::internal::designBUpdate(v);
  } else {
    applyValues(v);
  }
}

void skipBoot() {
  g_bootSkipped = true;
}

}  // namespace ui

namespace ui {
namespace internal {

uint32_t speedColor() {
  return g_speedArcColor;
}

void showSpeedColorMenu() {
  if (ui::internal::byeActive()) {
    return;
  }
  colorMenuShow();
}

}  // namespace internal
}  // namespace ui
