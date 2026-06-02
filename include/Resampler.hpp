#pragma once
#include <vector>

namespace speech {

/// Resample mono float32 PCM from src_rate to dst_rate using linear interpolation.
std::vector<float> resample(const float* in, int n_in, int src_rate, int dst_rate);

} // namespace speech
