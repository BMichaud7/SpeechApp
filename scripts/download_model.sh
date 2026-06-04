#!/bin/bash
# ========================================================================
# Project: OpenRFStack
# Author:  Brendan Michaud
# Year:    2026
# Part of OpenRFStack (https://github.com/OpenRFStack)
#
# Licensed under the Personal Use License.
# Do not use for commercial, organizational, or military purposes.
# ========================================================================

# Download a whisper.cpp GGML model.
# Usage: ./download_model.sh [base.en|small|medium|large]
set -euo pipefail

MODEL=${1:-base.en}
DEST=${2:-/etc/sdr-speech/models}
mkdir -p "$DEST"

BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
FILE="ggml-${MODEL}.bin"
URL="${BASE_URL}/${FILE}"

echo "Downloading ${FILE} → ${DEST}/${FILE}"
curl -L --progress-bar "$URL" -o "${DEST}/${FILE}"
echo "Done: $(du -h "${DEST}/${FILE}" | cut -f1)"

# ========================================================================
# End of file — OpenRFStack
# Subject to Personal Use License
# https://github.com/OpenRFStack
# ========================================================================
