"""Tests for src/config.py — XML parsing and defaults."""
import textwrap, tempfile, os, pytest
from src.config import AppConfig, AmqpConfig, VadConfig, TranscriberConfig, TranscriptConfig


def _write_xml(content: str) -> str:
    f = tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False)
    f.write(content); f.close()
    return f.name


# ── Defaults ──────────────────────────────────────────────────────────────────

def test_defaults():
    cfg = AppConfig()
    assert cfg.amqp.url          == "amqp://localhost:5672"
    assert cfg.amqp.demod_topic  == "rf.demod"
    assert cfg.vad.threshold     == 0.5
    assert cfg.vad.min_speech_ms == 250
    assert cfg.transcriber.model == "base"
    assert cfg.transcriber.language is None
    assert cfg.transcript.enabled    is True
    assert cfg.transcript.rotate_days == 30


# ── Full parse ────────────────────────────────────────────────────────────────

def test_full_parse():
    xml = textwrap.dedent("""\
    <?xml version="1.0"?>
    <speech_app version="1.0">
      <amqp>
        <url>amqp://broker:5672</url>
        <username>alice</username>
        <password>secret</password>
        <demod_topic>rf.demod.test</demod_topic>
        <speech_topic>rf.speech.test</speech_topic>
      </amqp>
      <vad>
        <threshold>0.7</threshold>
        <min_speech_ms>500</min_speech_ms>
        <min_silence_ms>200</min_silence_ms>
      </vad>
      <transcript>
        <enabled>true</enabled>
        <dir>/tmp/speech</dir>
        <db_path>/tmp/speech/t.db</db_path>
        <rotate_days>7</rotate_days>
      </transcript>
      <transcriber>
        <model>small</model>
        <language>en</language>
        <device>cpu</device>
        <compute_type>int8</compute_type>
        <beam_size>3</beam_size>
        <publish_enabled>true</publish_enabled>
      </transcriber>
    </speech_app>
    """)
    path = _write_xml(xml)
    try:
        cfg = AppConfig.from_xml(path)
    finally:
        os.unlink(path)

    assert cfg.amqp.url          == "amqp://broker:5672"
    assert cfg.amqp.username     == "alice"
    assert cfg.amqp.demod_topic  == "rf.demod.test"
    assert cfg.amqp.speech_topic == "rf.speech.test"

    assert cfg.vad.threshold      == pytest.approx(0.7)
    assert cfg.vad.min_speech_ms  == 500
    assert cfg.vad.min_silence_ms == 200

    assert cfg.transcript.enabled     is True
    assert cfg.transcript.dir         == "/tmp/speech"
    assert cfg.transcript.rotate_days == 7

    assert cfg.transcriber.model           == "small"
    assert cfg.transcriber.language        == "en"
    assert cfg.transcriber.device          == "cpu"
    assert cfg.transcriber.compute_type    == "int8"
    assert cfg.transcriber.beam_size       == 3
    assert cfg.transcriber.publish_enabled is True


def test_transcript_disabled():
    xml = textwrap.dedent("""\
    <?xml version="1.0"?>
    <speech_app>
      <transcript><enabled>false</enabled></transcript>
    </speech_app>
    """)
    path = _write_xml(xml)
    try:
        cfg = AppConfig.from_xml(path)
    finally:
        os.unlink(path)
    assert cfg.transcript.enabled is False


def test_missing_file_raises():
    with pytest.raises(FileNotFoundError):
        AppConfig.from_xml("/nonexistent/path.xml")


def test_partial_config_keeps_defaults():
    """Only amqp.url specified — everything else stays default."""
    xml = "<speech_app><amqp><url>amqp://x:1234</url></amqp></speech_app>"
    path = _write_xml(xml)
    try:
        cfg = AppConfig.from_xml(path)
    finally:
        os.unlink(path)
    assert cfg.amqp.url         == "amqp://x:1234"
    assert cfg.vad.threshold    == 0.5    # default preserved
    assert cfg.transcriber.model == "base" # default preserved
