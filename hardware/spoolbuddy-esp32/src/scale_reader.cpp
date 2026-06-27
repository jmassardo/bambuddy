#include "scale_reader.h"
#include "pins.h"
#include <HX711.h>

static HX711 hx711;

bool ScaleReader::begin() {
    hx711.begin(PIN_HX711_DOUT, PIN_HX711_SCK);

    // Wait up to 1s for HX711 to be ready
    uint32_t start = millis();
    while (!hx711.is_ready() && millis() - start < 1000) {
        delay(10);
    }

    if (hx711.is_ready()) {
        _ok = true;
        log_i("HX711 scale initialized (tare=%d, cal=%.6f)", _tareOffset, _calibrationFactor);
    } else {
        _ok = false;
        log_w("HX711 not responding");
    }

    return _ok;
}

void ScaleReader::close() {
    hx711.power_down();
    _ok = false;
}

ScaleReader::Reading ScaleReader::read() {
    Reading result = {0, false, 0, false};

    if (!_ok || !hx711.is_ready()) return result;

    int32_t raw = (int32_t)hx711.read();
    _lastRaw = raw;
    _ok = true;

    float grams = (raw - _tareOffset) * _calibrationFactor;
    float avgGrams = _movingAverage(grams);
    bool stable = _isStable(avgGrams);

    result.grams = roundf(avgGrams * 10.0f) / 10.0f;  // 1 decimal
    result.stable = stable;
    result.rawAdc = raw;
    result.valid = true;

    return result;
}

int32_t ScaleReader::tare() {
    if (_lastRaw != 0) {
        _tareOffset = _lastRaw;
        _sampleCount = 0;
        _sampleIndex = 0;
        _stabilityCount = 0;
        _stabilityIndex = 0;
        log_i("Tared at raw=%d", _tareOffset);
    }
    return _tareOffset;
}

void ScaleReader::updateCalibration(int32_t tareOffset, float calibrationFactor) {
    _tareOffset = tareOffset;
    _calibrationFactor = calibrationFactor;
    log_i("Calibration updated: tare=%d, factor=%.6f", tareOffset, calibrationFactor);
}

float ScaleReader::_movingAverage(float newSample) {
    _samples[_sampleIndex] = newSample;
    _sampleIndex = (_sampleIndex + 1) % AVG_SIZE;
    if (_sampleCount < AVG_SIZE) _sampleCount++;

    float sum = 0;
    for (size_t i = 0; i < _sampleCount; i++) {
        sum += _samples[i];
    }
    return sum / _sampleCount;
}

bool ScaleReader::_isStable(float currentGrams) {
    uint32_t now = millis();

    _stabilityHistory[_stabilityIndex] = {now, currentGrams};
    _stabilityIndex = (_stabilityIndex + 1) % STABILITY_SIZE;
    if (_stabilityCount < STABILITY_SIZE) _stabilityCount++;

    if (_stabilityCount < 5) return false;

    // Check if all readings within 1s window are within 2g
    uint32_t cutoff = now - 1000;
    float minVal = 1e9, maxVal = -1e9;
    size_t recentCount = 0;

    for (size_t i = 0; i < _stabilityCount; i++) {
        if (_stabilityHistory[i].timeMs >= cutoff) {
            float g = _stabilityHistory[i].grams;
            if (g < minVal) minVal = g;
            if (g > maxVal) maxVal = g;
            recentCount++;
        }
    }

    if (recentCount < 3) return false;
    return (maxVal - minVal) < 2.0f;
}
