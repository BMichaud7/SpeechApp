#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct sqlite3;

namespace speech {

struct TranscriptRow {
    std::string ts_utc;
    std::string freq_mhz;
    std::string modulation;
    std::string language;
    float       lang_prob;
    float       energy_db;
    float       duration_s;
    std::string text;
};

class TranscriptStore {
public:
    TranscriptStore(const std::string& dir, const std::string& db_path,
                    int rotate_days = 30);
    ~TranscriptStore();

    TranscriptStore(const TranscriptStore&) = delete;

    void save(int64_t     ts_ms,
              double      freq_hz,
              const std::string& modulation,
              const std::string& language,
              float       lang_prob,
              float       energy_db,
              float       duration_s,
              const std::string& text);

    std::vector<TranscriptRow> tail(int n = 20) const;

private:
    std::string dir_;
    std::string db_path_;
    int         rotate_days_;
    sqlite3*    db_ = nullptr;

    void        init_db();
    void        rotate_old_logs(const std::string& today) const;
    std::string log_path(const std::string& date) const;
};

} // namespace speech
