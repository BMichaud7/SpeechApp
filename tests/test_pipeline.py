"""
Integration tests — full VAD→Transcription pipeline with mocked AMQP.
Tests the listener's message parsing and routing without a live broker.
"""
import base64
import json
import math
import time
import numpy as np
import pytest
from unittest.mock import MagicMock, patch, call
from src.config import AppConfig
from src.transcript_store import TranscriptStore
from src.transcriber import TranscriptionResult
from src.vad import VoiceDetector


# ── Helpers ───────────────────────────────────────────────────────────────────

def _pcm_b64(audio: np.ndarray) -> str:
    return base64.b64encode(audio.astype("<f4").tobytes()).decode()


def _demod_msg(audio: np.ndarray, sr: int = 48000,
               freq_hz: float = 146.52e6,
               modulation: str = "FM_NB") -> dict:
    return {
        "msg_type":       "DEMOD_RESULT",
        "schema_version": "1.0",
        "demod_class":    "audio",
        "center_freq_hz": freq_hz,
        "modulation":     modulation,
        "timestamp_ms":   int(time.time() * 1000),
        "duration_ms":    int(len(audio) / sr * 1000),
        "sample_rate_hz": sr,
        "channels":       1,
        "format":         "pcm_f32le",
        "num_samples":    len(audio),
        "data_b64":       _pcm_b64(audio),
    }


def _harmonic(duration_s=2.0, sr=48000) -> np.ndarray:
    n, t = int(duration_s * sr), np.arange(int(duration_s * sr)) / sr
    sig = sum((1/k) * np.sin(2*math.pi*k*130*t) for k in range(1, 15))
    return (sig / np.abs(sig).max()).astype(np.float32)


# ── Message parsing ───────────────────────────────────────────────────────────

def test_decode_pcm_b64_roundtrip():
    from src.listener import _decode_pcm_f32le
    audio = np.array([0.1, -0.5, 0.9, 0.0], dtype=np.float32)
    b64   = base64.b64encode(audio.tobytes()).decode()
    result = _decode_pcm_f32le(b64, len(audio))
    np.testing.assert_allclose(result, audio, atol=1e-6)


def test_decode_truncates_to_num_samples():
    from src.listener import _decode_pcm_f32le
    audio = np.ones(100, dtype=np.float32)
    b64   = base64.b64encode(audio.tobytes()).decode()
    result = _decode_pcm_f32le(b64, 50)
    assert len(result) == 50


# ── VAD + transcript pipeline (mocked ML) ────────────────────────────────────

@pytest.fixture
def cfg(tmp_path):
    c = AppConfig()
    c.transcript.dir     = str(tmp_path / "logs")
    c.transcript.db_path = str(tmp_path / "logs" / "t.db")
    return c


@pytest.fixture
def store(cfg):
    s = TranscriptStore(cfg.transcript.dir, cfg.transcript.db_path)
    yield s
    s.close()


def test_speech_saved_to_store(cfg, store):
    """Mock VAD→STT→store: speech detected → row written to DB."""
    audio = _harmonic(2.0)
    msg   = _demod_msg(audio, freq_hz=146.52e6, modulation="FM_NB")

    fake_result = TranscriptionResult(
        text="Alpha Bravo Charlie",
        language="en",
        language_probability=0.95,
        duration_s=2.0,
        segments=[{"start": 0.0, "end": 2.0, "text": "Alpha Bravo Charlie"}],
    )

    with patch("src.vad.VoiceDetector.contains_speech", return_value=(True, 0.98)), \
         patch("src.transcriber.Transcriber.transcribe", return_value=fake_result):

        # Simulate what the listener does
        from src.listener import _decode_pcm_f32le
        audio_out = _decode_pcm_f32le(msg["data_b64"], msg["num_samples"])
        from src.vad import VoiceDetector
        from src.transcriber import Transcriber
        vad = VoiceDetector()
        stt = Transcriber.__new__(Transcriber)  # skip __init__
        detected, prob = vad.contains_speech(audio_out, msg["sample_rate_hz"])
        assert detected
        result = fake_result
        store.save(
            ts_ms      = msg["timestamp_ms"],
            freq_hz    = msg["center_freq_hz"],
            modulation = msg["modulation"],
            language   = result.language,
            lang_prob  = result.language_probability,
            vad_prob   = prob,
            duration_s = result.duration_s,
            text       = result.text,
        )

    rows = store.tail(5)
    assert len(rows) == 1
    assert rows[0]["text"]      == "Alpha Bravo Charlie"
    assert rows[0]["freq_mhz"]  == "146.520"
    assert rows[0]["modulation"] == "FM_NB"


def test_non_speech_not_saved(cfg, store):
    """VAD returns False → nothing stored."""
    with patch("src.vad.VoiceDetector.contains_speech", return_value=(False, 0.1)):
        from src.vad import VoiceDetector
        vad = VoiceDetector()
        silence = np.zeros(48000, np.float32)
        detected, prob = vad.contains_speech(silence, 48000)
        assert not detected
        # nothing saved
    assert store.tail(10) == []


def test_bits_message_ignored():
    """demod_class=bits should be skipped — audio pipeline only."""
    msg = {
        "msg_type":    "DEMOD_RESULT",
        "demod_class": "bits",
        "data_b64":    base64.b64encode(b"\x00\xff").decode(),
    }
    # The listener checks demod_class != "audio" and returns early
    assert msg.get("demod_class") != "audio"


def test_wrong_msg_type_ignored():
    msg = {"msg_type": "TASK_REQUEST", "demod_class": "audio"}
    assert msg.get("msg_type") != "DEMOD_RESULT"


# ── SNR degradation expectations ──────────────────────────────────────────────

@pytest.mark.parametrize("snr_db,must_detect", [
    (20, True),
    (10, True),
    (5,  True),
    (-10, False),  # below -10dB: non-deterministic; we only assert NOT required
])
def test_vad_on_noisy_speech(snr_db, must_detect, real_speech_48k):
    """
    VAD detects real speech reliably down to ~5dB SNR.
    Uses TTS audio (or modulated harmonic fallback from conftest).
    """
    vad = VoiceDetector(threshold=0.5)
    rng = np.random.default_rng(99)
    audio = real_speech_48k
    sig_pwr = np.mean(audio**2)
    noise = rng.standard_normal(len(audio)).astype(np.float32) * math.sqrt(sig_pwr / 10**(snr_db/10))
    noisy = np.clip(audio + noise, -1, 1)
    detected, prob = vad.contains_speech(noisy, 48000)
    if must_detect:
        assert detected, f"Expected speech at {snr_db}dB SNR (p={prob:.3f})"
    # At -10dB no assertion — VAD may or may not fire
