/**
 * @file EnergyVad.hpp
 * @brief 
 */
#pragma once
#include <vector>

namespace speech {

/// Fast energy-based pre-filter VAD.
/// Returns true when the signal contains enough energy to be worth
/// running Whisper. Much cheaper than neural VAD — runs in microseconds.
class EnergyVad {
public:
    explicit EnergyVad(float threshold_db = -35.0f, int min_speech_ms = 250);

    struct Result {
        bool  detected;
        float energy_db;
    };

    Result check(const float* pcm, int n_samples, int sample_rate) const;

private:
    float threshold_db_;
    int   min_speech_ms_;
};

} // namespace speech
