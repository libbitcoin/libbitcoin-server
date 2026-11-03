#!/bin/bash
###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
###############################################################################
# Build one release binary in the pinned Guix container.
# usage: guix-build.sh <source> <output>
# environment: HOST, BASENAME, PROJECT, PROGRAM, FLAGS, LINKFLAGS, OPTIONS, JOBS
set -euo pipefail

GUIX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="$(realpath "$1")"
OUTPUT="$(realpath -m "$2")"
mkdir -p "${OUTPUT}"

export HOST
guix time-machine --channels="${GUIX_DIR}/channels.scm" -- \
    shell --container --pure --no-cwd --no-grafts \
        --load-path="${GUIX_DIR}" \
        --manifest="${GUIX_DIR}/manifest.scm" \
        --share="${SOURCE}=/distsrc" \
        --share="${OUTPUT}=/output" \
        --expose="${GUIX_DIR}=/guix" \
        -- env \
            HOST="${HOST}" \
            BASENAME="${BASENAME}" \
            PROJECT="${PROJECT:-libbitcoin-server}" \
            PROGRAM="${PROGRAM:-bs}" \
            FLAGS="${FLAGS:-}" \
            LINKFLAGS="${LINKFLAGS:-}" \
            OPTIONS="${OPTIONS:-}" \
            JOBS="${JOBS:-$(nproc)}" \
            bash /guix/build.sh
