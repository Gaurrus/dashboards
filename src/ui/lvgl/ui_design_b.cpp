#include "ui_design_b.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

#include "font_speed_160.h"
#include "icons.h"
#include "ui_lvgl.h"
#include "ui_theme.h"

namespace {

using namespace ui::theme;

constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;

// --- Speed: one big number, slightly left of the screen centre. ---
constexpr int SPEED_CX = 200;
constexpr int SPEED_OFFSET_Y = -60;

// --- Concentric quarter gauges: 4th quadrant, right -> bottom. ---
constexpr int CLUSTER_CX = 780;
constexpr int CLUSTER_CY = 460;
constexpr int TACH_R = 360;
constexpr int TACH_ARC_W = 36;
constexpr float TACH_MAX = 5500.0f;
constexpr int TEMP_R = 264;
constexpr int TEMP_ARC_W = 20;
constexpr float TEMP_MIN_C = 60.0f;
constexpr float TEMP_MAX_C = 120.0f;
constexpr int FUEL_R = 168;
constexpr int FUEL_ARC_W = 20;

constexpr int SEGMENT_WIDTH_PX = 10;
constexpr int SEGMENT_GAP_PX = 4;
constexpr int SEGMENT_RADIUS_PX = 2;

// --- Bottom row of status lamps. ---
constexpr int LAMP_Y = 436;
constexpr int LAMP_FIRST_X = 150;
constexpr int LAMP_STEP = 44;

struct SegmentedGauge {
  lv_obj_t* canvas = nullptr;
  uint8_t* buffer = nullptr;
  int canvasSize = 0;
  int radius = 0;
  int width = 0;
  int segmentCount = 0;
  float periodDeg = 0.0f;
  float dashDeg = 0.0f;
  float scaleMin = 0.0f;
  float scaleMax = 0.0f;
  uint32_t (*colorFn)(float) = nullptr;
  int lit = 0;
};

lv_obj_t* g_root = nullptr;
lv_obj_t* g_speedDigits[3] = {};
lv_obj_t* g_digitCanvas[11] = {};
uint8_t* g_digitBuffer[11] = {};
lv_img_dsc_t g_digitDsc[11];
int g_digitAdvance[11] = {};
int g_digitWidth[11] = {};
int g_digitCanvasH = 0;
int g_speedTop = 0;
uint32_t g_speedColor = 0;
bool g_speedHoldActive = false;
uint32_t g_speedHoldMs = 0;
lv_obj_t* g_clockLabel = nullptr;
lv_obj_t* g_odoLabel = nullptr;

SegmentedGauge g_tachGauge;
SegmentedGauge g_tempGauge;
SegmentedGauge g_fuelGauge;

lv_obj_t* g_iconOil = nullptr;
lv_obj_t* g_iconChg = nullptr;
lv_obj_t* g_iconGlow = nullptr;
lv_obj_t* g_iconFog = nullptr;
lv_obj_t* g_iconBrake = nullptr;
lv_obj_t* g_iconBrakeFault = nullptr;
lv_obj_t* g_iconLowBeam = nullptr;
lv_obj_t* g_iconBeam = nullptr;
lv_obj_t* g_turnLeft = nullptr;
lv_obj_t* g_turnRight = nullptr;

constexpr int kMaxArcTicks = 64;
lv_point_t g_arcTickPoints[kMaxArcTicks][2];
int g_arcTickPointCount = 0;

void setImgOpa(lv_obj_t* obj, lv_opa_t opa) {
  if (obj == nullptr || lv_obj_get_style_img_opa(obj, 0) == opa) {
    return;
  }
  lv_obj_set_style_img_opa(obj, opa, 0);
}

void setIconState(lv_obj_t* icon, bool on) {
  setImgOpa(icon, on ? LV_OPA_COVER : LV_OPA_20);
}

void setLabelIfChanged(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current == nullptr || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
  }
}

