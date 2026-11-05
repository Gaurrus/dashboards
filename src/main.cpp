#include <Arduino.h>

#include "config.h"
#include "hal/hal.h"
#include "logic/model.h"
#include "ui/dashboard.h"

namespace {

Hal hal;
DashboardModel model;
Dashboard dash;

uint32_t lastMs = 0;
uint32_t lastBeepMs = 0;
bool buzzerOn = false;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("VW T3 dashboard starting...");

  hal.begin();
  dash.begin();
  lastMs = millis();
}

void loop() {
  const uint32_t now = millis();
  const float dt = (now - lastMs) / 1000.0f;
  if (dt < 0.02f) {
    delay(5);
    return;
  }
  lastMs = now;

  RawInputs raw;
  hal.poll(raw);
  model.update(raw, dt);

  const Values& v = model.values();
  dash.render(v);

  const bool alarm = v.oilWarning || v.tempWarning;
  if (alarm) {
    if (now - lastBeepMs > 250) {
      lastBeepMs = now;
      buzzerOn = !buzzerOn;
      digitalWrite(cfg::PIN_BUZZER, buzzerOn ? HIGH : LOW);
    }
  } else if (buzzerOn) {
    buzzerOn = false;
    digitalWrite(cfg::PIN_BUZZER, LOW);
  }
}
