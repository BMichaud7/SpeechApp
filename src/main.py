#!/usr/bin/env python3
"""
SpeechApp — passive RF speech detection and transcription.

Subscribes to the rf.demod AMQP topic published by DemodApp,
applies Silero-VAD to detect human speech in audio demod results,
and transcribes detected speech with faster-whisper (Whisper).

Does NOT issue any requests — purely a consumer.
"""
import argparse
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from src.config import AppConfig
from src.listener import DemodListener


def main() -> None:
    ap = argparse.ArgumentParser(description="RF speech detection and transcription")
    ap.add_argument("config", nargs="?",
                    default="/etc/sdr-speech/speech.xml",
                    help="Path to speech.xml config (default: /etc/sdr-speech/speech.xml)")
    ap.add_argument("--log-level", default="INFO",
                    choices=["DEBUG", "INFO", "WARNING", "ERROR"])
    args = ap.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    log = logging.getLogger("speech")

    try:
        cfg = AppConfig.from_xml(args.config)
    except FileNotFoundError:
        log.error("Config not found: %s", args.config)
        sys.exit(1)

    log.info("SpeechApp starting — broker=%s model=%s vad_threshold=%.2f",
             cfg.amqp.url, cfg.transcriber.model, cfg.vad.threshold)

    listener = DemodListener(cfg)
    listener.run()

    log.info("SpeechApp stopped.")


if __name__ == "__main__":
    main()
