# ── Stage 1: Builder ──────────────────────────────────────────────────────────
FROM python:3.12-slim AS builder

RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        gcc g++ libffi-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY requirements.txt .
# Install without torch (too large for builder layer — use runtime base)
RUN pip install --prefix=/install --no-cache-dir \
    python-qpid-proton \
    numpy \
    faster-whisper

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM python:3.12-slim AS runtime

RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Install torch + torchaudio separately (they have wheels)
RUN pip install --no-cache-dir torch torchaudio --index-url https://download.pytorch.org/whl/cpu

COPY --from=builder /install /usr/local

WORKDIR /app
COPY src/ ./src/
COPY config/ /etc/sdr-speech/

ENV PYTHONUNBUFFERED=1

ENTRYPOINT ["python3", "src/main.py"]
CMD ["/etc/sdr-speech/speech.xml"]
