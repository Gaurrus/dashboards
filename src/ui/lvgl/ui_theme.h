#pragma once

#include <stdint.h>

#include "lvgl.h"

// Shared dashboard palette and small UI helpers.
namespace ui {
namespace theme {

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
constexpr uint32_t COL_YELLOW_BRIGHT = 0xFFE600;
constexpr uint32_t COL_BLUE = 0x3B82F6;
constexpr uint32_t COL_TICK = 0x9CA3AF;
constexpr uint32_t COL_TURN = 0x2ECC71;

inline uint32_t lerpColor(uint32_t from, uint32_t to, float t) {
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

inline uint32_t rpmColor(float value) {
  if (value <= 2500.0f) {
    return COL_DARK_GREEN;
  }
  if (value >= 4500.0f) {
    return COL_RED;
  }
  if (value < 3000.0f) {
    return lerpColor(COL_DARK_GREEN, COL_GREEN, (value - 2500.0f) / 500.0f);
  }
  if (value < 3500.0f) {
    return lerpColor(COL_GREEN, COL_YELLOW, (value - 3000.0f) / 500.0f);
  }
  if (value < 4000.0f) {
    return lerpColor(COL_YELLOW, COL_ORANGE, (value - 3500.0f) / 500.0f);
  }
  return lerpColor(COL_ORANGE, COL_RED, (value - 4000.0f) / 500.0f);
}

inline lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                           uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

}  // namespace theme
}  // namespace ui
