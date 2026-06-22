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
 * @file WhisperTranscriber.hpp
 * @brief C++ wrapper around whisper.cpp for speech-to-text inference.
 *
 * Loads a GGML-quantised Whisper model at construction and provides a single
 * @c transcribe() method.  No Python, no PyTorch runtime — pure C++ GGML
 * inference using whisper.cpp.
 *
 * @par Model sizes (English-only .en variants)
 * - tiny.en:  75 MB  — 15× real-time on CPU, 50× on CUDA
 * - base.en:  145 MB — 7× real-time on CPU, 25× on CUDA  ← tested baseline
 * - small:    488 MB — 4× real-time on CPU, 15× on CUDA
 * - medium:   1.5 GB — 2× real-time on CPU, 8× on CUDA
 * - large-v3: 3.1 GB — 0.5× real-time on CPU, 3× on CUDA
 *
 * @see EnergyVad, TranscriptStore
 */
#pragma once
#include <string>
#include <vector>
#include <memory>

struct whisper_context; ///< Forward declaration — whisper.cpp internal type.

namespace speech {

/// @brief A single timed segment of transcribed text.
struct Segment {
    float       start_s; ///< Segment start time relative to audio start (seconds).
    float       end_s;   ///< Segment end time (seconds).
    std::string text;    ///< Transcribed text for this segment.
};

/// @brief Result of one Whisper inference call.
struct TranscriptionResult {
    std::string          text;       ///< Full concatenated transcript across all segments.
    std::string          language;   ///< Detected language code (e.g. "en", "fr").
    float                lang_prob;  ///< Language detection confidence 0–1.
    float                duration_s; ///< Audio duration that was transcribed (seconds).
    std::vector<Segment> segments;   ///< Individual timed segments (word-boundary aligned).
    bool                 ok = false; ///< False if the model is not loaded or inference failed.
};

/**
 * @class WhisperTranscriber
 * @brief Whisper speech-to-text engine wrapper.
 *
 * Non-copyable (owns the whisper_context).  Thread-safe for concurrent calls
 * to @c transcribe() after construction.
 */
class WhisperTranscriber {
public:
    /**
     * @brief Load a GGML Whisper model from disk.
     * @param model_path Path to the @c .bin GGML model file.
     * @param language   Force a specific language (e.g. "en"). Empty = auto-detect.
     * @param n_threads  CPU thread count for inference.
     * @param translate  If true, translate non-English speech to English.
     *
     * Does not throw if the model fails to load -- check loaded() instead.
     * A missing model is the expected state when WHISPER_MODEL is unset
     * (transcription is opt-in); transcribe() safely no-ops in that case.
     */
    explicit WhisperTranscriber(const std::string& model_path,
                                const std::string& language   = "",
                                int                n_threads  = 4,
                                bool               translate  = false);
    ~WhisperTranscriber();

    WhisperTranscriber(const WhisperTranscriber&)            = delete;
    WhisperTranscriber& operator=(const WhisperTranscriber&) = delete;

    /**
     * @brief Transcribe mono float32 PCM audio at 16 kHz.
     *
     * Input must already be at 16 kHz — use the Resampler class to convert
     * from any other sample rate first.  The EnergyVad gate should be applied
     * before calling this to avoid wasting inference time on silence.
     *
     * @param pcm16k   Pointer to mono float32 PCM samples at 16 kHz.
     * @param n_samples Number of samples (= duration_s × 16000).
     * @return TranscriptionResult — always check @c ok before using fields.
     */
    TranscriptionResult transcribe(const float* pcm16k, int n_samples) const;

    /// @brief True if the Whisper model was successfully loaded.
    bool loaded() const { return ctx_ != nullptr; }

private:
    whisper_context* ctx_       = nullptr; ///< Opaque whisper.cpp context.
    std::string      language_;            ///< Forced language (empty = auto).
    int              n_threads_;           ///< CPU thread count.
    bool             translate_;           ///< Translate to English.
};

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
