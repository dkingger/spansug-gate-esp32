#pragma once

class CurrentSensor {
public:
  explicit CurrentSensor(int pin);

  void begin();
  float readAmps() const;

private:
  int pin_;
};