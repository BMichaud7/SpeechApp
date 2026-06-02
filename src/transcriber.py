"""Speech-to-text using faster-whisper."""
from __future__ import annotations
import numpy as np
import logging
from dataclasses import dataclass

log = logging.getLogger(__name__)


@dataclass
class TranscriptionResult:
    text: str
    language: str
    language_probability: float
    duration_s: float
    segments: list[dict]      # [{start, end, text}]


class Transcriber:
    """
    Wraps faster-whisper for speech-to-text transcription.

    Models are cached in ~/.cache/huggingface/hub on first use.
    """

    def __init__(self, model: str = "base",
                 language: str | None = None,
                 device: str = "auto",
                 compute_type: str = "auto",
                 beam_size: int = 5) -> None:
        from faster_whisper import WhisperModel

        # Resolve auto device/compute_type
        import torch
        if device == "auto":
            device = "cuda" if torch.cuda.is_available() else "cpu"
        if compute_type == "auto":
            compute_type = "float16" if device == "cuda" else "int8"

        log.info("Loading faster-whisper model=%s device=%s compute=%s",
                 model, device, compute_type)
        self._model      = WhisperModel(model, device=device,
                                        compute_type=compute_type)
        self._language   = language
        self._beam_size  = beam_size
        log.info("Transcriber ready")

    def transcribe(self, audio: np.ndarray,
                   sample_rate: int) -> TranscriptionResult:
        """
        Transcribe a mono float32 PCM audio block.

        faster-whisper expects 16 kHz float32 mono. We resample if needed.
        """
        import torchaudio.functional as F
        import torch

        if sample_rate != 16000:
            wav = torch.from_numpy(audio.astype(np.float32)).unsqueeze(0)
            wav = F.resample(wav, sample_rate, 16000)
            audio = wav.squeeze(0).numpy()

        segments_iter, info = self._model.transcribe(
            audio,
            language=self._language,
            beam_size=self._beam_size,
            vad_filter=False,   # we run our own VAD upstream
            word_timestamps=False,
        )

        segs = []
        full_text = []
        for seg in segments_iter:
            segs.append({"start": seg.start, "end": seg.end, "text": seg.text.strip()})
            full_text.append(seg.text.strip())

        return TranscriptionResult(
            text=" ".join(full_text),
            language=info.language,
            language_probability=info.language_probability,
            duration_s=len(audio) / 16000,
            segments=segs,
        )
