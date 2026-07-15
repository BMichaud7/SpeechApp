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
#include "TranscriptStore.hpp"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <stdexcept>

namespace speech {
namespace fs = std::filesystem;

static constexpr const char* SCHEMA = R"sql(
CREATE TABLE IF NOT EXISTS transcripts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_ms       INTEGER NOT NULL,
    ts_utc      TEXT    NOT NULL,
    freq_hz     REAL    NOT NULL,
    freq_mhz    TEXT    NOT NULL,
    modulation  TEXT    NOT NULL,
    language    TEXT,
    lang_prob   REAL,
    energy_db   REAL,
    duration_s  REAL,
    text        TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_ts   ON transcripts(ts_ms);
CREATE INDEX IF NOT EXISTS idx_freq ON transcripts(freq_hz);
)sql";

TranscriptStore::TranscriptStore(const std::string& dir,
                                 const std::string& db_path,
                                 int rotate_days)
    : dir_(dir), db_path_(db_path), rotate_days_(rotate_days)
{
    fs::create_directories(dir_);
    auto db_parent = fs::path(db_path_).parent_path();
    if (!db_parent.empty())
        fs::create_directories(db_parent);
    init_db();
    spdlog::info("TranscriptStore: db={}", db_path_);
}

TranscriptStore::~TranscriptStore() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

void TranscriptStore::init_db() {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("TranscriptStore: cannot open db: " + db_path_);
    char* err = nullptr;
    if (sqlite3_exec(db_, SCHEMA, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg(err); sqlite3_free(err);
        throw std::runtime_error("TranscriptStore: schema error: " + msg);
    }
}

static std::string utc_str(int64_t ts_ms) {
    time_t sec = ts_ms / 1000;
    struct tm tm_val;
#ifdef _WIN32
    gmtime_s(&tm_val, &sec);
#else
    gmtime_r(&sec, &tm_val);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm_val);
    return buf;
}

static std::string utc_date(int64_t ts_ms) {
    time_t sec = ts_ms / 1000;
    struct tm tm_val;
#ifdef _WIN32
    gmtime_s(&tm_val, &sec);
#else
    gmtime_r(&sec, &tm_val);
#endif
    char buf[12];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
    return buf;
}

void TranscriptStore::save(int64_t ts_ms, double freq_hz,
                            const std::string& modulation,
                            const std::string& language,
                            float lang_prob, float energy_db,
                            float duration_s, const std::string& text) {
    if (text.empty()) return;

    auto ts_utc  = utc_str(ts_ms);
    auto date    = utc_date(ts_ms);
    char freq_mhz[16];
    snprintf(freq_mhz, sizeof(freq_mhz), "%.3f", freq_hz / 1e6);

    // SQLite insert
    const char* sql =
        "INSERT INTO transcripts "
        "(ts_ms,ts_utc,freq_hz,freq_mhz,modulation,language,lang_prob,energy_db,duration_s,text) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("TranscriptStore: prepare failed: {}", sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_int64(stmt, 1, ts_ms);
    sqlite3_bind_text (stmt, 2, ts_utc.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt,3, freq_hz);
    sqlite3_bind_text (stmt, 4, freq_mhz,          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 5, modulation.c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 6, language.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt,7, lang_prob);
    sqlite3_bind_double(stmt,8, energy_db);
    sqlite3_bind_double(stmt,9, duration_s);
    sqlite3_bind_text (stmt,10, text.c_str(),      -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE)
        spdlog::error("TranscriptStore: insert failed: {}", sqlite3_errmsg(db_));
    sqlite3_finalize(stmt);

    // Daily text log
    auto log_file = log_path(date);
    std::ofstream f(log_file, std::ios::app);
    f << "[" << ts_utc << "] " << freq_mhz << " MHz  " << modulation
      << "  lang=" << language << " p=" << std::fixed << std::setprecision(2) << lang_prob
      << "  " << std::setprecision(1) << duration_s << "s\n"
      << "  " << text << "\n\n";

    rotate_old_logs(date);
}

std::vector<TranscriptRow> TranscriptStore::tail(int n) const {
    std::vector<TranscriptRow> rows;
    const char* sql =
        "SELECT ts_utc,freq_mhz,modulation,language,lang_prob,energy_db,duration_s,text "
        "FROM transcripts ORDER BY ts_ms DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TranscriptRow r;
        auto col = [&](int i) -> std::string {
            auto* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            return v ? v : "";
        };
        r.ts_utc    = col(0);
        r.freq_mhz  = col(1);
        r.modulation= col(2);
        r.language  = col(3);
        r.lang_prob = static_cast<float>(sqlite3_column_double(stmt, 4));
        r.energy_db = static_cast<float>(sqlite3_column_double(stmt, 5));
        r.duration_s= static_cast<float>(sqlite3_column_double(stmt, 6));
        r.text      = col(7);
        rows.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return rows;
}

std::string TranscriptStore::log_path(const std::string& date) const {
    return dir_ + "/transcript_" + date + ".txt";
}

void TranscriptStore::rotate_old_logs(const std::string& today) const {
    time_t now = time(nullptr);
    time_t cutoff_t = now - static_cast<time_t>(rotate_days_) * 86400;
    struct tm tm_val{};
    gmtime_r(&cutoff_t, &tm_val);
    char cutoff_buf[12];
    strftime(cutoff_buf, sizeof(cutoff_buf), "%Y-%m-%d", &tm_val);
    std::string cutoff(cutoff_buf);

    for (auto& e : fs::directory_iterator(dir_)) {
        if (!e.is_regular_file()) continue;
        auto name = e.path().filename().string();
        if (name.rfind("transcript_", 0) != 0) continue;
        if (!name.ends_with(".txt")) continue;
        auto date_part = name.substr(11, 10);  // "YYYY-MM-DD"
        if (date_part >= today) continue;
        if (date_part >= cutoff) continue;
        fs::remove(e.path());
        spdlog::info("TranscriptStore: rotated {}", name);
    }
}

} // namespace speech

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
