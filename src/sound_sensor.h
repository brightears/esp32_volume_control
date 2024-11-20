#ifndef SOUND_SENSOR_H
#define SOUND_SENSOR_H

#include <Arduino.h>
#include <cmath>

class SoundSensor {
public:
    SoundSensor(int pin, int sampleWindow = 50, float alpha = 0.2);
    float getSoundLevel();
    void setSensitivity(float alpha);

private:
    int _pin;
    int _sampleWindow;
    float _alpha;
    float _filteredSignal;
};

#endif // SOUND_SENSOR_H
