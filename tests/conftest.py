"""
pytest configuration — adds system proton packages to path and
provides shared audio fixtures.
"""
import sys
import math
import numpy as np
import pytest

# python3-qpid-proton is installed via apt, not pip
sys.path.insert(0, "/usr/lib/python3/dist-packages")


# ── Shared audio fixtures ─────────────────────────────────────────────────────

@pytest.fixture(scope="session")
def real_speech_48k() -> np.ndarray:
    """
    Generate real TTS speech at 48kHz using gTTS + PyAV.
    Cached for the entire test session (downloads once).
    """
    try:
        import io, av
        from scipy.signal import resample_poly
        from gtts import gTTS
        from math import gcd
        tts = gTTS("Alpha Bravo Charlie, radio check, how copy, over.", lang="en")
        buf = io.BytesIO(); tts.write_to_fp(buf); buf.seek(0)
        container = av.open(buf, format="mp3")
        frames = [f.to_ndarray() for f in container.decode(audio=0)]
        pcm = np.concatenate(frames, axis=1).squeeze().astype(np.float32)
        orig_sr = 22050
        g = gcd(48000, orig_sr)
        audio = resample_poly(pcm, 48000 // g, orig_sr // g).astype(np.float32)
        audio /= np.abs(audio).max() + 1e-9
        return audio
    except Exception:
        # Fallback: voiced harmonic signal that Silero-VAD reliably detects
        # (envelope-modulated to mimic syllable rhythm)
        sr, duration = 48000, 3.0
        n = int(duration * sr)
        t = np.arange(n) / sr
        sig = sum((1/k) * np.sin(2*math.pi*k*130*t) for k in range(1, 20))
        # Amplitude-modulate at syllable rate (5 Hz)
        env = (0.5 + 0.5 * np.sin(2*math.pi*5*t)) ** 2
        sig = (sig * env).astype(np.float32)
        sig /= np.abs(sig).max()
        return sig
