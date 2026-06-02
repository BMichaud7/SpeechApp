"""Tests for src/transcriber.py — Whisper STT."""
import math
import numpy as np
import pytest
from src.transcriber import Transcriber, TranscriptionResult


@pytest.fixture(scope="module")
def stt():
    """Shared tiny-model instance — downloads once, reused across tests."""
    return Transcriber(model="tiny", device="cpu", compute_type="int8")


def _silence(duration_s=2.0, sr=16000) -> np.ndarray:
    return np.zeros(int(duration_s * sr), dtype=np.float32)


def _tone(freq=440.0, duration_s=2.0, sr=16000) -> np.ndarray:
    t = np.arange(int(duration_s * sr)) / sr
    return (0.5 * np.sin(2 * math.pi * freq * t)).astype(np.float32)


# ── Return type ───────────────────────────────────────────────────────────────

def test_returns_transcription_result(stt):
    result = stt.transcribe(_silence(), 16000)
    assert isinstance(result, TranscriptionResult)


def test_result_has_required_fields(stt):
    result = stt.transcribe(_silence(), 16000)
    assert isinstance(result.text, str)
    assert isinstance(result.language, str)
    assert isinstance(result.language_probability, float)
    assert isinstance(result.duration_s, float)
    assert isinstance(result.segments, list)


def test_language_probability_in_range(stt):
    result = stt.transcribe(_silence(), 16000)
    assert 0.0 <= result.language_probability <= 1.0


def test_duration_matches_input(stt):
    audio = _silence(3.0, sr=16000)
    result = stt.transcribe(audio, 16000)
    assert result.duration_s == pytest.approx(3.0, abs=0.1)


# ── Silence / tones → no meaningful text ─────────────────────────────────────

def test_silence_produces_short_or_empty_text(stt):
    result = stt.transcribe(_silence(2.0), 16000)
    # Silence may produce empty string or very short hallucination
    assert len(result.text) < 50, f"Unexpectedly long text from silence: {result.text!r}"


def test_pure_tone_not_mistaken_for_long_speech(stt):
    result = stt.transcribe(_tone(440.0, 2.0), 16000)
    # A 440Hz tone should produce little or no text
    assert len(result.text) < 80


# ── Resampling ────────────────────────────────────────────────────────────────

def test_48khz_input_resampled(stt):
    """Transcriber must resample 48kHz audio to 16kHz internally."""
    audio_48k = _silence(2.0, sr=48000)
    result = stt.transcribe(audio_48k, 48000)
    assert isinstance(result.text, str)
    assert result.duration_s == pytest.approx(2.0, abs=0.3)


def test_22khz_input_resampled(stt):
    audio_22k = _silence(1.0, sr=22050)
    result = stt.transcribe(audio_22k, 22050)
    assert isinstance(result.text, str)


# ── Segments ──────────────────────────────────────────────────────────────────

def test_segments_have_start_end_text(stt):
    result = stt.transcribe(_silence(2.0), 16000)
    for seg in result.segments:
        assert "start" in seg
        assert "end"   in seg
        assert "text"  in seg
        assert seg["start"] <= seg["end"]


def test_segment_times_within_duration(stt):
    audio = _silence(3.0, sr=16000)
    result = stt.transcribe(audio, 16000)
    for seg in result.segments:
        assert seg["start"] >= 0
        assert seg["end"]   <= result.duration_s + 1.0  # small tolerance
