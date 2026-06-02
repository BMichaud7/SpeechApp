#include "Config.hpp"
#include <tinyxml2.h>
#include <stdexcept>

using namespace tinyxml2;

namespace speech {

static const char* txt(XMLElement* e, const char* tag, const char* def = "") {
    if (!e) return def;
    auto* child = e->FirstChildElement(tag);
    if (!child || !child->GetText()) return def;
    return child->GetText();
}

static float flt(XMLElement* e, const char* tag, float def) {
    const char* v = txt(e, tag);
    return v && v[0] ? std::stof(v) : def;
}

static int nt(XMLElement* e, const char* tag, int def) {
    const char* v = txt(e, tag);
    return v && v[0] ? std::stoi(v) : def;
}

static bool bl(XMLElement* e, const char* tag, bool def) {
    const char* v = txt(e, tag);
    if (!v || !v[0]) return def;
    std::string s(v);
    return s == "true" || s == "1" || s == "yes";
}

AppConfig AppConfig::from_xml(const std::string& path) {
    XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != XML_SUCCESS)
        throw std::runtime_error("Cannot open config: " + path);

    auto* root = doc.RootElement();
    AppConfig cfg;

    if (auto* a = root->FirstChildElement("amqp")) {
        cfg.amqp.url          = txt(a, "url",          cfg.amqp.url.c_str());
        cfg.amqp.username     = txt(a, "username",     "");
        cfg.amqp.password     = txt(a, "password",     "");
        cfg.amqp.demod_topic  = txt(a, "demod_topic",  cfg.amqp.demod_topic.c_str());
        cfg.amqp.speech_topic = txt(a, "speech_topic", cfg.amqp.speech_topic.c_str());
    }

    if (auto* v = root->FirstChildElement("vad")) {
        cfg.vad.energy_threshold_db = flt(v, "energy_threshold_db", cfg.vad.energy_threshold_db);
        cfg.vad.min_speech_ms       = nt (v, "min_speech_ms",       cfg.vad.min_speech_ms);
    }

    if (auto* t = root->FirstChildElement("transcriber")) {
        cfg.transcriber.model_path = txt(t, "model_path", cfg.transcriber.model_path.c_str());
        cfg.transcriber.language   = txt(t, "language",   "");
        cfg.transcriber.n_threads  = nt (t, "n_threads",  cfg.transcriber.n_threads);
        cfg.transcriber.translate  = bl (t, "translate",  false);
    }

    if (auto* s = root->FirstChildElement("transcript")) {
        cfg.transcript.enabled     = bl (s, "enabled",     true);
        cfg.transcript.dir         = txt(s, "dir",         cfg.transcript.dir.c_str());
        cfg.transcript.db_path     = txt(s, "db_path",     cfg.transcript.db_path.c_str());
        cfg.transcript.rotate_days = nt (s, "rotate_days", cfg.transcript.rotate_days);
    }

    return cfg;
}

} // namespace speech
