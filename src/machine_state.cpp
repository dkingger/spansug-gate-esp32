#include "machine_state.h"

#include <Arduino.h>

#include "config.h"

void MachineState::begin(float baselineCurrent) {
  setBaseline(baselineCurrent);
}

bool MachineState::update(float currentAmps, unsigned long now) {
  const bool requestedState = isOn_
      ? currentAmps > baselineCurrent_ - STATE_HYSTERESIS
      : currentAmps >= baselineCurrent_ + STATE_HYSTERESIS;

  if (requestedState == isOn_) {
    hasPendingState_ = false;
    return false;
  }

  if (!hasPendingState_ || pendingState_ != requestedState) {
    pendingState_ = requestedState;
    pendingSince_ = now;
    hasPendingState_ = true;
    return false;
  }

  if (now - pendingSince_ >= STATE_DEBOUNCE_MS) {
    isOn_ = requestedState;
    hasPendingState_ = false;
    return true;
  }

  return false;
}

bool MachineState::isOn() const {
  return isOn_;
}

float MachineState::baseline() const {
  return baselineCurrent_;
}

void MachineState::setBaseline(float baselineCurrent) {
  baselineCurrent_ = constrain(baselineCurrent, 0.0f, MAX_BASELINE_CURRENT);
  hasPendingState_ = false;
}