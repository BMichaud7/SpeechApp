"""Voice Activity Detection using Silero-VAD."""
from __future__ import annotations
import numpy as np
import torch
import logging

log = logging.getLogger(__name__)

_MODEL_REPO  = "snakers4/silero-vad"
_MODEL_NAME  = "silero_vad"
_SAMPLE_RATE = 16000   # Silero-VAD native rate


class VoiceDetector:
    """
    Wraps Silero-VAD to detect whether a PCM audio block contains speech.

    The model is downloaded once (~1 MB) and cached by torch.hub.
    Input audio is resampled to 16 kHz internally.
    """

    def __init__(self, threshold: float = 0.5,
                 min_speech_ms: int = 250,
                 min_silence_ms: int = 100) -> None:
        self.threshold      = threshold
        self.min_speech_ms  = min_speech_ms
        self.min_silence_ms = min_silence_ms
        self._model, self._utils = torch.hub.load(
            _MODEL_REPO, _MODEL_NAME, trust_repo=True)
        self._model.eval()
        log.info("Silero-VAD loaded (threshold=%.2f)", threshold)

    def contains_speech(self, audio: np.ndarray, sample_rate: int) -> tuple[bool, float]:
        """
        Returns (speech_detected, max_probability).

        audio        : float32 numpy array, mono PCM (any sample rate)
        sample_rate  : original sample rate of the audio
        """
        if len(audio) == 0:
            return False, 0.0

        wav = torch.from_numpy(audio.astype(np.float32))

        # Resample to 16 kHz if needed
        if sample_rate != _SAMPLE_RATE:
            import torchaudio.functional as F
            wav = F.resample(wav, sample_rate, _SAMPLE_RATE)

        # Silero-VAD expects (1, T) or (T,)
        if wav.ndim == 1:
            wav = wav.unsqueeze(0)

        with torch.no_grad():
            probs = self._model.audio_forward(wav, sr=_SAMPLE_RATE)

        # probs: tensor of per-chunk probabilities
        max_prob = float(probs.max().item()) if len(probs) else 0.0
        duration_ms = len(audio) / sample_rate * 1000

        # Require minimum duration and confidence
        detected = (max_prob >= self.threshold and
                    duration_ms >= self.min_speech_ms)

        return detected, max_prob
