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
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
#include <string>

namespace speech {
void run_listener(const AppConfig& cfg);
}

int main(int argc, char* argv[]) {
    auto log = spdlog::stdout_color_mt("speech");
    spdlog::set_default_logger(log);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

    std::string config_path = "/etc/sdr-speech/speech.xml";
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--debug") {
            spdlog::set_level(spdlog::level::debug);
        } else if (arg.size() > 2 && arg[0] != '-') {
            config_path = arg;
        }
    }

    spdlog::info("SpeechApp (C++) starting — config={}", config_path);

    try {
        auto cfg = speech::AppConfig::from_xml(config_path);
        spdlog::info("Broker: {}  Model: {}", cfg.amqp.url,
                     cfg.transcriber.model_path);
        speech::run_listener(cfg);
    } catch (const std::exception& e) {
        spdlog::critical("Fatal: {}", e.what());
        return 1;
    }
    return 0;
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
