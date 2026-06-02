#include "Resampler.hpp"
#include <cmath>
#include <stdexcept>

namespace speech {

std::vector<float> resample(const float* in, int n_in, int src_rate, int dst_rate) {
    if (src_rate == dst_rate)
        return std::vector<float>(in, in + n_in);
    if (n_in == 0) return {};

    const double ratio   = static_cast<double>(dst_rate) / src_rate;
    const int    n_out   = static_cast<int>(std::ceil(n_in * ratio));
    std::vector<float> out(n_out);

    for (int i = 0; i < n_out; ++i) {
        double src_pos = i / ratio;
        int    lo      = static_cast<int>(src_pos);
        int    hi      = lo + 1;
        double frac    = src_pos - lo;

        float s_lo = (lo < n_in) ? in[lo] : 0.0f;
        float s_hi = (hi < n_in) ? in[hi] : 0.0f;
        out[i] = static_cast<float>(s_lo + frac * (s_hi - s_lo));
    }
    return out;
}

} // namespace speech
