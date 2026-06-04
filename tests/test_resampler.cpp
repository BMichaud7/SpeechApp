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
#include "Resampler.hpp"
#include <cmath>

using namespace speech;

TEST(Resampler, SameRatePassthrough) {
    std::vector<float> in = {0.1f, 0.5f, -0.3f, 0.9f};
    auto out = resample(in.data(), (int)in.size(), 16000, 16000);
    ASSERT_EQ(out.size(), in.size());
    for (size_t i = 0; i < in.size(); ++i)
        EXPECT_FLOAT_EQ(out[i], in[i]);
}

TEST(Resampler, EmptyInput) {
    auto out = resample(nullptr, 0, 48000, 16000);
    EXPECT_TRUE(out.empty());
}

TEST(Resampler, OutputLengthDownsample) {
    // 48kHz → 16kHz: output should be ~1/3 the size
    int n_in = 48000;  // 1 second
    std::vector<float> in(n_in, 0.5f);
    auto out = resample(in.data(), n_in, 48000, 16000);
    EXPECT_NEAR((int)out.size(), 16000, 5);   // allow small rounding
}

TEST(Resampler, OutputLengthUpsample) {
    // 16kHz → 48kHz: output should be ~3× the size
    int n_in = 16000;
    std::vector<float> in(n_in, 0.5f);
    auto out = resample(in.data(), n_in, 16000, 48000);
    EXPECT_NEAR((int)out.size(), 48000, 5);
}

TEST(Resampler, DcSignalPreserved) {
    // A DC signal (constant value) should remain constant after resampling
    int n = 4800;
    std::vector<float> in(n, 0.7f);
    auto out = resample(in.data(), n, 48000, 16000);
    for (size_t i = 0; i < out.size() - 1; ++i)  // last sample may differ
        EXPECT_NEAR(out[i], 0.7f, 0.01f);
}

TEST(Resampler, ToneFrequencyPreserved) {
    // A 440Hz tone resampled 48k→16k should still be a 440Hz tone
    // (zero crossings at the right positions)
    int sr_in = 48000, sr_out = 16000;
    int n_in  = sr_in;  // 1 second
    std::vector<float> in(n_in);
    for (int i = 0; i < n_in; ++i)
        in[i] = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / sr_in);

    auto out = resample(in.data(), n_in, sr_in, sr_out);
    // Check zero crossing count ≈ 2 × 440 = 880 per second
    int crossings = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if ((out[i-1] < 0) != (out[i] < 0)) ++crossings;
    EXPECT_NEAR(crossings, 880, 20);
}
