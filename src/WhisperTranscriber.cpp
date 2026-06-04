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
#include "WhisperTranscriber.hpp"
#include <whisper.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace speech {

WhisperTranscriber::WhisperTranscriber(const std::string& model_path,
                                       const std::string& language,
                                       int n_threads,
                                       bool translate)
    : language_(language), n_threads_(n_threads), translate_(translate)
{
    whisper_context_params cparams = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_)
        throw std::runtime_error("WhisperTranscriber: failed to load model: " + model_path);
    spdlog::info("WhisperTranscriber: model loaded from {}", model_path);
}

WhisperTranscriber::~WhisperTranscriber() {
    if (ctx_) { whisper_free(ctx_); ctx_ = nullptr; }
}

TranscriptionResult WhisperTranscriber::transcribe(const float* pcm16k, int n_samples) const {
    TranscriptionResult res;
    res.duration_s = static_cast<float>(n_samples) / WHISPER_SAMPLE_RATE;

    if (!ctx_ || n_samples == 0) return res;

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads      = n_threads_;
    params.translate      = translate_;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.suppress_blank = true;

    if (!language_.empty()) {
        params.language       = language_.c_str();
        params.detect_language = false;
    } else {
        params.language       = nullptr;
        params.detect_language = true;
    }

    if (whisper_full(ctx_, params, pcm16k, n_samples) != 0) {
        spdlog::warn("WhisperTranscriber: inference failed");
        return res;
    }

    // Language detection result
    const int lang_id = whisper_full_lang_id(ctx_);
    res.language  = (lang_id >= 0) ? whisper_lang_str(lang_id) : "?";
    res.lang_prob = 0.9f;  // whisper_full doesn't expose lang prob directly

    // Collect segments
    std::ostringstream full;
    const int n_seg = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_seg; ++i) {
        const char* t = whisper_full_get_segment_text(ctx_, i);
        if (!t) continue;
        std::string seg_text(t);
        // Strip leading/trailing whitespace
        auto it = seg_text.find_first_not_of(" \t\n\r");
        if (it == std::string::npos) continue;
        seg_text = seg_text.substr(it);

        float t0 = whisper_full_get_segment_t0(ctx_, i) / 100.0f;
        float t1 = whisper_full_get_segment_t1(ctx_, i) / 100.0f;
        res.segments.push_back({t0, t1, seg_text});
        if (!full.str().empty()) full << ' ';
        full << seg_text;
    }
    res.text = full.str();
    res.ok   = true;
    return res;
}

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
