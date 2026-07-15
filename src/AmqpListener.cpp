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
#include "Config.hpp"
#include "EnergyVad.hpp"
#include "Resampler.hpp"
#include "WhisperTranscriber.hpp"
#include "TranscriptStore.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/source_options.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "Base64.hpp"

#include <cstring>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>

using json = nlohmann::json;

namespace speech {

class DemodHandler : public proton::messaging_handler {
public:
    DemodHandler(const AppConfig& cfg,
                 EnergyVad& vad,
                 WhisperTranscriber& stt,
                 TranscriptStore* store)
        : cfg_(cfg), vad_(vad), stt_(stt), store_(store)
    {
        worker_ = std::thread([this]{ workerLoop(); });
    }

    ~DemodHandler() {
        {
            std::lock_guard<std::mutex> lk(work_mu_);
            stopping_ = true;
        }
        work_cv_.notify_one();
        if (worker_.joinable()) worker_.join();
    }

    void on_container_start(proton::container& c) override {
        proton::connection_options copts;
        if (!cfg_.amqp.username.empty()) {
            // Without explicitly allowing PLAIN (and the insecure-mechs flag
            // PLAIN requires without TLS), proton never actually negotiates
            // the username/password onto the wire -- Artemis then sees an
            // anonymous connection and rejects it with AMQ229031 "Unable to
            // validate user ... Username: null", even though .user()/.password()
            // were set. Confirmed live: this was masked for the entire
            // lifetime of this file by an unrelated earlier crash (the
            // WhisperTranscriber model-load throw) that always killed the
            // process before execution ever reached this connect() call.
            copts.sasl_allowed_mechs("PLAIN");
            copts.sasl_allow_insecure_mechs(true);
            copts.user(cfg_.amqp.username);
        }
        if (!cfg_.amqp.password.empty()) copts.password(cfg_.amqp.password);
        // Without this, a failed initial connection (Artemis not up yet) is
        // permanent -- speech transcription would silently never receive
        // demod audio again after one bad boot timing.
        proton::reconnect_options reconn_opts;
        reconn_opts.delay(proton::duration(2000));
        reconn_opts.max_delay(proton::duration(30000));
        reconn_opts.max_attempts(0);
        copts.reconnect(reconn_opts);
        c.connect(cfg_.amqp.url, copts);
    }

    void on_connection_open(proton::connection& conn) override {
        proton::receiver_options ropts;
        ropts.source(proton::source_options().address(cfg_.amqp.demod_topic));
        conn.open_receiver(cfg_.amqp.demod_topic, ropts);
        spdlog::info("Listening on {} → {}", cfg_.amqp.url, cfg_.amqp.demod_topic);
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        try {
            std::string body = m.body().get<std::string>();
            {
                std::lock_guard<std::mutex> lk(work_mu_);
                if (work_q_.size() < 8) {
                    work_q_.push(std::move(body));
                } else {
                    spdlog::warn("SpeechApp: transcription backlog full — dropping audio");
                }
            }
            work_cv_.notify_one();
        } catch (const std::exception& e) {
            spdlog::warn("Message receive error: {}", e.what());
        }
        d.accept();
    }

private:
    void workerLoop() {
        while (true) {
            std::string body;
            {
                std::unique_lock<std::mutex> lk(work_mu_);
                work_cv_.wait(lk, [this]{ return !work_q_.empty() || stopping_; });
                if (stopping_ && work_q_.empty()) break;
                body = std::move(work_q_.front());
                work_q_.pop();
            }
            try {
                handle(json::parse(body));
            } catch (const std::exception& e) {
                spdlog::warn("Message parse error: {}", e.what());
            }
        }
    }

    void handle(const json& j) {
        if (j.value("msg_type", "") != "DEMOD_RESULT") return;
        if (j.value("demod_class", "") != "audio")       return;
        if (!j.contains("data_b64") || j["data_b64"].is_null()) return;

        double  freq_hz    = j.value("center_freq_hz",  0.0);
        std::string mod    = j.value("modulation",      "?");
        int64_t ts_ms      = j.value("timestamp_ms",    (int64_t)0);
        int     sr         = j.value("sample_rate_hz",  48000);

        if (sr <= 0) {
            spdlog::warn("handle: invalid sample_rate_hz={} — dropping", sr);
            return;
        }

        // Decode base64 PCM float32-LE
        auto raw = base64::decode(j["data_b64"].get<std::string>());
        int n_floats = static_cast<int>(raw.size() / sizeof(float));
        if (n_floats == 0) return;

        std::vector<float> audio(n_floats);
        std::memcpy(audio.data(), raw.data(), n_floats * sizeof(float));

        float dur_s = static_cast<float>(n_floats) / sr;
        spdlog::debug("rx {:.1f} MHz {}  {:.1f}s  {} samples",
                      freq_hz / 1e6, mod, dur_s, n_floats);

        // ── Energy VAD (fast pre-filter) ──────────────────────────────────────
        auto [detected, energy_db] = vad_.check(audio.data(), n_floats, sr);
        if (!detected) {
            spdlog::debug("  {:.1f} MHz — VAD skip (energy={:.1f}dB)", freq_hz/1e6, energy_db);
            return;
        }
        spdlog::info("  {:.1f} MHz {} — energy={:.1f}dB, running Whisper...",
                     freq_hz/1e6, mod, energy_db);

        // ── Resample to 16kHz for Whisper ─────────────────────────────────────
        auto pcm16k = resample(audio.data(), n_floats, sr, 16000);

        // ── Transcription ─────────────────────────────────────────────────────
        auto result = stt_.transcribe(pcm16k.data(), static_cast<int>(pcm16k.size()));
        if (!result.ok || result.text.empty()) {
            spdlog::debug("  {:.1f} MHz — no speech transcribed", freq_hz/1e6);
            return;
        }

        spdlog::info("┌─ SPEECH  {:.3f} MHz  {}  (lang={} dur={:.1f}s)\n│  {}\n└─",
                     freq_hz/1e6, mod, result.language, result.duration_s, result.text);

        if (store_) {
            store_->save(ts_ms > 0 ? ts_ms :
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count(),
                         freq_hz, mod,
                         result.language, result.lang_prob, energy_db,
                         result.duration_s, result.text);
        }
    }

    const AppConfig&    cfg_;
    EnergyVad&          vad_;
    WhisperTranscriber& stt_;
    TranscriptStore*    store_;

    std::mutex               work_mu_;
    std::queue<std::string>  work_q_;
    std::condition_variable  work_cv_;
    std::thread              worker_;
    bool                     stopping_{false};
};

void run_listener(const AppConfig& cfg) {
    EnergyVad vad(cfg.vad.energy_threshold_db, cfg.vad.min_speech_ms);
    WhisperTranscriber stt(cfg.transcriber.model_path,
                           cfg.transcriber.language,
                           cfg.transcriber.n_threads,
                           cfg.transcriber.translate);
    if (!stt.loaded())
        spdlog::info("SpeechApp: transcription disabled — listener still running "
                     "(received demod audio will be ignored)");

    std::unique_ptr<TranscriptStore> store;
    if (cfg.transcript.enabled)
        store = std::make_unique<TranscriptStore>(
            cfg.transcript.dir, cfg.transcript.db_path, cfg.transcript.rotate_days);

    DemodHandler handler(cfg, vad, stt, store.get());
    proton::container(handler).run();
}

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
