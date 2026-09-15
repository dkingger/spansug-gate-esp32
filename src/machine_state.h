#pragma once

class MachineState {
public:
  void begin(float baselineCurrent);
  bool update(float currentAmps, unsigned long now);
  bool isOn() const;
  float baseline() const;
  void setBaseline(float baselineCurrent);

private:
  float baselineCurrent_ = 0.1f;
  bool isOn_ = false;
  bool pendingState_ = false;
  bool hasPendingState_ = false;
  unsigned long pendingSince_ = 0;
};