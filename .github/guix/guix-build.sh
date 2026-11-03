#!/bin/bash
###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
###############################################################################
# Build one release binary in the pinned Guix container.
# usage: guix-build.sh <source> <output>
# environment: HOST, BASENAME, PROJECT, PROGRAM, FLAGS, LINKFLAGS, OPTIONS, JOBS
# darwin also: SDK (directory of the extracted Apple SDK), DEPLOYMENT_TARGET
# windows also: SDK (directory holding msvc/ and sdk/ from Visual Studio)
set -euo pipefail

GUIX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="$(realpath "$1")"
OUTPUT="$(realpath -m "$2")"
mkdir -p "${OUTPUT}"

SHARE_SDK=()
case "${HOST}" in
    *-apple-darwin)    SHARE_SDK=( --expose="$(realpath "${SDK}")=/sdk" ) ;;
    *-pc-windows-msvc) SHARE_SDK=( --expose="$(realpath "${SDK}")=/winsdk" ) ;;
esac

export HOST
guix time-machine --channels="${GUIX_DIR}/channels.scm" -- \
    shell --container --pure --no-cwd --no-grafts \
        --load-path="${GUIX_DIR}" \
        --manifest="${GUIX_DIR}/manifest.scm" \
        --share="${SOURCE}=/distsrc" \
        --share="${OUTPUT}=/output" \
        --expose="${GUIX_DIR}=/guix" \
        "${SHARE_SDK[@]}" \
        -- env \
            HOST="${HOST}" \
            BASENAME="${BASENAME}" \
            PROJECT="${PROJECT:-libbitcoin-server}" \
            PROGRAM="${PROGRAM:-bs}" \
            FLAGS="${FLAGS:-}" \
            LINKFLAGS="${LINKFLAGS:-}" \
            OPTIONS="${OPTIONS:-}" \
            JOBS="${JOBS:-$(nproc)}" \
            DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-}" \
            bash /guix/build.sh
