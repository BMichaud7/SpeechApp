/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include "EnergyVad.hpp"
#include <cmath>
#include <vector>
#include <numeric>

using namespace speech;

static std::vector<float> silence(int n_samples) {
    return std::vector<float>(n_samples, 0.0f);
}

static std::vector<float> white_noise(int n_samples, float amp, unsigned seed = 42) {
    std::vector<float> v(n_samples);
    // LCG noise (no <random> dep for speed)
    uint32_t s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = amp * ((static_cast<float>(s >> 16) / 32768.0f) - 1.0f);
    }
    return v;
}

static std::vector<float> tone(float freq, float amp, int n, int sr) {
    std::vector<float> v(n);
    for (int i = 0; i < n; ++i)
        v[i] = amp * std::sin(2.0f * static_cast<float>(M_PI) * freq * i / sr);
    return v;
}

// ── Silence ───────────────────────────────────────────────────────────────────

TEST(EnergyVad, SilenceNotDetected) {
    EnergyVad vad(-35.0f, 250);
    auto s = silence(48000);
    auto [det, db] = vad.check(s.data(), (int)s.size(), 48000);
    EXPECT_FALSE(det);
    EXPECT_LT(db, -35.0f);
}

TEST(EnergyVad, EmptyInputNotDetected) {
    EnergyVad vad(-35.0f, 250);
    auto [det, db] = vad.check(nullptr, 0, 48000);
    EXPECT_FALSE(det);
}

// ── Short clips ───────────────────────────────────────────────────────────────

TEST(EnergyVad, TooShortNotDetected) {
    EnergyVad vad(-35.0f, 1000);   // require 1 second
    auto loud = tone(440.0f, 0.9f, 8000, 48000);  // only 0.17s
    auto [det, _] = vad.check(loud.data(), (int)loud.size(), 48000);
    EXPECT_FALSE(det);   // too short regardless of energy
}

// ── Energy detection ──────────────────────────────────────────────────────────

TEST(EnergyVad, LoudToneDetected) {
    EnergyVad vad(-35.0f, 250);
    auto t = tone(440.0f, 0.8f, 48000, 48000);   // 1s @ -2dBFS
    auto [det, db] = vad.check(t.data(), (int)t.size(), 48000);
    EXPECT_TRUE(det);
    EXPECT_GT(db, -35.0f);
}

TEST(EnergyVad, QuietNoiseBelowThreshold) {
    EnergyVad vad(-35.0f, 250);
    auto n = white_noise(48000, 0.001f);   // very quiet
    auto [det, db] = vad.check(n.data(), (int)n.size(), 48000);
    EXPECT_FALSE(det);
    EXPECT_LT(db, -35.0f);
}

TEST(EnergyVad, LoudNoiseAboveThreshold) {
    EnergyVad vad(-35.0f, 250);
    auto n = white_noise(48000, 0.3f);    // ~-10dBFS
    auto [det, db] = vad.check(n.data(), (int)n.size(), 48000);
    EXPECT_TRUE(det);
}

// ── Energy value accuracy ─────────────────────────────────────────────────────

TEST(EnergyVad, EnergyDbApproximatelyCorrect) {
    // A 0dBFS sine: energy_db ≈ -3dB (RMS of sine = amp/√2)
    EnergyVad vad(-99.0f, 0);   // threshold off
    auto t = tone(1000.0f, 1.0f, 48000, 48000);
    auto [_, db] = vad.check(t.data(), (int)t.size(), 48000);
    EXPECT_NEAR(db, -3.0f, 1.5f);
}

// ── Threshold edge ────────────────────────────────────────────────────────────

TEST(EnergyVad, CustomThresholdRespected) {
    auto sig = tone(440.0f, 0.1f, 48000, 48000);   // ~-20dBFS
    {
        EnergyVad strict(-10.0f, 250);
        auto [det, _] = strict.check(sig.data(), (int)sig.size(), 48000);
        EXPECT_FALSE(det);   // -20 < -10 → not detected
    }
    {
        EnergyVad loose(-30.0f, 250);
        auto [det, _] = loose.check(sig.data(), (int)sig.size(), 48000);
        EXPECT_TRUE(det);    // -20 > -30 → detected
    }
}
