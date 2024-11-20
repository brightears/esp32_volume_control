#include "sound_sensor.h"

SoundSensor::SoundSensor(int pin, int sampleWindow, float alpha)
    : _pin(pin)
    , _sampleWindow(sampleWindow)
    , _alpha(alpha)
    , _filteredSignal(0) {
    pinMode(_pin, INPUT);
}

float SoundSensor::getSoundLevel() {
    unsigned long startMillis = millis();
    unsigned int signalMax = 0;
    unsigned int signalMin = 4095;
    unsigned int sample;

    while (millis() - startMillis < _sampleWindow) {
        sample = analogRead(_pin);
        if (sample < 4095) {
            if (sample > signalMax) {
                signalMax = sample;
            } else if (sample < signalMin) {
                signalMin = sample;
            }
        }
    }

    unsigned int peakToPeak = signalMax - signalMin;
    _filteredSignal = _alpha * peakToPeak + (1 - _alpha) * _filteredSignal;
    float normalizedSignal = _filteredSignal / 4095.0; // Normalize to 0-1 range
    return pow(normalizedSignal, 0.5); // Apply square root to increase sensitivity to lower sounds
}

void SoundSensor::setSensitivity(float alpha) {
    if (alpha > 0 && alpha <= 1) {
        _alpha = alpha;
    }
}
