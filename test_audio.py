#!/usr/bin/env python3
"""
test_audio.py — publish a fake DEMOD_RESULT with crappy simulated speech
to rf.demod for testing SpeechApp.

Generates a synthetic voice-like signal (harmonic tone burst with formants)
with heavy FM-demod noise added — the kind of degraded audio you'd hear
from a weak 2m voice transmission or a barely-in-range FM broadcast.

Usage:
    python test_audio.py [--text "what to say"] [--snr 5] [--broker amqp://...]
"""
import argparse
import base64
import json
import math
import struct
import sys
import time
import numpy as np
from scipy.signal import butter, lfilter, resample_poly

# ── Synthetic speech generator ────────────────────────────────────────────────

def _bandpass(data, lo, hi, fs, order=4):
    b, a = butter(order, [lo / (fs / 2), hi / (fs / 2)], btype="band")
    return lfilter(b, a, data)


def _lowpass(data, cutoff, fs, order=4):
    b, a = butter(order, cutoff / (fs / 2), btype="low")
    return lfilter(b, a, data)


def make_crappy_speech(text_hint: str = "", duration_s: float = 4.0,
                       sr: int = 48000, snr_db: float = 8.0,
                       rng: np.random.Generator | None = None) -> np.ndarray:
    """
    Synthesise a voice-like signal that Whisper can partially understand.

    Strategy:
      1. Generate a buzz (voiced source) at a realistic pitch (~120 Hz male,
         ~220 Hz female) with harmonics up to 4 kHz — this is the "glottal"
         source in the source-filter model of speech.
      2. Shape with three formant band-passes (F1≈700 Hz, F2≈1200 Hz,
         F3≈2500 Hz) — approximates a mid-vowel /a/-like sound.
      3. Apply amplitude envelope (speech rhythm, 4–6 syllables/s).
      4. Add FM-demod artefacts: impulse noise, heterodyne whistle, dropouts.
      5. Mix with white noise at the target SNR.
    """
    if rng is None:
        rng = np.random.default_rng(42)

    n = int(duration_s * sr)
    t = np.arange(n) / sr

    # ── Voiced source: sawtooth buzz at 130 Hz (male pitch) ──────────────────
    f0 = 130.0
    harmonics = np.zeros(n)
    for k in range(1, 30):                       # first 29 harmonics
        amp = 1.0 / k                            # 1/f spectral slope
        harmonics += amp * np.sin(2 * math.pi * k * f0 * t)
    harmonics /= np.abs(harmonics).max()

    # ── Formant shaping (vowel-like) ──────────────────────────────────────────
    voiced = (_bandpass(harmonics, 500,  900,  sr)  * 1.0   # F1
            + _bandpass(harmonics, 1000, 1500, sr)  * 0.8   # F2
            + _bandpass(harmonics, 2000, 3000, sr)  * 0.5)  # F3
    voiced = _lowpass(voiced, 3400, sr)          # telephone BW

    # ── Speech rhythm envelope: 5 syllables/s ────────────────────────────────
    syllable_rate = 5.0
    env = 0.5 + 0.5 * np.sin(2 * math.pi * syllable_rate * t)
    env = np.clip(env, 0, 1) ** 2               # shape envelope
    # Random silence gaps (inter-word pauses)
    pause_mask = np.ones(n)
    for _ in range(int(duration_s * 0.8)):       # ~80% duty cycle
        start = rng.integers(0, n - sr // 4)
        gap   = rng.integers(sr // 20, sr // 5)
        pause_mask[start:start + gap] = 0.0
    env *= pause_mask

    voiced *= env
    voiced /= np.abs(voiced).max() + 1e-9

    # ── FM-demod artefacts ────────────────────────────────────────────────────
    # 1. Impulse noise (squelch tail / multipath clicks)
    impulse = np.zeros(n)
    click_times = rng.integers(0, n, size=int(duration_s * 20))
    impulse[click_times] = rng.uniform(-3, 3, size=len(click_times))

    # 2. 50 Hz heterodyne hum (AC interference)
    hum = 0.05 * np.sin(2 * math.pi * 50 * t + rng.uniform(0, 2 * math.pi))

    # 3. Random dropout (signal fade)
    dropout = np.ones(n)
    for _ in range(3):
        start = rng.integers(0, n)
        drop_len = rng.integers(sr // 20, sr // 4)
        fade = np.linspace(1, 0, drop_len // 2)
        fade_up = np.linspace(0, 1, drop_len - len(fade))
        d = np.concatenate([fade, fade_up])
        end = min(start + len(d), n)
        dropout[start:end] *= d[:end - start]

    # ── Mix signal ────────────────────────────────────────────────────────────
    signal = (voiced + 0.15 * impulse + hum) * dropout

    # Add AWGN at target SNR
    sig_power = np.mean(signal ** 2)
    noise_power = sig_power / (10 ** (snr_db / 10))
    noise = rng.standard_normal(n) * math.sqrt(noise_power)
    mixed = signal + noise

    # Normalise to ±0.9 (avoid clipping)
    mixed /= np.abs(mixed).max() / 0.9
    return mixed.astype(np.float32)


# ── AMQP publish ──────────────────────────────────────────────────────────────

def publish(audio: np.ndarray, sr: int, broker: str,
            user: str, password: str, topic: str,
            modulation: str = "FM_NB", freq_hz: float = 146.52e6) -> None:
    sys.path.insert(0, "/usr/lib/python3/dist-packages")
    import proton
    import proton.reactor

    data_b64 = base64.b64encode(audio.tobytes()).decode()
    msg_body = json.dumps({
        "msg_type":       "DEMOD_RESULT",
        "schema_version": "1.0",
        "center_freq_hz": freq_hz,
        "modulation":     modulation,
        "timestamp_ms":   int(time.time() * 1000),
        "duration_ms":    int(len(audio) / sr * 1000),
        "demod_class":    "audio",
        "sample_rate_hz": sr,
        "channels":       1,
        "format":         "pcm_f32le",
        "num_samples":    len(audio),
        "data_b64":       data_b64,
    })

    class _Sender(proton.handlers.MessagingHandler):
        def __init__(self, body):
            super().__init__()
            self.body = body
            self.sent = False

        def on_start(self, event):
            conn = event.container.connect(
                broker,
                user=user or None,
                password=password or None,
            )
            event.container.create_sender(conn, topic)

        def on_sendable(self, event):
            if not self.sent:
                m = proton.Message(body=self.body)
                m.content_type = "application/json"
                event.sender.send(m)
                self.sent = True
                print(f"  Published {len(audio)/sr:.1f}s of audio to {topic}")
                event.sender.close()
                event.connection.close()

    proton.reactor.Container(_Sender(msg_body)).run()


# ── Local-only test (no AMQP) ────────────────────────────────────────────────

def test_local(audio: np.ndarray, sr: int) -> None:
    """Run VAD + transcription directly without AMQP."""
    sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
    from src.vad import VoiceDetector
    from src.transcriber import Transcriber

    print("\n── VAD ──")
    vad = VoiceDetector(threshold=0.3)   # lower threshold for noisy audio
    detected, prob = vad.contains_speech(audio, sr)
    print(f"  speech detected: {detected}  (p={prob:.3f})")

    if not detected:
        print("  [VAD: no speech — running transcriber anyway for demo]")

    print("\n── Transcription ──")
    stt = Transcriber(model="base", device="auto", compute_type="auto")
    result = stt.transcribe(audio, sr)
    print(f"  language : {result.language} (p={result.language_probability:.2f})")
    print(f"  duration : {result.duration_s:.1f}s")
    print(f"  text     : {result.text!r}")
    for seg in result.segments:
        print(f"    [{seg['start']:.1f}s–{seg['end']:.1f}s] {seg['text']}")


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snr",       type=float, default=8.0,
                    help="Signal-to-noise ratio in dB (lower = crappier)")
    ap.add_argument("--duration",  type=float, default=5.0,
                    help="Audio duration in seconds")
    ap.add_argument("--broker",    default="amqp://localhost:5672")
    ap.add_argument("--user",      default="sdr_ctrl")
    ap.add_argument("--password",  default="sdr_hw_test")
    ap.add_argument("--topic",     default="rf.demod")
    ap.add_argument("--modulation",default="FM_NB")
    ap.add_argument("--freq-mhz",  type=float, default=146.52)
    ap.add_argument("--local",     action="store_true",
                    help="Run VAD+STT locally without AMQP (default if broker unreachable)")
    ap.add_argument("--save",      metavar="FILE",
                    help="Save audio to .wav file for listening")
    args = ap.parse_args()

    print(f"Generating {args.duration:.0f}s of crappy voice-like audio at SNR={args.snr:.0f}dB...")
    sr = 48000
    audio = make_crappy_speech(duration_s=args.duration, sr=sr, snr_db=args.snr)
    print(f"  {len(audio)} samples @ {sr} Hz  ({len(audio)/sr:.1f}s)")

    if args.save:
        from scipy.io import wavfile
        wavfile.write(args.save, sr, audio)
        print(f"  Saved to {args.save}")

    if args.local:
        test_local(audio, sr)
    else:
        try:
            publish(audio, sr, args.broker, args.user, args.password,
                    args.topic, args.modulation, args.freq_mhz * 1e6)
        except Exception as e:
            print(f"  AMQP publish failed ({e}) — falling back to local test")
            test_local(audio, sr)


if __name__ == "__main__":
    main()
