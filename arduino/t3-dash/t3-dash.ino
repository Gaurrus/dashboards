/*
 * VW T3 dashboard for Waveshare ESP32-S3-Touch-LCD-7.
 *
 * Arduino IDE:
 *   - board package: ESP32 by Espressif (3.x) or Waveshare ESP32-S3-Touch-LCD-7
 *   - board: ESP32S3 Dev Module, Flash 16MB, PSRAM OPI
 *   - libraries: ESP32_Display_Panel, ESP32_IO_Expander, esp-lib-utils, LVGL 8.4.0
 */

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>

#include "demo_values.h"
#include "lvgl_v8_port.h"
#include "ui_lvgl.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

namespace {

float gDemoTime = 0.0f;

void demoUpdateCb(lv_timer_t* /*timer*/) {
  gDemoTime += 0.05f;
  ui::update(demo::make(gDemoTime));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("VW T3 dashboard: Waveshare ESP32-S3-Touch-LCD-7");

  Board* board = new Board();
  board->init();
#if LVGL_PORT_AVOID_TEARING_MODE
  auto lcd = board->getLCD();
  lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
  auto lcd_bus = lcd->getBus();
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    static_cast<BusRGB*>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
  }
#endif
#endif
  assert(board->begin());

  Serial.println("Initializing LVGL");
  lvgl_port_init(board->getLCD(), board->getTouch());

  Serial.println("Creating UI");
  lvgl_port_lock(-1);
  ui::init();
  lv_timer_create(demoUpdateCb, 50, nullptr);
  lvgl_port_unlock();
}

void loop() {
  delay(1000);
}
