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
constexpr uint32_t COL_BLUE = 0x3B82F6;
constexpr uint32_t COL_TICK = 0x9CA3AF;
constexpr uint32_t COL_TURN = 0x2ECC71;

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
