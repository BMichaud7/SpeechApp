"""Tests for src/vad.py — voice activity detection."""
import math
import numpy as np
import pytest
from src.vad import VoiceDetector

SR = 16000   # Silero-VAD native rate; we feed at 48kHz and let VoiceDetector resample


@pytest.fixture(scope="module")
def vad():
    return VoiceDetector(threshold=0.5, min_speech_ms=250)


def _silence(duration_s=1.0, sr=48000) -> np.ndarray:
    return np.zeros(int(duration_s * sr), dtype=np.float32)


def _white_noise(duration_s=1.0, sr=48000, amp=0.1) -> np.ndarray:
    rng = np.random.default_rng(0)
    return (rng.standard_normal(int(duration_s * sr)) * amp).astype(np.float32)


def _harmonic_buzz(duration_s=1.0, sr=48000, f0=130.0) -> np.ndarray:
    """Sawtooth-like harmonic stack — voice-like energy distribution."""
    n = int(duration_s * sr)
    t = np.arange(n) / sr
    sig = np.zeros(n, np.float32)
    for k in range(1, 20):
        sig += (1.0 / k) * np.sin(2 * math.pi * k * f0 * t).astype(np.float32)
    sig /= np.abs(sig).max()
    return sig


# ── Silence ───────────────────────────────────────────────────────────────────

def test_silence_not_detected(vad):
    detected, prob = vad.contains_speech(_silence(2.0), 48000)
    assert not detected
    assert prob < 0.5


def test_empty_audio_not_detected(vad):
    detected, prob = vad.contains_speech(np.array([], dtype=np.float32), 48000)
    assert not detected
    assert prob == pytest.approx(0.0)


# ── Noise ─────────────────────────────────────────────────────────────────────

def test_low_noise_not_detected(vad):
    # Very quiet broadband noise — should not trigger VAD
    noise = _white_noise(2.0, amp=0.01)
    detected, prob = vad.contains_speech(noise, 48000)
    assert not detected


# ── Voice-like signals ────────────────────────────────────────────────────────

def test_real_speech_detected(vad, real_speech_48k):
    """Real TTS speech (or modulated harmonic fallback) must trigger VAD."""
    detected, prob = vad.contains_speech(real_speech_48k, 48000)
    assert detected, f"Expected speech detected (p={prob:.3f})"
    assert prob > 0.5


def test_returns_probability_in_range(vad):
    audio = _harmonic_buzz(1.0)
    _, prob = vad.contains_speech(audio, 48000)
    assert 0.0 <= prob <= 1.0


# ── Resampling ────────────────────────────────────────────────────────────────

def test_different_sample_rates_accepted(vad):
    """VAD must resample 8kHz and 44.1kHz inputs without crashing."""
    for sr in (8000, 16000, 22050, 44100, 48000):
        n   = int(1.0 * sr)
        buzz = _harmonic_buzz(1.0, sr=sr, f0=130.0)
        detected, prob = vad.contains_speech(buzz, sr)
        assert isinstance(detected, bool)
        assert 0.0 <= prob <= 1.0


# ── Threshold ─────────────────────────────────────────────────────────────────

def test_high_threshold_suppresses_weak_speech(vad_high=None):
    vad_strict = VoiceDetector(threshold=0.99)
    silence    = _silence(1.0)
    detected, _ = vad_strict.contains_speech(silence, 48000)
    assert not detected


def test_low_threshold_detects_more(real_speech_48k):
    """Lower threshold means lower-confidence speech still fires."""
    vad_loose = VoiceDetector(threshold=0.1)
    detected, _ = vad_loose.contains_speech(real_speech_48k, 48000)
    assert detected
