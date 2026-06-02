"""Configuration loader for SpeechApp."""
from __future__ import annotations
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field


@dataclass
class AmqpConfig:
    url: str = "amqp://localhost:5672"
    username: str = ""
    password: str = ""
    demod_topic: str = "rf.demod"
    speech_topic: str = "rf.speech"   # publish target (reserved for future use)


@dataclass
class VadConfig:
    threshold: float = 0.5            # Silero-VAD speech probability threshold
    min_speech_ms: int = 250          # discard shorter segments
    min_silence_ms: int = 100         # merge gaps shorter than this


@dataclass
class TranscriberConfig:
    model: str = "base"               # tiny | base | small | medium | large
    language: str | None = None       # None = auto-detect
    device: str = "auto"              # auto | cpu | cuda
    compute_type: str = "auto"        # auto | int8 | float16 | float32
    beam_size: int = 5
    publish_enabled: bool = False     # set True to push to speech_topic


@dataclass
class AppConfig:
    amqp: AmqpConfig = field(default_factory=AmqpConfig)
    vad: VadConfig = field(default_factory=VadConfig)
    transcriber: TranscriberConfig = field(default_factory=TranscriberConfig)

    @staticmethod
    def from_xml(path: str) -> "AppConfig":
        cfg = AppConfig()
        tree = ET.parse(path)
        root = tree.getroot()

        if (a := root.find("amqp")) is not None:
            if (v := a.findtext("url")):       cfg.amqp.url = v
            if (v := a.findtext("username")):  cfg.amqp.username = v
            if (v := a.findtext("password")):  cfg.amqp.password = v
            if (v := a.findtext("demod_topic")):  cfg.amqp.demod_topic = v
            if (v := a.findtext("speech_topic")): cfg.amqp.speech_topic = v

        if (v := root.find("vad")) is not None:
            if (t := v.findtext("threshold")):       cfg.vad.threshold = float(t)
            if (t := v.findtext("min_speech_ms")):   cfg.vad.min_speech_ms = int(t)
            if (t := v.findtext("min_silence_ms")):  cfg.vad.min_silence_ms = int(t)

        if (t := root.find("transcriber")) is not None:
            if (v := t.findtext("model")):            cfg.transcriber.model = v
            if (v := t.findtext("language")):         cfg.transcriber.language = v or None
            if (v := t.findtext("device")):           cfg.transcriber.device = v
            if (v := t.findtext("compute_type")):     cfg.transcriber.compute_type = v
            if (v := t.findtext("beam_size")):        cfg.transcriber.beam_size = int(v)
            if (v := t.findtext("publish_enabled")):
                cfg.transcriber.publish_enabled = v.lower() in ("true", "1", "yes")

        return cfg
