/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <Arduino.h>
#include <Preferences.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <stdio.h>

#include "demo_values.h"
#include "lvgl_v8_port.h"
#include "ui_lvgl.h"
#include "version.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

namespace {

constexpr uint32_t CLOCK_SAVE_MS = 10UL * 60UL * 1000UL;

float gDemoTime = 0.0f;
Preferences gPrefs;
String gSerialLine;
uint32_t gLastClockSaveMs = 0;

void saveClock() {
  lvgl_port_lock(-1);
  const uint32_t seconds = ui::clockSeconds();
  lvgl_port_unlock();
  gPrefs.putUInt("clock", seconds);
}

void handleSerial() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (gSerialLine.startsWith("time ")) {
        int h = 0;
        int m = 0;
        int s = 0;
        const int n = sscanf(gSerialLine.c_str() + 5, "%d:%d:%d", &h, &m, &s);
        if (n >= 2 && h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
          lvgl_port_lock(-1);
          ui::setClock(h, m, s);
          lvgl_port_unlock();
          Serial.printf("Clock set to %02d:%02d:%02d\n", h, m, s);
        } else {
          Serial.println("Usage: time HH:MM[:SS]");
        }
      } else if (gSerialLine == "time") {
        lvgl_port_lock(-1);
        const uint32_t seconds = ui::clockSeconds();
        lvgl_port_unlock();
        Serial.printf("Clock: %02u:%02u:%02u\n", (unsigned)(seconds / 3600U),
                      (unsigned)((seconds / 60U) % 60U), (unsigned)(seconds % 60U));
      }
      gSerialLine = "";
    } else if (gSerialLine.length() < 32) {
      gSerialLine += c;
    }
  }
}

void demoUpdateCb(lv_timer_t* /*timer*/) {
  gDemoTime += 0.05f;
  ui::update(demo::make(gDemoTime));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("VW T3 dashboard v%s: Waveshare ESP32-S3-Touch-LCD-7\n", T3DASH_VERSION);

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

  gPrefs.begin("t3dash", false);
  const uint32_t savedClock = gPrefs.getUInt("clock", 0xFFFFFFFFU);
  if (savedClock != 0xFFFFFFFFU) {
    lvgl_port_lock(-1);
    ui::setClock((int)(savedClock / 3600U), (int)((savedClock / 60U) % 60U),
                 (int)(savedClock % 60U));
    lvgl_port_unlock();
  }
  lvgl_port_lock(-1);
  ui::consumeClockChanged();
  lvgl_port_unlock();

  gLastClockSaveMs = millis();
  Serial.println("Clock: send 'time HH:MM[:SS]' to set, 'time' to read");
}

void loop() {
  handleSerial();

  lvgl_port_lock(-1);
  const bool clockChanged = ui::consumeClockChanged();
  lvgl_port_unlock();
  if (clockChanged) {
    saveClock();
  }

  const uint32_t now = millis();
  if (now - gLastClockSaveMs >= CLOCK_SAVE_MS) {
    gLastClockSaveMs = now;
    saveClock();
  }
  delay(20);
}
