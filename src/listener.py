"""
AMQP listener — subscribes to rf.demod, extracts audio, runs VAD + STT.
Passive consumer: never sends requests to DemodApp or any other service.
"""
from __future__ import annotations

import base64
import json
import logging
import struct
import threading
from typing import Callable

import numpy as np
import proton
import proton.handlers
import proton.reactor

from .config import AppConfig
from .vad import VoiceDetector
from .transcriber import Transcriber, TranscriptionResult

log = logging.getLogger(__name__)


def _decode_pcm_f32le(b64: str, num_samples: int) -> np.ndarray:
    """Decode base64 PCM float32-LE to numpy float32 array."""
    raw = base64.b64decode(b64)
    return np.frombuffer(raw, dtype="<f4")[:num_samples].astype(np.float32)


class _Handler(proton.handlers.MessagingHandler):
    def __init__(self, cfg: AppConfig,
                 on_result: Callable[[dict, TranscriptionResult], None]) -> None:
        super().__init__()
        self._cfg       = cfg
        self._on_result = on_result
        self._vad       = VoiceDetector(
            threshold     = cfg.vad.threshold,
            min_speech_ms = cfg.vad.min_speech_ms,
            min_silence_ms= cfg.vad.min_silence_ms,
        )
        self._stt       = Transcriber(
            model        = cfg.transcriber.model,
            language     = cfg.transcriber.language,
            device       = cfg.transcriber.device,
            compute_type = cfg.transcriber.compute_type,
            beam_size    = cfg.transcriber.beam_size,
        )

    def on_start(self, event: proton.Event) -> None:
        conn = event.container.connect(
            self._cfg.amqp.url,
            user=self._cfg.amqp.username or None,
            password=self._cfg.amqp.password or None,
            reconnect=proton.reactor.Backoff(),
        )
        event.container.create_receiver(conn, self._cfg.amqp.demod_topic)
        log.info("Listening on %s → %s",
                 self._cfg.amqp.url, self._cfg.amqp.demod_topic)

    def on_message(self, event: proton.Event) -> None:
        try:
            msg = json.loads(event.message.body)
        except Exception:
            return

        if msg.get("msg_type") != "DEMOD_RESULT":
            return
        if msg.get("demod_class") != "audio":
            return
        if "data_b64" not in msg or not msg["data_b64"]:
            return

        cf_mhz    = msg.get("center_freq_hz", 0) / 1e6
        modulation= msg.get("modulation", "?")
        sr        = int(msg.get("sample_rate_hz", 48000))
        n_samp    = int(msg.get("num_samples", 0))

        try:
            audio = _decode_pcm_f32le(msg["data_b64"], n_samp)
        except Exception as e:
            log.warning("Audio decode failed: %s", e)
            return

        duration_s = len(audio) / sr
        log.debug("Received %.1f MHz %s  %.1fs  %d samples",
                  cf_mhz, modulation, duration_s, len(audio))

        # ── Voice activity detection ──────────────────────────────────────────
        is_speech, prob = self._vad.contains_speech(audio, sr)
        if not is_speech:
            log.debug("  %.1f MHz — no speech (p=%.2f)", cf_mhz, prob)
            return

        log.info("  %.1f MHz %s — SPEECH detected (p=%.2f, %.1fs)",
                 cf_mhz, modulation, prob, duration_s)

        # ── Transcription ─────────────────────────────────────────────────────
        try:
            result = self._stt.transcribe(audio, sr)
        except Exception as e:
            log.error("Transcription failed: %s", e)
            return

        self._on_result(msg, result)


class DemodListener:
    """
    Passive AMQP listener that detects speech in demodulated audio and
    transcribes it using Whisper.

    Starts a background thread; call stop() to shut down cleanly.
    """

    def __init__(self, cfg: AppConfig) -> None:
        self._cfg = cfg

    def _on_result(self, msg: dict, result: TranscriptionResult) -> None:
        cf_mhz = msg.get("center_freq_hz", 0) / 1e6
        mod    = msg.get("modulation", "?")
        ts_ms  = msg.get("timestamp_ms", 0)

        log.info(
            "┌─ SPEECH  %.3f MHz  %s  (lang=%s p=%.2f)\n"
            "│  %s\n"
            "└─",
            cf_mhz, mod, result.language, result.language_probability,
            result.text or "(no speech detected)",
        )

        if not result.text:
            return

        # Detailed segment log at debug level
        for seg in result.segments:
            log.debug("  [%.1fs–%.1fs] %s", seg["start"], seg["end"], seg["text"])

        # Future: publish to rf.speech if enabled
        if self._cfg.transcriber.publish_enabled:
            log.debug("(publish to %s not yet wired)", self._cfg.amqp.speech_topic)

    def run(self) -> None:
        """Block until interrupted."""
        handler   = _Handler(self._cfg, self._on_result)
        container = proton.reactor.Container(handler)
        try:
            container.run()
        except KeyboardInterrupt:
            pass
