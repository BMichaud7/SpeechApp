"""
Persistent transcript storage: rolling daily text log + SQLite database.

Text log  — human-readable, one entry per detection, rotated daily:
    /var/log/sdr-speech/transcript_2026-06-02.txt

SQLite DB — queryable history, survives rotations:
    /var/log/sdr-speech/transcripts.db
    Table: transcripts (id, ts_ms, freq_hz, modulation, language, lang_prob, text)
"""
from __future__ import annotations

import os
import sqlite3
import logging
from datetime import datetime, timezone
from pathlib import Path

log = logging.getLogger(__name__)

_SCHEMA = """
CREATE TABLE IF NOT EXISTS transcripts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_ms       INTEGER NOT NULL,           -- UNIX timestamp ms (UTC)
    ts_utc      TEXT    NOT NULL,           -- ISO-8601 for readability
    freq_hz     REAL    NOT NULL,
    freq_mhz    TEXT    NOT NULL,           -- e.g. "146.520"
    modulation  TEXT    NOT NULL,
    language    TEXT,
    lang_prob   REAL,
    vad_prob    REAL,
    duration_s  REAL,
    text        TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_ts   ON transcripts(ts_ms);
CREATE INDEX IF NOT EXISTS idx_freq ON transcripts(freq_hz);
"""


class TranscriptStore:
    def __init__(self, dir_path: str, db_path: str, rotate_days: int = 30) -> None:
        self._dir         = Path(dir_path)
        self._db_path     = Path(db_path)
        self._rotate_days = rotate_days
        self._dir.mkdir(parents=True, exist_ok=True)
        self._db_path.parent.mkdir(parents=True, exist_ok=True)
        self._db = sqlite3.connect(str(self._db_path), check_same_thread=False)
        self._db.executescript(_SCHEMA)
        self._db.commit()
        log.info("TranscriptStore: db=%s  logs=%s", self._db_path, self._dir)

    def save(self, *,
             ts_ms: int,
             freq_hz: float,
             modulation: str,
             language: str | None,
             lang_prob: float,
             vad_prob: float,
             duration_s: float,
             text: str) -> None:
        if not text.strip():
            return

        ts_utc   = datetime.fromtimestamp(ts_ms / 1000, tz=timezone.utc)
        freq_mhz = f"{freq_hz / 1e6:.3f}"
        ts_str   = ts_utc.strftime("%Y-%m-%d %H:%M:%S UTC")
        date_str = ts_utc.strftime("%Y-%m-%d")

        # ── SQLite ────────────────────────────────────────────────────────────
        self._db.execute(
            """INSERT INTO transcripts
               (ts_ms, ts_utc, freq_hz, freq_mhz, modulation,
                language, lang_prob, vad_prob, duration_s, text)
               VALUES (?,?,?,?,?,?,?,?,?,?)""",
            (ts_ms, ts_str, freq_hz, freq_mhz, modulation,
             language, lang_prob, vad_prob, duration_s, text.strip()),
        )
        self._db.commit()

        # ── Daily text log ────────────────────────────────────────────────────
        log_file = self._dir / f"transcript_{date_str}.txt"
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(
                f"[{ts_str}] {freq_mhz} MHz  {modulation}"
                f"  lang={language or '?'} p={lang_prob:.2f}"
                f"  vad={vad_prob:.2f}  {duration_s:.1f}s\n"
                f"  {text.strip()}\n\n"
            )

        self._rotate_old_logs(date_str)
        log.debug("Saved transcript: %s MHz  %s", freq_mhz, text[:60])

    def _rotate_old_logs(self, today: str) -> None:
        """Delete text log files older than rotate_days."""
        from datetime import timedelta
        cutoff = datetime.strptime(today, "%Y-%m-%d") - timedelta(days=self._rotate_days)
        for f in self._dir.glob("transcript_*.txt"):
            try:
                fdate = datetime.strptime(f.stem.replace("transcript_", ""), "%Y-%m-%d")
                if fdate < cutoff:
                    f.unlink()
                    log.info("Rotated old log: %s", f)
            except ValueError:
                pass

    def tail(self, n: int = 20) -> list[dict]:
        """Return the n most recent transcriptions."""
        cur = self._db.execute(
            """SELECT ts_utc, freq_mhz, modulation, language, lang_prob, text
               FROM transcripts ORDER BY ts_ms DESC LIMIT ?""", (n,)
        )
        return [
            dict(ts=r[0], freq_mhz=r[1], modulation=r[2],
                 language=r[3], lang_prob=r[4], text=r[5])
            for r in cur.fetchall()
        ]

    def close(self) -> None:
        self._db.close()
