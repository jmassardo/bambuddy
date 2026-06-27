#pragma once

#include <Arduino.h>

class ScaleReader {
public:
    bool begin();
    void close();

    struct Reading {
        float grams;
        bool stable;
        int32_t rawAdc;
        bool valid;
    };

    Reading read();
    int32_t tare();
    void updateCalibration(int32_t tareOffset, float calibrationFactor);

    bool ok() const { return _ok; }
    int32_t lastRaw() const { return _lastRaw; }

    void setTareOffset(int32_t offset) { _tareOffset = offset; }
    void setCalibrationFactor(float factor) { _calibrationFactor = factor; }

private:
    bool _ok = false;
    int32_t _tareOffset = 0;
    float _calibrationFactor = 1.0f;
    int32_t _lastRaw = 0;

    // Moving average
    static const size_t AVG_SIZE = 20;
    float _samples[AVG_SIZE] = {0};
    size_t _sampleCount = 0;
    size_t _sampleIndex = 0;

    // Stability detection
    struct TimedSample {
        uint32_t timeMs;
        float grams;
    };
    static const size_t STABILITY_SIZE = 20;
    TimedSample _stabilityHistory[STABILITY_SIZE] = {};
    size_t _stabilityCount = 0;
    size_t _stabilityIndex = 0;

    float _movingAverage(float newSample);
    bool _isStable(float currentGrams);
};
