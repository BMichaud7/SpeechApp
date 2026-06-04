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
#pragma once
#include <string>
#include <vector>
#include <memory>

struct whisper_context;

namespace speech {

struct Segment {
    float       start_s;
    float       end_s;
    std::string text;
};

struct TranscriptionResult {
    std::string          text;          // full concatenated transcript
    std::string          language;      // detected language code (e.g. "en")
    float                lang_prob;     // language detection confidence 0–1
    float                duration_s;
    std::vector<Segment> segments;
    bool                 ok = false;    // false if model not loaded / error
};

class WhisperTranscriber {
public:
    /// Load model from path.  Throws std::runtime_error if model not found.
    explicit WhisperTranscriber(const std::string& model_path,
                                const std::string& language = "",
                                int n_threads = 4,
                                bool translate = false);
    ~WhisperTranscriber();

    WhisperTranscriber(const WhisperTranscriber&) = delete;
    WhisperTranscriber& operator=(const WhisperTranscriber&) = delete;

    /// Transcribe mono float32 PCM at 16 kHz.
    TranscriptionResult transcribe(const float* pcm16k, int n_samples) const;

    bool loaded() const { return ctx_ != nullptr; }

private:
    whisper_context* ctx_ = nullptr;
    std::string      language_;
    int              n_threads_;
    bool             translate_;
};

} // namespace speech
