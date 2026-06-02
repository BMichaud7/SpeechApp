#pragma once
#include <string>
#include <stdexcept>

namespace speech {

struct AmqpConfig {
    std::string url          = "amqp://localhost:5672";
    std::string username;
    std::string password;
    std::string demod_topic  = "rf.demod";
    std::string speech_topic = "rf.speech";
};

struct VadConfig {
    float   energy_threshold_db = -35.0f; // below this → skip Whisper
    int     min_speech_ms       = 250;    // shorter clips are silence
};

struct TranscriberConfig {
    std::string model_path   = "/etc/sdr-speech/models/ggml-base.en.bin";
    std::string language     = "";        // empty = auto-detect
    int         n_threads    = 4;
    bool        translate    = false;     // translate to English
};

struct TranscriptConfig {
    bool        enabled      = true;
    std::string dir          = "/var/log/sdr-speech";
    std::string db_path      = "/var/log/sdr-speech/transcripts.db";
    int         rotate_days  = 30;
};

struct AppConfig {
    AmqpConfig       amqp;
    VadConfig        vad;
    TranscriberConfig transcriber;
    TranscriptConfig transcript;

    static AppConfig from_xml(const std::string& path);
};

} // namespace speech
