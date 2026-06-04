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
/**
 * @file Resampler.hpp
 * @brief 
 */
#pragma once
#include <vector>

namespace speech {

/// Resample mono float32 PCM from src_rate to dst_rate using linear interpolation.
std::vector<float> resample(const float* in, int n_in, int src_rate, int dst_rate);

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
