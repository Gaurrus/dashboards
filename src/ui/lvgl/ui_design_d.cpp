#include "ui_design_d.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "icons.h"
#include "ui_bye.h"
#include "ui_clock.h"
#include "ui_lvgl.h"
#include "ui_theme.h"

namespace {

using namespace ui::theme;

constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;
constexpr float kScale = 768.0f / 606.0f;
constexpr int WELL_RADIUS = 127;
constexpr int WELL_BORDER = 5;
constexpr int LEFT_CX = 147;
constexpr int RIGHT_CX = 653;
constexpr int WELL_CY = 250;
constexpr int CONTENT = WELL_RADIUS - WELL_BORDER;
constexpr int OLD_CONTENT = 96;
constexpr int BAR_W = 9;
constexpr int BAR_H = 89;
constexpr uint32_t COL_WHITE = 0xFFFFFFU;

int sx(int x) {
  return (int)lroundf(400.0f + (float)(x - 400) * kScale);
}
int sy(int y) {
  return (int)lroundf(250.0f + (float)(y - 250) * kScale);
}
int sc(int v) {
  return CONTENT + (int)lroundf((float)(v - OLD_CONTENT) * kScale);
}

constexpr int kMaxTicks = 64;
lv_point_t g_tickPoints[kMaxTicks][2];
int g_tickPointCount = 0;

constexpr int kRuleLayers = 13;
constexpr int kRuleOutline = 3;
lv_point_t g_topRuleLayers[kRuleLayers][6];
lv_point_t g_bottomRuleLayers[kRuleLayers][6];

struct Well {
  lv_obj_t* box = nullptr;
  lv_obj_t* shaft = nullptr;
  lv_point_t shaftPts[2] = {};
  float lastFrac = -1.0f;
};

lv_obj_t* g_root = nullptr;
Well g_speedWell;
Well g_tachWell;



lv_obj_t* g_iconOil = nullptr;
lv_obj_t* g_iconChg = nullptr;
lv_obj_t* g_iconGlow = nullptr;
lv_obj_t* g_iconBrake = nullptr;
lv_obj_t* g_iconBrakeFault = nullptr;
lv_obj_t* g_iconLowBeam = nullptr;
lv_obj_t* g_iconBeam = nullptr;
lv_obj_t* g_iconFog = nullptr;
lv_obj_t* g_turnLeft = nullptr;
lv_obj_t* g_turnRight = nullptr;

lv_obj_t* g_tempValue = nullptr;
lv_obj_t* g_tempFill = nullptr;
lv_obj_t* g_odoLabel = nullptr;
lv_obj_t* g_clockLabel = nullptr;
lv_obj_t* g_fuelValue = nullptr;
lv_obj_t* g_fuelFill = nullptr;

int g_tempFillH = -1;
int g_fuelFillH = -1;

lv_point_t g_topRulePts[6] = {};
lv_point_t g_bottomRulePts[6] = {};

void setImgOpa(lv_obj_t* obj, lv_opa_t opa) {
  if (obj == nullptr || lv_obj_get_style_img_opa(obj, 0) == opa) {
    return;
  }
  lv_obj_set_style_img_opa(obj, opa, 0);
}

void setIconState(lv_obj_t* icon, bool on) {
  setImgOpa(icon, on ? LV_OPA_COVER : LV_OPA_20);
}

void setLabelText(lv_obj_t* label, const char* text) {
  if (label == nullptr) {
    return;
  }
  const char* current = lv_label_get_text(label);
  if (current == nullptr || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
  }
}

lv_obj_t* addTick(lv_obj_t* parent, int cx, int cy, float angleDeg, int rInner,
                  int rOuter, int width, uint32_t color = COL_WHITE) {
  if (g_tickPointCount >= kMaxTicks) {
    return nullptr;
  }

  const float rad = angleDeg * 3.14159265f / 180.0f;
  lv_point_t* pts = g_tickPoints[g_tickPointCount++];
  pts[0].x = (lv_coord_t)(cx + cosf(rad) * rInner);
  pts[0].y = (lv_coord_t)(cy + sinf(rad) * rInner);
  pts[1].x = (lv_coord_t)(cx + cosf(rad) * rOuter);
  pts[1].y = (lv_coord_t)(cy + sinf(rad) * rOuter);

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

void createWell(lv_obj_t* parent, Well& well, int cx, int cy, bool speed) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, cx - WELL_RADIUS, cy - WELL_RADIUS);
  lv_obj_set_size(box, WELL_RADIUS * 2, WELL_RADIUS * 2);
  lv_obj_set_style_radius(box, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(box, lv_color_hex(0x0E1216), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, WELL_BORDER, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(COL_WHITE), 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
  well.box = box;

  // Children are positioned relative to the content area, which is inset by
  // the 6 px ring border. The hub is at the content centre.
  const int c = WELL_RADIUS - WELL_BORDER;
  const float startDeg = 135.0f;
  const float sweepDeg = 270.0f;

  if (speed) {
    for (int i = 0; i <= 18; ++i) {
      const float value = (float)(i * 10);
      const float angle = startDeg + sweepDeg * value / 180.0f;
      const bool major = (i % 2) == 0;
      addTick(box, c, c, angle, major ? 106 : 112, 118, major ? 3 : 1);
      if (major) {
        char text[8];
        snprintf(text, sizeof(text), "%d", (int)value);
        const float rad = angle * 3.14159265f / 180.0f;
        lv_obj_t* label =
            makeLabel(box, text, &lv_font_montserrat_14, COL_WHITE);
        lv_obj_set_width(label, 30);
        lv_obj_align(label, LV_ALIGN_CENTER, (int)(cosf(rad) * 95.0f),
                     (int)(sinf(rad) * 95.0f));
      }
    }
  } else {
    const int zoneOuter = 118;
    const int zoneWidth = 12;
    lv_obj_t* zone = lv_arc_create(box);
    lv_obj_remove_style_all(zone);
    lv_obj_set_size(zone, zoneOuter * 2, zoneOuter * 2);
    lv_obj_set_pos(zone, c - zoneOuter, c - zoneOuter);
    lv_arc_set_bg_angles(zone, 0, 360);
    lv_arc_set_angles(zone, 356, 405);
    lv_obj_set_style_arc_width(zone, zoneWidth, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(zone, lv_color_hex(COL_RED), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(zone, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(zone, false, LV_PART_INDICATOR);
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i <= 11; ++i) {
      const float value = (float)(i * 500);
      const float angle = startDeg + sweepDeg * value / 5500.0f;
      const bool major = (i % 2) == 0;
      const uint32_t color = value >= 4500.0f ? COL_RED : COL_WHITE;
      addTick(box, c, c, angle, major ? 106 : 112, 118, major ? 3 : 1, color);
      if (major) {
        char text[8];
        snprintf(text, sizeof(text), "%d", i / 2);
        const float rad = angle * 3.14159265f / 180.0f;
        lv_obj_t* label =
            makeLabel(box, text, &lv_font_montserrat_14, color);
        lv_obj_set_width(label, 15);
        lv_obj_align(label, LV_ALIGN_CENTER, (int)(cosf(rad) * 93.0f),
                     (int)(sinf(rad) * 93.0f));
      }
    }
  }

  auto makeNeedleLine = [&]() {
    lv_obj_t* line = lv_line_create(box);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(COL_WHITE), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
  };
  well.shaft = makeNeedleLine();

  lv_obj_t* hub = lv_obj_create(box);
  lv_obj_remove_style_all(hub);
  lv_obj_set_size(hub, 3, 3);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(COL_WHITE), 0);
  lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
  lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
}

void setLinePoints(lv_obj_t* line, lv_point_t* pts, lv_point_t a,
                   lv_point_t b) {
  const int pad = 4;
  const lv_coord_t x1 = LV_MIN(a.x, b.x) - pad;
  const lv_coord_t y1 = LV_MIN(a.y, b.y) - pad;
  const lv_coord_t x2 = LV_MAX(a.x, b.x) + pad;
  const lv_coord_t y2 = LV_MAX(a.y, b.y) + pad;
  pts[0].x = (lv_coord_t)(a.x - x1);
  pts[0].y = (lv_coord_t)(a.y - y1);
  pts[1].x = (lv_coord_t)(b.x - x1);
  pts[1].y = (lv_coord_t)(b.y - y1);
  lv_line_set_points(line, pts, 2);
  lv_obj_set_pos(line, x1, y1);
  lv_obj_set_size(line, x2 - x1 + 1, y2 - y1 + 1);
}

void setNeedle(Well& well, float frac) {
  if (well.shaft == nullptr) {
    return;
  }
  if (frac < 0.0f) {
    frac = 0.0f;
  }
  if (frac > 1.0f) {
    frac = 1.0f;
  }
  if (fabsf(frac - well.lastFrac) < 0.0005f) {
    return;
  }
  well.lastFrac = frac;

  const float angle = 135.0f + 270.0f * frac;
  const float rad = angle * 3.14159265f / 180.0f;
  const int cx = WELL_RADIUS - WELL_BORDER;
  const int cy = WELL_RADIUS - WELL_BORDER;
  const int len = 105;
  const lv_point_t center = {(lv_coord_t)cx, (lv_coord_t)cy};
  const lv_point_t tip = {
      (lv_coord_t)(cx + (int)(cosf(rad) * len + 0.5f)),
      (lv_coord_t)(cy + (int)(sinf(rad) * len + 0.5f))};
  setLinePoints(well.shaft, well.shaftPts, center, tip);
}

lv_obj_t* createLamp(lv_obj_t* parent, const lv_img_dsc_t* src, int cx, int cy) {
  lv_obj_t* icon = lv_img_create(parent);
  lv_img_set_src(icon, src);
  const int iw = (int)src->header.w;
  const int ih = (int)src->header.h;
  lv_obj_set_pos(icon, cx - iw / 2, cy - ih / 2);
  lv_img_set_zoom(icon, 215);
  lv_obj_set_style_img_opa(icon, LV_OPA_20, 0);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  return icon;
}

lv_obj_t* createTurn(lv_obj_t* parent, const lv_img_dsc_t* src, int x, int y,
                     int zoom) {
  lv_obj_t* arrow = lv_img_create(parent);
  lv_img_set_src(arrow, src);
  lv_obj_set_pos(arrow, x, y);
  lv_img_set_zoom(arrow, zoom);
  lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  return arrow;
}

lv_obj_t* createVBar(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* track = lv_obj_create(parent);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, x, y);
  lv_obj_set_size(track, w, h);
  lv_obj_set_style_radius(track, 0, 0);
  lv_obj_set_style_bg_color(track, lv_color_hex(COL_WHITE), 0);
  lv_obj_set_style_bg_opa(track, (lv_opa_t)40, 0);
  lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* fill = lv_obj_create(track);
  lv_obj_remove_style_all(fill);
  lv_obj_set_pos(fill, 0, h);
  lv_obj_set_size(fill, w, 0);
  lv_obj_set_style_radius(fill, 0, 0);
  lv_obj_set_style_bg_color(fill, lv_color_hex(COL_WHITE), 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(fill, LV_OBJ_FLAG_CLICKABLE);
  return fill;
}

void clockTapCb(lv_event_t* /*e*/) {
  if (ui::internal::byeActive()) {
    return;
  }
  ui::internal::clockShowSettings();
}

lv_obj_t* createSubPanel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, w, h);
  lv_obj_set_style_radius(panel, 5, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(COL_TRACK), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  return panel;
}

void updateTemp(const Values& v) {
  char text[8];
  if (v.coolantValid) {
    snprintf(text, sizeof(text), "%d", (int)(v.coolantC + 0.5f));
  } else {
    snprintf(text, sizeof(text), "--");
  }
  setLabelText(g_tempValue, text);

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
  const int height = (int)((float)BAR_H * frac + 0.5f);
  if (height != g_tempFillH) {
    g_tempFillH = height;
    lv_obj_set_pos(g_tempFill, 0, BAR_H - height);
    lv_obj_set_size(g_tempFill, BAR_W, height);
  }
}

void updateFuel(const Values& v) {
  char text[8];
  if (v.fuelValid) {
    snprintf(text, sizeof(text), "%d%%", (int)(v.fuelPct + 0.5f));
  } else {
    snprintf(text, sizeof(text), "--");
  }
  setLabelText(g_fuelValue, text);

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
  const int height = (int)((float)BAR_H * frac + 0.5f);
  if (height != g_fuelFillH) {
    g_fuelFillH = height;
    lv_obj_set_pos(g_fuelFill, 0, BAR_H - height);
    lv_obj_set_size(g_fuelFill, BAR_W, height);
  }
}

}  // namespace

namespace ui {
namespace internal {

void designDInit(lv_obj_t* parent) {
  g_tickPointCount = 0;
  g_root = lv_obj_create(parent);
  lv_obj_remove_style_all(g_root);
  lv_obj_set_pos(g_root, 0, 0);
  lv_obj_set_size(g_root, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);


  createWell(g_root, g_speedWell, LEFT_CX, WELL_CY, true);
  createWell(g_root, g_tachWell, RIGHT_CX, WELL_CY, false);

  auto makeOuterRing = [&](int cx) {
    const int outer = WELL_RADIUS + 4;
    lv_obj_t* ring = lv_obj_create(g_root);
    lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring, cx - outer, WELL_CY - outer);
    lv_obj_set_size(ring, outer * 2, outer * 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(COL_WHITE), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  };
  makeOuterRing(LEFT_CX);
  makeOuterRing(RIGHT_CX);

  const lv_point_t topSrc[6] = {{97, 129},  {280, 129}, {338, 167},
                                {462, 167}, {520, 129}, {703, 129}};
  const lv_point_t bottomSrc[6] = {{97, 371},  {280, 371}, {338, 321},
                                   {462, 321}, {520, 371}, {703, 371}};
  for (int i = 0; i < 6; i++) {
    g_topRulePts[i] = {(lv_coord_t)sx(topSrc[i].x), (lv_coord_t)sy(topSrc[i].y)};
    g_bottomRulePts[i] = {(lv_coord_t)sx(bottomSrc[i].x),
                          (lv_coord_t)sy(bottomSrc[i].y)};
  }

  auto makeRule = [&](lv_point_t* pts, lv_point_t layers[kRuleLayers][6],
                      bool brightAtBottom) {
    constexpr float kFadedTint = 0.15f;
    for (int layer = 0; layer < kRuleLayers; layer++) {
      const int offset = layer - kRuleLayers / 2;
      float t = (float)layer / (float)(kRuleLayers - 1);
      if (!brightAtBottom) {
        t = 1.0f - t;
      }
      const float tint = kFadedTint + (1.0f - kFadedTint) * t;
      for (int i = 0; i < 6; i++) {
        layers[layer][i].x = pts[i].x;
        layers[layer][i].y = pts[i].y + offset;
      }

      lv_obj_t* rule = lv_line_create(g_root);
      lv_obj_set_pos(rule, 0, 0);
      lv_obj_set_size(rule, SCREEN_W, SCREEN_H);
      lv_line_set_points(rule, layers[layer], 6);
      lv_obj_set_style_line_width(rule, 1, 0);
      lv_obj_set_style_line_color(rule, lv_color_hex(lerpColor(COL_BG, COL_BLUE, tint)), 0);
      lv_obj_set_style_line_rounded(rule, false, 0);
      lv_obj_clear_flag(rule, LV_OBJ_FLAG_CLICKABLE);
    }

    for (int i = 0; i < kRuleOutline; i++) {
      const int edge = brightAtBottom ? (kRuleLayers - 1 - i) : i;
      lv_obj_t* outline = lv_line_create(g_root);
      lv_obj_set_pos(outline, 0, 0);
      lv_obj_set_size(outline, SCREEN_W, SCREEN_H);
      lv_line_set_points(outline, layers[edge], 6);
      lv_obj_set_style_line_width(outline, 1, 0);
      lv_obj_set_style_line_color(outline, lv_color_hex(COL_WHITE), 0);
      lv_obj_set_style_line_rounded(outline, false, 0);
      lv_obj_clear_flag(outline, LV_OBJ_FLAG_CLICKABLE);
    }
  };
  makeRule(g_topRulePts, g_topRuleLayers, true);
  makeRule(g_bottomRulePts, g_bottomRuleLayers, false);

  g_iconOil = createLamp(g_speedWell.box, &icon_oil, sc(47), sc(73));
  g_iconChg = createLamp(g_speedWell.box, &icon_charge, sc(69), sc(50));
  g_iconGlow = createLamp(g_speedWell.box, &icon_glow, sc(96), sc(43));
  g_iconBrake = createLamp(g_speedWell.box, &icon_brake, sc(123), sc(50));
  g_iconBrakeFault = createLamp(g_speedWell.box, &icon_brake_fault, sc(145), sc(73));

  g_iconLowBeam = createLamp(g_tachWell.box, &icon_beam_low, sc(58), sc(58));
  g_iconBeam = createLamp(g_tachWell.box, &icon_beam_high, sc(96), sc(43));
  g_iconFog = createLamp(g_tachWell.box, &icon_anti_fog, sc(134), sc(58));

  lv_obj_t* speedPanel = createSubPanel(g_speedWell.box, sc(56), sc(102), 101, 34);
  g_odoLabel = makeLabel(speedPanel, "0.0 km", &lv_font_montserrat_14,
                         COL_WHITE);
  lv_obj_center(g_odoLabel);
  lv_obj_move_background(speedPanel);

  lv_obj_t* tachPanel = createSubPanel(g_tachWell.box, sc(56), sc(102), 101, 34);
  g_clockLabel = makeLabel(tachPanel, "--:--", &lv_font_montserrat_20,
                           COL_WHITE);
  lv_obj_center(g_clockLabel);
  lv_obj_add_flag(tachPanel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(tachPanel, clockTapCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_move_background(tachPanel);

  lv_obj_t* tempIcon = lv_img_create(g_root);
  lv_img_set_src(tempIcon, &icon_temp);
  lv_obj_set_pos(tempIcon, 302, 178);
  lv_img_set_zoom(tempIcon, 217);
  lv_obj_clear_flag(tempIcon, LV_OBJ_FLAG_CLICKABLE);
  g_tempValue = makeLabel(g_root, "--", &lv_font_montserrat_10, COL_WHITE);
  lv_obj_align(g_tempValue, LV_ALIGN_TOP_MID, -85, sy(289));
  g_tempFill = createVBar(g_root, sx(330), sy(215), BAR_W, BAR_H);

  lv_obj_t* fuelIcon = lv_img_create(g_root);
  lv_img_set_src(fuelIcon, &icon_fuel);
  lv_obj_set_pos(fuelIcon, 474, 178);
  lv_img_set_zoom(fuelIcon, 217);
  lv_obj_clear_flag(fuelIcon, LV_OBJ_FLAG_CLICKABLE);
  g_fuelValue = makeLabel(g_root, "--", &lv_font_montserrat_10, COL_WHITE);
  lv_obj_align(g_fuelValue, LV_ALIGN_TOP_MID, 85, sy(289));
  g_fuelFill = createVBar(g_root, sx(463), sy(215), BAR_W, BAR_H);

  lv_obj_t* vwLogo = lv_img_create(g_root);
  lv_img_set_src(vwLogo, &icon_vw_logo);
  lv_obj_set_pos(vwLogo, 352, 202);
  lv_obj_clear_flag(vwLogo, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* vwWordmark =
      makeLabel(g_root, "transporter", &lv_font_montserrat_20, 0x001E50);
  lv_obj_align(vwWordmark, LV_ALIGN_TOP_MID, 0, 393);

  g_turnLeft = createTurn(g_root, &icon_arrow_left, 23, 21, 322);
  g_turnRight = createTurn(g_root, &icon_arrow_right, 723, 21, 322);
}

void designDSetVisible(bool visible) {
  if (g_root == nullptr) {
    return;
  }
  if (visible) {
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);
  }
}

void designDUpdate(const Values& v) {
  if (g_root == nullptr) {
    return;
  }

  const float speedFrac = v.speedValid ? v.speedKmh / 180.0f : 0.0f;
  setNeedle(g_speedWell, speedFrac);

  const float rpmFrac = v.rpmValid ? v.rpm / 5500.0f : 0.0f;
  setNeedle(g_tachWell, rpmFrac);

  updateTemp(v);
  updateFuel(v);

  char text[16];
  snprintf(text, sizeof(text), "%.1f km", v.odometerKm);
  setLabelText(g_odoLabel, text);

  const uint32_t seconds = ui::clockSeconds();
  snprintf(text, sizeof(text), "%02u:%02u", (unsigned)(seconds / 3600U),
           (unsigned)((seconds / 60U) % 60U));
  setLabelText(g_clockLabel, text);

  setIconState(g_iconOil, v.oilWarning);
  setIconState(g_iconChg, v.chargeWarning);
  setIconState(g_iconGlow, v.glowPlug);
  setIconState(g_iconBrake, v.parkingBrake);
  setIconState(g_iconBrakeFault, v.brakeFault);

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
  setIconState(g_iconFog, v.fogLight);

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

}  // namespace internal
}  // namespace ui
