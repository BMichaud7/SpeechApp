# SpeechApp — RF Speech Detection and Transcription

Subscribes to demodulated audio from `DemodApp` (`rf.demod` AMQP topic), detects human speech using Silero-VAD, and transcribes it with faster-whisper (Whisper).

**Passive consumer — does not issue any requests to DemodApp or other services.**

## Architecture

```
DemodApp ──► rf.demod (AMQP)
                  │
                  ▼
           DemodListener
           (demod_class=audio only)
                  │
                  ▼
          VoiceDetector (Silero-VAD)
          threshold=0.5, min_speech=250ms
                  │ speech detected
                  ▼
          Transcriber (faster-whisper)
          model=base, lang=auto
                  │
            stdout + spdlog
           (rf.speech reserved)
```

## Input Message Format

Consumes `DEMOD_RESULT` JSON from DemodApp:

```json
{
  "msg_type": "DEMOD_RESULT",
  "demod_class": "audio",
  "center_freq_hz": 100100000.0,
  "modulation": "FM_WB",
  "timestamp_ms": 1234567890,
  "sample_rate_hz": 48000,
  "channels": 1,
  "format": "pcm_f32le",
  "num_samples": 240000,
  "data_b64": "<base64 float32-LE PCM>"
}
```

Only `demod_class=audio` messages are processed. `bits` and `raw_iq` are ignored.

## Configuration

```xml
<speech_app version="1.0">
  <amqp>
    <url>amqp://localhost:5672</url>
    <username>sdr_ctrl</username>
    <password>sdr_hw_test</password>
    <demod_topic>rf.demod</demod_topic>
  </amqp>
  <vad>
    <threshold>0.5</threshold>        <!-- Silero-VAD probability threshold -->
    <min_speech_ms>250</min_speech_ms>
  </vad>
  <transcriber>
    <model>base</model>               <!-- tiny | base | small | medium | large -->
    <language></language>             <!-- empty = auto-detect -->
    <device>auto</device>             <!-- auto | cpu | cuda -->
    <compute_type>auto</compute_type>
    <beam_size>5</beam_size>
  </transcriber>
</speech_app>
```

## Model Sizes

| Whisper model | Parameters | VRAM | CPU speed | Accuracy |
|---|---|---|---|---|
| `tiny` | 39M | ~1 GB | ~10× real-time | baseline |
| `base` | 74M | ~1 GB | ~7× real-time | good |
| `small` | 244M | ~2 GB | ~4× real-time | better |
| `medium` | 769M | ~5 GB | ~2× real-time | best practical |
| `large` | 1.5B | ~10 GB | ~1× real-time | highest |

Default is `base` — good balance of speed and accuracy on CPU.

## Dependencies

| Library | Purpose |
|---|---|
| `python-qpid-proton` | AMQP 1.0 broker connection |
| `torch` / `torchaudio` | Silero-VAD inference + audio resampling |
| `faster-whisper` | CTranslate2-optimised Whisper STT |
| `numpy` | PCM array handling |

## Building

```bash
# Container (recommended — all deps included)
podman build -t sdr-speech:1.0.0 .

# Native
pip install -r requirements.txt
python src/main.py config/speech.xml --log-level DEBUG
```

## Running

```bash
podman run -d --name sdr-speech --network=host \
    -v /path/to/speech.xml:/etc/sdr-speech/speech.xml:ro \
    sdr-speech:1.0.0
```

Detected speech is logged at INFO level:
```
┌─ SPEECH  100.100 MHz  FM_WB  (lang=en p=0.98)
│  Hello, this is a test transmission.
└─
```

## Future: rf.speech Output

Set `<publish_enabled>true</publish_enabled>` when the AMQP publish path is wired. The schema will be:

```json
{
  "msg_type": "SPEECH_RESULT",
  "center_freq_hz": 100100000.0,
  "modulation": "FM_WB",
  "timestamp_ms": 1234567890,
  "language": "en",
  "language_probability": 0.98,
  "text": "Hello, this is a test transmission.",
  "segments": [
    {"start": 0.0, "end": 2.5, "text": "Hello, this is a test transmission."}
  ]
}
```
