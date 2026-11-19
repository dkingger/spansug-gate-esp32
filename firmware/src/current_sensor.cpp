#include "current_sensor.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"

CurrentSensor::CurrentSensor(int pin) : pin_(pin) {}

void CurrentSensor::begin() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(pin_, INPUT);
}

float CurrentSensor::readAmps() const {
  long sum = 0;
  int minimum = ADC_RESOLUTION - 1;
  int maximum = 0;

  for (int sample = 0; sample < SAMPLES; ++sample) {
    const int value = analogRead(pin_);
    sum += value;
    minimum = min(minimum, value);
    maximum = max(maximum, value);
  }

  if (maximum - minimum < 10) {
    return 0.0f;
  }

  const float average = static_cast<float>(sum) / SAMPLES;
  double sumSquared = 0.0;
  for (int sample = 0; sample < SAMPLES; ++sample) {
    const float centered = static_cast<float>(analogRead(pin_)) - average;
    sumSquared += centered * centered;
  }

  const float rmsAdc = sqrtf(static_cast<float>(sumSquared / SAMPLES));
  const float sensorVoltage = (rmsAdc / ADC_RESOLUTION) * VOLTAGE_REF;
  return sensorVoltage * VOLTAGE_COMPENSATION * CALIBRATION_FACTOR;
}