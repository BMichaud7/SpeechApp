# SpeechApp — RF Speech Detection and Transcription (C++)

Subscribes to demodulated audio from DemodApp (`rf.demod`), detects speech
using energy-based VAD, and transcribes with **whisper.cpp** — pure C++,
no Python, no PyTorch at runtime.

**Passive consumer — does not issue any requests.**

## Architecture

```
DemodApp ──► rf.demod (AMQP, qpid-proton-cpp)
                  │  demod_class=audio, pcm_f32le base64
                  ▼
           EnergyVad          fast RMS gate (<1ms), skips Whisper for silence
                  │ energy > threshold
                  ▼
           Resampler           any SR → 16kHz, linear interpolation
                  │
                  ▼
           WhisperTranscriber  whisper.cpp, GGML model, CPU + CUDA
                  │
            spdlog stdout + transcripts.db (SQLite) + transcript_YYYY-MM-DD.txt
```

## Dependencies

| Library | Install |
|---|---|
| `qpid-proton-cpp` | `apt install libqpid-proton-cpp12-dev` |
| `sqlite3` | `apt install libsqlite3-dev` |
| `tinyxml2` | `apt install libtinyxml2-dev` |
| `whisper.cpp` | FetchContent (built automatically) |
| `spdlog`, `nlohmann/json` | FetchContent (built automatically) |

## Model Setup

```bash
./scripts/download_model.sh base.en    # 145MB English-only, fast
./scripts/download_model.sh small      # 488MB multilingual
./scripts/download_model.sh medium     # 1.5GB highest accuracy
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

## GPU

```bash
cmake -B build -DWHISPER_CUDA=ON
```
Set `<n_threads>1</n_threads>` — GPU inference is single-threaded.
