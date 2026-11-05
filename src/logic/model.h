#pragma once

#include "dash_types.h"
#include "hal/hal.h"

class DashboardModel {
 public:
  void update(const RawInputs& raw, float dt);
  const Values& values() const { return v_; }

 private:
  Values v_{};

  bool speedSeeded_ = false;
  bool rpmSeeded_ = false;
  bool coolantSeeded_ = false;
  bool fuelSeeded_ = false;
  bool batterySeeded_ = false;
};
