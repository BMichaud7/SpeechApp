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
 * @file TranscriptStore.hpp
 * @brief Dual-backend transcript persistence: SQLite database + rolling daily text log.
 *
 * Each transcribed utterance is written to both a SQLite3 table (for structured
 * querying) and an append-only plain-text log file (one file per calendar day,
 * auto-rotated after @c rotate_days).
 *
 * @par SQLite schema
 * @code{.sql}
 * CREATE TABLE transcripts (
 *   id         INTEGER PRIMARY KEY AUTOINCREMENT,
 *   ts_ms      INTEGER NOT NULL,
 *   ts_utc     TEXT,
 *   freq_hz    REAL,
 *   modulation TEXT,
 *   language   TEXT,
 *   lang_prob  REAL,
 *   energy_db  REAL,
 *   duration_s REAL,
 *   text       TEXT
 * );
 * @endcode
 *
 * @see WhisperTranscriber, EnergyVad
 */
#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct sqlite3; ///< Forward declaration — SQLite3 internal type.

namespace speech {

/// @brief One row from the @c transcripts SQLite table.
struct TranscriptRow {
    std::string ts_utc;     ///< ISO-8601 UTC timestamp (e.g. "2026-06-04 18:30:01").
    std::string freq_mhz;   ///< Centre frequency in MHz as a string.
    std::string modulation; ///< Modulation class from DemodApp (e.g. "FM_NB").
    std::string language;   ///< Detected language code (e.g. "en").
    float       lang_prob;  ///< Language detection confidence 0–1.
    float       energy_db;  ///< RMS energy of the audio in dBFS.
    float       duration_s; ///< Duration of the transcribed audio in seconds.
    std::string text;       ///< Transcribed text.
};

/**
 * @class TranscriptStore
 * @brief Writes transcriptions to SQLite and rolling daily text logs.
 *
 * Non-copyable (owns a SQLite3 connection).  All methods are safe to call
 * from a single writer thread; external synchronisation required for concurrent access.
 */
class TranscriptStore {
public:
    /**
     * @brief Open (or create) the SQLite database and log directory.
     * @param dir        Directory for daily @c .txt log files.
     * @param db_path    Path to the SQLite database file.
     * @param rotate_days Delete log files older than this many days.
     * @throws std::runtime_error if the database cannot be opened.
     */
    TranscriptStore(const std::string& dir, const std::string& db_path,
                    int rotate_days = 30);
    ~TranscriptStore();

    TranscriptStore(const TranscriptStore&) = delete;

    /**
     * @brief Persist one transcription result.
     * @param ts_ms      Unix timestamp in milliseconds.
     * @param freq_hz    Signal centre frequency (Hz).
     * @param modulation Modulation class (e.g. "FM_NB").
     * @param language   Detected language code.
     * @param lang_prob  Language confidence 0–1.
     * @param energy_db  Audio RMS energy (dBFS).
     * @param duration_s Audio clip duration (seconds).
     * @param text       Transcribed text.
     */
    void save(int64_t            ts_ms,
              double             freq_hz,
              const std::string& modulation,
              const std::string& language,
              float              lang_prob,
              float              energy_db,
              float              duration_s,
              const std::string& text);

    /**
     * @brief Return the most recent @p n transcriptions in reverse time order.
     * @param n Maximum number of rows to return.
     */
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

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
