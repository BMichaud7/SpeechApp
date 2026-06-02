"""Tests for src/transcript_store.py — persistence without any ML."""
import time, sqlite3, pytest
from pathlib import Path
from src.transcript_store import TranscriptStore


@pytest.fixture
def store(tmp_path):
    s = TranscriptStore(
        dir_path    = str(tmp_path / "logs"),
        db_path     = str(tmp_path / "logs" / "t.db"),
        rotate_days = 30,
    )
    yield s
    s.close()


def _entry(store, **overrides):
    defaults = dict(
        ts_ms      = int(time.time() * 1000),
        freq_hz    = 146.52e6,
        modulation = "FM_NB",
        language   = "en",
        lang_prob  = 0.95,
        vad_prob   = 0.98,
        duration_s = 4.0,
        text       = "Alpha Bravo Charlie, over.",
    )
    defaults.update(overrides)
    store.save(**defaults)


# ── SQLite ────────────────────────────────────────────────────────────────────

def test_db_file_created(store, tmp_path):
    assert Path(tmp_path / "logs" / "t.db").exists()


def test_single_save_appears_in_tail(store):
    _entry(store, text="Hello world")
    rows = store.tail(10)
    assert len(rows) == 1
    assert rows[0]["text"] == "Hello world"
    assert rows[0]["freq_mhz"] == "146.520"
    assert rows[0]["modulation"] == "FM_NB"


def test_multiple_saves_ordered_newest_first(store):
    for i in range(5):
        _entry(store, ts_ms=1000 * (i + 1), text=f"msg {i}")
    rows = store.tail(10)
    assert rows[0]["text"] == "msg 4"   # newest first
    assert rows[-1]["text"] == "msg 0"


def test_tail_limit_respected(store):
    for i in range(20):
        _entry(store, text=f"entry {i}")
    assert len(store.tail(5)) == 5


def test_empty_text_not_saved(store):
    _entry(store, text="")
    _entry(store, text="   ")
    assert store.tail(10) == []


def test_db_has_correct_schema(store, tmp_path):
    db = sqlite3.connect(str(tmp_path / "logs" / "t.db"))
    cur = db.execute("PRAGMA table_info(transcripts)")
    cols = {r[1] for r in cur.fetchall()}
    db.close()
    for expected in ("ts_ms", "ts_utc", "freq_hz", "freq_mhz",
                     "modulation", "language", "lang_prob", "vad_prob",
                     "duration_s", "text"):
        assert expected in cols, f"missing column: {expected}"


def test_different_frequencies_queryable(store, tmp_path):
    _entry(store, freq_hz=146.52e6, text="two meters")
    _entry(store, freq_hz=162.025e6, text="weather radio")
    _entry(store, freq_hz=146.52e6, text="two meters again")
    db = sqlite3.connect(str(tmp_path / "logs" / "t.db"))
    rows = db.execute(
        "SELECT text FROM transcripts WHERE freq_hz BETWEEN 146e6 AND 147e6"
    ).fetchall()
    db.close()
    assert len(rows) == 2
    assert all("two meters" in r[0] for r in rows)


# ── Text log file ─────────────────────────────────────────────────────────────

def test_text_log_created(store, tmp_path):
    _entry(store, text="test entry")
    logs = list((tmp_path / "logs").glob("transcript_*.txt"))
    assert len(logs) == 1


def test_text_log_contains_entry(store, tmp_path):
    _entry(store, freq_hz=100.1e6, modulation="FM_WB", text="Broadcast content")
    log_content = next((tmp_path / "logs").glob("transcript_*.txt")).read_text()
    assert "100.100" in log_content
    assert "FM_WB"   in log_content
    assert "Broadcast content" in log_content


def test_text_log_appends(store, tmp_path):
    _entry(store, text="first")
    _entry(store, text="second")
    log_content = next((tmp_path / "logs").glob("transcript_*.txt")).read_text()
    assert "first"  in log_content
    assert "second" in log_content
