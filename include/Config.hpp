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
 * @file Config.hpp
 * @brief Configuration structs for SpeechApp, loaded from XML via from_xml().
 *
 * All sub-structs correspond to XML blocks in the SpeechApp config file.
 * Fields have safe defaults; any element can be omitted from the XML.
 */
#pragma once
#include <string>
#include <stdexcept>

namespace speech {

/// @brief AMQP broker connection and topic configuration.
struct AmqpConfig {
    std::string url          = "amqp://localhost:5672"; ///< Broker URL (amqp:// or amqps://).
    std::string username;                               ///< AMQP username (empty = anonymous).
    std::string password;                               ///< AMQP password.
    std::string demod_topic  = "rf.demod";             ///< Subscribe: DEMOD_RESULT messages from DemodApp.
    std::string speech_topic = "rf.speech";            ///< Publish: transcription results (optional).
};

/// @brief Voice Activity Detection configuration.
struct VadConfig {
    float energy_threshold_db = -35.0f; ///< RMS energy below this (dBFS) → skip Whisper inference.
    int   min_speech_ms       = 250;    ///< Clips shorter than this are treated as silence/noise.
};

/// @brief Whisper.cpp transcription engine configuration.
struct TranscriberConfig {
    std::string model_path = "/etc/sdr-speech/models/ggml-base.en.bin"; ///< Path to GGML model file.
    std::string language   = "";    ///< Language code (e.g. "en"). Empty = auto-detect.
    int         n_threads  = 4;    ///< CPU threads for Whisper inference.
    bool        translate  = false; ///< Translate non-English speech to English if true.
};

/// @brief Transcript storage and log rotation configuration.
struct TranscriptConfig {
    bool        enabled     = true;                         ///< Write transcripts to SQLite and daily log.
    std::string dir         = "/var/log/sdr-speech";        ///< Directory for daily text logs.
    std::string db_path     = "/var/log/sdr-speech/transcripts.db"; ///< SQLite database path.
    int         rotate_days = 30; ///< Delete log files older than this many days.
};

/**
 * @brief Top-level SpeechApp configuration.
 *
 * Loaded from XML via @c from_xml().  All fields have safe defaults.
 * Build the XML with @c \<amqp\>, @c \<vad\>, @c \<transcriber\>, @c \<transcript\> blocks.
 */
struct AppConfig {
    AmqpConfig        amqp;        ///< AMQP broker and topic settings.
    VadConfig         vad;         ///< Energy-based VAD gate.
    TranscriberConfig transcriber; ///< Whisper model and inference settings.
    TranscriptConfig  transcript;  ///< SQLite and log file output settings.

    /**
     * @brief Parse SpeechApp configuration from an XML file.
     * @param path Path to the XML config file.
     * @return Populated AppConfig with all fields.
     * @throws std::runtime_error if the file cannot be opened or is malformed.
     */
    static AppConfig from_xml(const std::string& path);
};

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
