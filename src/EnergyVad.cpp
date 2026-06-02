#include "EnergyVad.hpp"
#include <cmath>
#include <algorithm>

namespace speech {

EnergyVad::EnergyVad(float threshold_db, int min_speech_ms)
    : threshold_db_(threshold_db), min_speech_ms_(min_speech_ms) {}

EnergyVad::Result EnergyVad::check(const float* pcm, int n_samples, int sample_rate) const {
    if (n_samples == 0) return {false, -100.0f};

    float duration_ms = n_samples * 1000.0f / sample_rate;
    if (duration_ms < min_speech_ms_) return {false, -100.0f};

    // RMS energy in dBFS
    double sum_sq = 0.0;
    for (int i = 0; i < n_samples; ++i)
        sum_sq += static_cast<double>(pcm[i]) * pcm[i];
    float energy_db = 10.0f * std::log10(sum_sq / n_samples + 1e-10f);

    return {energy_db >= threshold_db_, energy_db};
}

} // namespace speech