void updateSpeed(const Values& v) {
  int digits[3] = {};
  int count = 0;

  if (v.speedValid) {
    int speed = (int)(v.speedKmh + 0.5f);
    if (speed < 0) {
      speed = 0;
    }
    if (speed > 999) {
      speed = 999;
    }
    if (speed >= 100) {
      digits[0] = speed / 100;
      digits[1] = (speed / 10) % 10;
      digits[2] = speed % 10;
      count = 3;
    } else if (speed >= 10) {
      digits[0] = speed / 10;
      digits[1] = speed % 10;
      count = 2;
    } else {
      digits[0] = speed;
      count = 1;
    }
  } else {
    digits[0] = 10;
    digits[1] = 10;
    count = 2;
  }

  int total = 0;
  for (int i = 0; i < count; ++i) {
    total += g_digitAdvance[digits[i]];
  }

  int x = SPEED_CX - total / 2;
  for (int slot = 0; slot < 3; ++slot) {
    if (slot < count) {
      const int index = digits[slot];
      lv_img_set_src(g_speedDigits[slot], &g_digitDsc[index]);
      lv_obj_set_pos(g_speedDigits[slot], x - 8, g_speedTop);
      lv_obj_clear_flag(g_speedDigits[slot], LV_OBJ_FLAG_HIDDEN);
      x += g_digitAdvance[index];
    } else {
      lv_obj_add_flag(g_speedDigits[slot], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void updateClockAndOdo(const Values& v) {
  const uint32_t seconds = ui::clockSeconds();
  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02u:%02u",
           (unsigned)(seconds / 3600U), (unsigned)((seconds / 60U) % 60U));
  setLabelIfChanged(g_clockLabel, clockText);

  char odoText[16];
  snprintf(odoText, sizeof(odoText), "%.1f km", v.odometerKm);
  setLabelIfChanged(g_odoLabel, odoText);
}

void* allocCanvasBuffer(size_t bytes) {
#if defined(ARDUINO)
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

void renderDigit(int index, uint32_t topColor) {
  if (g_digitCanvas[index] == nullptr || g_digitBuffer[index] == nullptr) {
    return;
  }

  const int w = g_digitWidth[index];
  const int h = g_digitCanvasH;
  lv_canvas_fill_bg(g_digitCanvas[index], lv_color_black(), LV_OPA_TRANSP);

  const uint32_t ch =
      index < 10 ? (uint32_t)('0' + index) : (uint32_t)'-';
  lv_draw_label_dsc_t dsc;
  lv_draw_label_dsc_init(&dsc);
  dsc.font = &lv_font_speed_160;
  dsc.color = lv_color_hex(0xFFFFFF);
  dsc.opa = LV_OPA_COVER;
  dsc.align = LV_TEXT_ALIGN_LEFT;
  char text[2] = {(char)ch, '\0'};
  lv_canvas_draw_text(g_digitCanvas[index], 8, 2, w, &dsc, text);

  uint8_t* data = g_digitBuffer[index];
  for (int y = 0; y < h; ++y) {
    const float t = (float)y / (float)(h - 1);
    const uint16_t c =
        lv_color_to16(lv_color_hex(lerpColor(topColor, COL_BG, t)));
    for (int x = 0; x < w; ++x) {
      uint8_t* px = data + ((size_t)y * (size_t)w + (size_t)x) * 3U;
      if (px[2] == 0) {
        continue;
      }
      px[0] = (uint8_t)(c & 0xFF);
      px[1] = (uint8_t)(c >> 8);
    }
  }
}

void createSpeedDigits(lv_obj_t* parent) {
  const lv_font_t* font = &lv_font_speed_160;
  const int lineH = lv_font_get_line_height(font);
  g_digitCanvasH = lineH + 4;
  g_speedTop = SCREEN_H / 2 + SPEED_OFFSET_Y - lineH / 2 - 2;
  const uint32_t initialColor = ui::internal::speedColor();

  for (int i = 0; i < 11; ++i) {
    const uint32_t ch = i < 10 ? (uint32_t)('0' + i) : (uint32_t)'-';
    const int adv = lv_font_get_glyph_width(font, ch, 0);
    g_digitAdvance[i] = adv;
    g_digitWidth[i] = adv + 16;

    const int w = g_digitWidth[i];
    const size_t bytes =
        LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(w, g_digitCanvasH);
    void* buf = allocCanvasBuffer(bytes);
    if (buf == nullptr) {
      continue;
    }
    g_digitBuffer[i] = (uint8_t*)buf;

    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buf, w, g_digitCanvasH,
                         LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    g_digitCanvas[i] = canvas;

    renderDigit(i, initialColor);
    g_digitDsc[i] = *lv_canvas_get_img(canvas);
  }

  for (int i = 0; i < 3; ++i) {
    g_speedDigits[i] = lv_img_create(parent);
    lv_obj_add_flag(g_speedDigits[i], LV_OBJ_FLAG_HIDDEN);
  }
  g_speedColor = initialColor;
}

void rebuildSpeedDigits(uint32_t color) {
  if (color == g_speedColor) {
    return;
  }
  g_speedColor = color;
  for (int i = 0; i < 11; ++i) {
    renderDigit(i, color);
  }
  for (int i = 0; i < 3; ++i) {
    lv_obj_invalidate(g_speedDigits[i]);
  }
}

void speedHoldCb(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    g_speedHoldActive = true;
    g_speedHoldMs = lv_tick_get();
  } else if (code == LV_EVENT_PRESSING) {
    if (!g_speedHoldActive) {
      return;
    }
    if (lv_tick_elaps(g_speedHoldMs) >= 3000U) {
      g_speedHoldActive = false;
      ui::internal::showSpeedColorMenu();
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    g_speedHoldActive = false;
  }
}

void writeCanvasPixel(SegmentedGauge& g, int x, int y, uint32_t color,
                      lv_opa_t opa) {
  if (x < 0 || y < 0 || x >= g.canvasSize || y >= g.canvasSize) {
    return;
  }
  uint8_t* p =
      g.buffer + ((size_t)y * (size_t)g.canvasSize + (size_t)x) * 3U;
  const uint16_t c = lv_color_to16(lv_color_hex(color));
  p[0] = (uint8_t)(c & 0xFF);
  p[1] = (uint8_t)(c >> 8);
  p[2] = opa;
}

void drawSegment(SegmentedGauge& g, int index, bool active) {
  if (g.buffer == nullptr) {
    return;
  }

  const float a0 = 180.0f + (float)index * g.periodDeg;
  const float a1 = a0 + g.dashDeg;
  const float midAngle = (a0 + a1) * 0.5f;
  const float midRad = midAngle * 3.14159265f / 180.0f;
  const int s = g.canvasSize;
  const int mx = (int)(s + cosf(midRad) * g.radius + 0.5f);
  const int my = (int)(s + sinf(midRad) * g.radius + 0.5f);

  const float halfW = (float)SEGMENT_WIDTH_PX * 0.5f;
  const float halfH = (float)g.width * 0.5f;
  const float corner = (float)SEGMENT_RADIUS_PX;
  const float hx = halfW - corner;
  const float hy = halfH - corner;
  const float rOuter = (float)g.radius + halfH;
  const float rInner = (float)g.radius - halfH;
  const float fadeLen = (float)g.width;

  const float midValue =
      g.scaleMin + (g.scaleMax - g.scaleMin) *
                       ((float)index + 0.5f) / (float)g.segmentCount;
  const uint32_t base = g.colorFn != nullptr ? g.colorFn(midValue) : 0xFFFFFFU;
  constexpr uint32_t GREY = 0x6B7280U;

  const int half = g.width / 2 + 4;
  for (int y = my - half; y <= my + half; ++y) {
    for (int x = mx - half; x <= mx + half; ++x) {
      const float dx = (float)x - (float)s;
      const float dy = (float)y - (float)s;
      const float rr = sqrtf(dx * dx + dy * dy);
      if (rr < rInner - 1.0f || rr > rOuter + 1.0f) {
        continue;
      }

      float ang = atan2f(dy, dx) * 180.0f / 3.14159265f;
      if (ang < 0.0f) {
        ang += 360.0f;
      }
      if (ang < a0 - 1.0f || ang > a1 + 1.0f) {
        continue;
      }

      const float arcPos = (ang - a0) * 3.14159265f / 180.0f * rr;
      const float u = arcPos - halfW;
      const float v = rr - (float)g.radius;
      const float ax = fabsf(u);
      const float ay = fabsf(v);

      bool inside;
      if (ax <= halfW && ay <= halfH) {
        if (ax <= hx || ay <= hy) {
          inside = true;
        } else {
          const float cx = ax - hx;
          const float cy = ay - hy;
          inside = (cx * cx + cy * cy <= corner * corner);
        }
      } else {
        inside = false;
      }
      if (!inside) {
        continue;
      }

      uint32_t color;
      lv_opa_t opa;
      if (active) {
        const float distOuter = rOuter - rr;
        float t = 1.0f - distOuter / fadeLen;
        if (t < 0.0f) {
          t = 0.0f;
        }
        color = lerpColor(0x000000U, base, t);
        opa = LV_OPA_COVER;
      } else {
        const float t = (rOuter - rr) / (float)g.width;
        color = lerpColor(0x000000U, GREY, 1.0f - t);
        opa = (lv_opa_t)(30.0f + 70.0f * (1.0f - t));
      }
      writeCanvasPixel(g, x, y, color, opa);
    }
  }
}

void createSegmentedGauge(lv_obj_t* parent, SegmentedGauge& g, int radius,
                          int width, float scaleMin, float scaleMax,
                          uint32_t (*colorFn)(float)) {
  g.radius = radius;
  g.width = width;
  g.scaleMin = scaleMin;
  g.scaleMax = scaleMax;
  g.colorFn = colorFn;

  const float arcLen = 3.14159265f * 0.5f * (float)radius;
  g.segmentCount =
      (int)(arcLen / (float)(SEGMENT_WIDTH_PX + SEGMENT_GAP_PX));
  if (g.segmentCount < 1) {
    g.segmentCount = 1;
  }
  g.dashDeg = (float)SEGMENT_WIDTH_PX / (float)radius * 180.0f / 3.14159265f;
  g.periodDeg = (float)(SEGMENT_WIDTH_PX + SEGMENT_GAP_PX) / (float)radius *
                180.0f / 3.14159265f;

  g.canvasSize = radius + width / 2 + 1;
  const size_t bufferSize =
      LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(g.canvasSize, g.canvasSize);
  void* buffer = allocCanvasBuffer(bufferSize);
  if (buffer == nullptr) {
    return;
  }
  g.buffer = (uint8_t*)buffer;

  lv_obj_t* canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buffer, g.canvasSize, g.canvasSize,
                       LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_set_pos(canvas, CLUSTER_CX - g.canvasSize,
                 CLUSTER_CY - g.canvasSize);
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
  g.canvas = canvas;

  for (int i = 0; i < g.segmentCount; ++i) {
    drawSegment(g, i, false);
  }
  g.lit = 0;
  lv_obj_invalidate(canvas);
}

void setGaugeFraction(SegmentedGauge& g, float frac) {
  if (g.buffer == nullptr) {
    return;
  }
  if (frac < 0.0f) {
    frac = 0.0f;
  }
  if (frac > 1.0f) {
    frac = 1.0f;
  }

  int lit = (int)(frac * (float)g.segmentCount + 0.5f);
  if (lit < 0) {
    lit = 0;
  }
  if (lit > g.segmentCount) {
    lit = g.segmentCount;
  }
  if (lit == g.lit) {
    return;
  }

  const int from = LV_MIN(g.lit, lit);
  const int to = LV_MAX(g.lit, lit);
  for (int i = from; i < to; ++i) {
    drawSegment(g, i, i < lit);
  }
  g.lit = lit;
  lv_obj_invalidate(g.canvas);
}

void placeAtPolar(lv_obj_t* label, int radius, float angleDeg) {
  const float rad = angleDeg * 3.14159265f / 180.0f;
  const int x = CLUSTER_CX + (int)(cosf(rad) * radius + 0.5f);
  const int y = CLUSTER_CY + (int)(sinf(rad) * radius + 0.5f);
  lv_obj_align(label, LV_ALIGN_CENTER, x - SCREEN_W / 2, y - SCREEN_H / 2);
}

lv_obj_t* addArcTick(lv_obj_t* parent, int rInner, int rOuter, float angleDeg,
                     int width, uint32_t color) {
  if (g_arcTickPointCount >= kMaxArcTicks) {
    return nullptr;
  }

  const float rad = angleDeg * 3.14159265f / 180.0f;
  lv_point_t* pts = g_arcTickPoints[g_arcTickPointCount++];
  pts[0].x = (lv_coord_t)(CLUSTER_CX + cosf(rad) * rInner);
  pts[0].y = (lv_coord_t)(CLUSTER_CY + sinf(rad) * rInner);
  pts[1].x = (lv_coord_t)(CLUSTER_CX + cosf(rad) * rOuter);
  pts[1].y = (lv_coord_t)(CLUSTER_CY + sinf(rad) * rOuter);

  const int pad = width / 2 + 2;
  const lv_coord_t x1 = LV_MIN(pts[0].x, pts[1].x) - pad;
  const lv_coord_t y1 = LV_MIN(pts[0].y, pts[1].y) - pad;
  const lv_coord_t x2 = LV_MAX(pts[0].x, pts[1].x) + pad;
  const lv_coord_t y2 = LV_MAX(pts[0].y, pts[1].y) + pad;
  pts[0].x -= x1;
  pts[0].y -= y1;
  pts[1].x -= x1;
  pts[1].y -= y1;

  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, pts, 2);
  lv_obj_set_size(line, x2 - x1 + 1, y2 - y1 + 1);
  lv_obj_set_pos(line, x1, y1);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  return line;
}

void createTach(lv_obj_t* parent) {
  createSegmentedGauge(parent, g_tachGauge, TACH_R, TACH_ARC_W, 0.0f, TACH_MAX,
                       rpmColor);

  const int rOuter = TACH_R - TACH_ARC_W / 2 - 2;
  constexpr int kTickCount = (int)(TACH_MAX / 500.0f) + 1;
  for (int i = 0; i < kTickCount; ++i) {
    const float value = i * 500.0f;
    const float frac = value / TACH_MAX;
    const float angle = 180.0f + 90.0f * frac;
    const bool major = (i % 2) == 0;
    const bool red = value >= 4500.0f;
    const int rInner = rOuter - (major ? 16 : 11);

    addArcTick(parent, rInner, rOuter, angle, major ? 5 : 4,
               red ? COL_RED : COL_TICK);
  }

  for (int v = 0; v <= 5000; v += 1000) {
    char text[4];
    snprintf(text, sizeof(text), "%d", v / 1000);
    const float angle = 180.0f + 90.0f * (float)v / TACH_MAX;
    lv_obj_t* label =
        makeLabel(parent, text, &lv_font_montserrat_24, COL_DIM);
    placeAtPolar(label, 310, angle);
  }
}

uint32_t tempColor(float value) {
  if (value <= 70.0f) {
    return lerpColor(COL_BLUE, COL_GREEN, (value - 40.0f) / 30.0f);
  }
  if (value <= 90.0f) {
    return COL_GREEN;
  }
  if (value < 110.0f) {
    return lerpColor(COL_GREEN, COL_RED, (value - 90.0f) / 20.0f);
  }
  return COL_RED;
}

void createTemp(lv_obj_t* parent) {
  createSegmentedGauge(parent, g_tempGauge, TEMP_R, TEMP_ARC_W, TEMP_MIN_C,
                       TEMP_MAX_C, tempColor);

  const struct {
    float value;
    const char* text;
  } labels[] = {{75.0f, "75"}, {90.0f, "90"}, {110.0f, "110"}};

  for (const auto& item : labels) {
    const float angle =
        180.0f + (item.value - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C) * 90.0f;
    lv_obj_t* label =
        makeLabel(parent, item.text, &lv_font_montserrat_20, COL_DIM);
    placeAtPolar(label, 230, angle);
  }

  const int rOuter = TEMP_R - TEMP_ARC_W / 2 - 2;
  const struct {
    float value;
    bool major;
  } ticks[] = {{60.0f, false}, {75.0f, true}, {90.0f, true},
               {110.0f, true}, {120.0f, false}};
  for (const auto& item : ticks) {
    const float angle =
        180.0f + (item.value - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C) * 90.0f;
    const int rInner = rOuter - (item.major ? 14 : 8);
    addArcTick(parent, rInner, rOuter, angle, item.major ? 4 : 3, COL_TICK);
  }
}

uint32_t fuelColor(float value) {
  if (value <= 12.0f) {
    return COL_RED;
  }
  if (value <= 30.0f) {
    return COL_ORANGE;
  }
  if (value <= 50.0f) {
    return COL_YELLOW;
  }
  return COL_GREEN;
}

void createFuel(lv_obj_t* parent) {
  createSegmentedGauge(parent, g_fuelGauge, FUEL_R, FUEL_ARC_W, 0.0f, 100.0f,
                       fuelColor);

  const struct {
    float value;
    const char* text;
  } labels[] = {{0.0f, "0"}, {50.0f, "1/2"}, {100.0f, "1"}};

  for (const auto& item : labels) {
    const float angle = 180.0f + item.value / 100.0f * 90.0f;
    lv_obj_t* label =
        makeLabel(parent, item.text, &lv_font_montserrat_20, COL_DIM);
    placeAtPolar(label, 125, angle);
  }

  const int rOuter = FUEL_R - FUEL_ARC_W / 2 - 2;
  const struct {
    float value;
    bool major;
  } ticks[] = {{0.0f, true}, {25.0f, false}, {50.0f, true},
               {75.0f, false}, {100.0f, true}};
  for (const auto& item : ticks) {
    const float angle = 180.0f + item.value / 100.0f * 90.0f;
    const int rInner = rOuter - (item.major ? 12 : 7);
    addArcTick(parent, rInner, rOuter, angle, item.major ? 4 : 3, COL_TICK);
  }
}

void updateTach(const Values& v) {
  float frac = v.rpmValid ? v.rpm / TACH_MAX : 0.0f;
  setGaugeFraction(g_tachGauge, frac);
}

void updateTemp(const Values& v) {
  float frac = 0.0f;
  if (v.coolantValid) {
    float temp = v.coolantC;
    if (temp < TEMP_MIN_C) {
      temp = TEMP_MIN_C;
    }
    if (temp > TEMP_MAX_C) {
      temp = TEMP_MAX_C;
    }
    frac = (temp - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C);
  }
  setGaugeFraction(g_tempGauge, frac);
}

void updateFuel(const Values& v) {
  float frac = v.fuelValid ? v.fuelPct / 100.0f : 0.0f;
  setGaugeFraction(g_fuelGauge, frac);
}

lv_obj_t* createLamp(lv_obj_t* parent, const lv_img_dsc_t* src, int cx, int cy) {
  lv_obj_t* icon = lv_img_create(parent);
  lv_img_set_src(icon, src);
  const int iw = (int)src->header.w;
  const int ih = (int)src->header.h;
  lv_obj_set_pos(icon, cx - iw / 2, cy - ih / 2);
  lv_obj_set_style_img_opa(icon, LV_OPA_20, 0);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  return icon;
}

lv_obj_t* createTurn(lv_obj_t* parent, const lv_img_dsc_t* src, int cx, int cy) {
  lv_obj_t* arrow = lv_img_create(parent);
  lv_img_set_src(arrow, src);
  const int iw = (int)src->header.w;
  const int ih = (int)src->header.h;
  lv_obj_set_pos(arrow, cx - iw / 2, cy - ih / 2);
  lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  return arrow;
}

void updateLamps(const Values& v) {
  setIconState(g_iconOil, v.oilWarning);
  setIconState(g_iconChg, v.chargeWarning);
  setIconState(g_iconGlow, v.glowPlug);
  setIconState(g_iconFog, v.fogLight);

  bool showBrakeFault = v.brakeFault;
  if (v.brakeFault && v.parkingBrake) {
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
}

void updateTurns(const Values& v) {
  const bool blinkOn = ((lv_tick_get() / 350U) & 1U) == 0U;
  const bool leftOn = v.turnLeft && blinkOn;
  const bool rightOn = v.turnRight && blinkOn;
  if (leftOn) {
    lv_obj_clear_flag(g_turnLeft, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_turnLeft, LV_OBJ_FLAG_HIDDEN);
  }
  if (rightOn) {
    lv_obj_clear_flag(g_turnRight, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_turnRight, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace

namespace ui {
namespace internal {

void designBInit(lv_obj_t* parent) {
  g_arcTickPointCount = 0;
  g_root = lv_obj_create(parent);
  lv_obj_remove_style_all(g_root);
  lv_obj_set_pos(g_root, 0, 0);
  lv_obj_set_size(g_root, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);

  createSpeedDigits(g_root);

  lv_obj_t* speedHit = lv_obj_create(g_root);
  lv_obj_remove_style_all(speedHit);
  lv_obj_set_pos(speedHit, 20, 90);
  lv_obj_set_size(speedHit, 360, 160);
  lv_obj_set_style_bg_opa(speedHit, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(speedHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(speedHit, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(speedHit, speedHoldCb, LV_EVENT_ALL, nullptr);

  lv_obj_t* unit = makeLabel(g_root, "km/h", &lv_font_montserrat_20, COL_DIM);
  lv_obj_set_pos(unit, SPEED_CX - 26, 246);

  g_clockLabel = makeLabel(g_root, "--:--", &lv_font_montserrat_32, COL_TEXT);
  lv_obj_align_to(g_clockLabel, unit, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

  g_odoLabel = makeLabel(g_root, "0.0 km", &lv_font_montserrat_20, COL_DIM);
  lv_obj_align_to(g_odoLabel, g_clockLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

  createTach(g_root);
  createTemp(g_root);
  createFuel(g_root);

  g_iconOil = createLamp(g_root, &icon_oil, LAMP_FIRST_X, LAMP_Y);
  g_iconChg =
      createLamp(g_root, &icon_charge, LAMP_FIRST_X + LAMP_STEP, LAMP_Y);
  g_iconBrake =
      createLamp(g_root, &icon_brake, LAMP_FIRST_X + 2 * LAMP_STEP, LAMP_Y);
  g_iconBrakeFault = createLamp(g_root, &icon_brake_fault,
                                LAMP_FIRST_X + 2 * LAMP_STEP, LAMP_Y);
  lv_obj_set_style_img_opa(g_iconBrakeFault, LV_OPA_0, 0);
  g_iconGlow =
      createLamp(g_root, &icon_glow, LAMP_FIRST_X + 3 * LAMP_STEP, LAMP_Y);
  g_iconFog =
      createLamp(g_root, &icon_anti_fog, LAMP_FIRST_X + 4 * LAMP_STEP, LAMP_Y);
  g_iconLowBeam =
      createLamp(g_root, &icon_beam_low, LAMP_FIRST_X + 5 * LAMP_STEP, LAMP_Y);
  g_iconBeam =
      createLamp(g_root, &icon_beam_high, LAMP_FIRST_X + 5 * LAMP_STEP, LAMP_Y);
  lv_obj_set_style_img_opa(g_iconLowBeam, LV_OPA_0, 0);

  g_turnLeft = createTurn(g_root, &icon_arrow_left, 325, 24);
  g_turnRight = createTurn(g_root, &icon_arrow_right, 475, 24);
}

void designBSetVisible(bool visible) {
  if (g_root == nullptr) {
    return;
  }
  if (visible) {
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);
  }
}

void designBUpdate(const Values& v) {
  if (g_root == nullptr) {
    return;
  }
  const uint32_t color = ui::internal::speedColor();
  if (color != g_speedColor) {
    rebuildSpeedDigits(color);
  }
  updateSpeed(v);
  updateClockAndOdo(v);
  updateTach(v);
  updateTemp(v);
  updateFuel(v);
  updateLamps(v);
  updateTurns(v);
}

}  // namespace internal
}  // namespace ui
