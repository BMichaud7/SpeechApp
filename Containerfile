# ════════════════════════════════════════════════════════════════════════
#  SpeechApp — C++ RF speech detection and transcription
#
#  Build:
#    podman build -t sdr-speech:1.0.0 .
#
#  Run (model must be available at the mount path):
#    podman run -d --name sdr-speech --network=host \
#      -v /path/to/speech.xml:/etc/sdr-speech/speech.xml:ro \
#      -v /path/to/models:/etc/sdr-speech/models:ro \
#      -v /var/log/sdr-speech:/var/log/sdr-speech \
#      sdr-speech:1.0.0
# ════════════════════════════════════════════════════════════════════════

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        cmake ninja-build pkg-config git ca-certificates curl \
        libqpid-proton-cpp12-dev libqpid-proton-dev \
        libtinyxml2-dev libsqlite3-dev libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

RUN cmake -B build -S . -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DFETCHCONTENT_QUIET=OFF \
    && cmake --build build --parallel \
    && cmake --install build --prefix /install

# ── Runtime ───────────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        libqpid-proton-cpp12 libqpid-proton11 \
        libtinyxml2-10 libsqlite3-0 \
        tini \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/bin/sdr_speech /usr/local/bin/sdr_speech
RUN mkdir -p /etc/sdr-speech/models /var/log/sdr-speech
COPY config/speech.xml /etc/sdr-speech/speech.xml

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/sdr_speech"]
CMD ["/etc/sdr-speech/speech.xml"]
