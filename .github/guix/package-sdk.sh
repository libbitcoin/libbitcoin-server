#!/bin/bash
###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
###############################################################################
# Package the runner's platform SDK for the Guix build.
# usage: package-sdk.sh <apple|windows> <output>
# output: sdk.tar.zst (one top directory) and sdk.txt (versions, tar sha256)
set -euo pipefail

KIND="$1"
OUTPUT="$2"
STAGE="$(mktemp -d)"
EPOCH=946684800
mkdir -p "${OUTPUT}"

sha256()
{
    if command -v sha256sum > /dev/null; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

archive()
{
    local TOP="$1"
    "${TAR}" --create \
        --sort=name \
        --mtime="@${EPOCH}" \
        --owner=0 --group=0 --numeric-owner \
        --mode="u+rw,go+r,go-w" \
        --format=posix \
        --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \
        --directory="${STAGE}" \
        --file="${STAGE}.tar" \
        "${TOP}"
    echo "tar: $(sha256 "${STAGE}.tar")" >> "${OUTPUT}/sdk.txt"
    zstd -q -T0 -12 "${STAGE}.tar" -o "${OUTPUT}/sdk.tar.zst"
    rm -rf "${STAGE}" "${STAGE}.tar"
}

case "${KIND}" in
    apple)
        command -v gtar > /dev/null || brew install gnu-tar
        TAR=gtar
        SDK="$(cd "$(xcrun --sdk macosx --show-sdk-path)" && pwd -P)"
        {
            echo "xcode: $(xcodebuild -version | tr '\n' ' ')"
            echo "sdk: $(xcrun --sdk macosx --show-sdk-version)"
        } > "${OUTPUT}/sdk.txt"
        cp -R "${SDK}" "${STAGE}/MacOSX.sdk"
        archive MacOSX.sdk
        ;;
    windows)
        TAR=tar
        VSWHERE="$(cygpath -u "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")"
        VS="$(cygpath -u "$("${VSWHERE}" -latest -products '*' -property installationPath | tr -d '\r')")"
        MSVC_VERSION="$(tr -d '\r\n' < "${VS}/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt")"
        MSVC="${VS}/VC/Tools/MSVC/${MSVC_VERSION}"
        KITS="$(cygpath -u "C:/Program Files (x86)/Windows Kits/10")"
        SDK_VERSION=""
        for VERSION in $(ls "${KITS}/Include" | grep '^10\.' | sort -rV); do
            if [[ -f "${KITS}/Include/${VERSION}/ucrt/stdio.h" && \
                  -f "${KITS}/Lib/${VERSION}/um/x64/kernel32.lib" && \
                  -f "${KITS}/Lib/${VERSION}/um/arm64/kernel32.lib" ]]; then
                SDK_VERSION="${VERSION}"
                break
            fi
        done
        if [[ -z "${SDK_VERSION}" ]]; then
            echo "No complete Windows SDK found." >&2
            exit 1
        fi
        {
            echo "msvc: ${MSVC_VERSION}"
            echo "sdk: ${SDK_VERSION}"
        } > "${OUTPUT}/sdk.txt"
        mkdir -p "${STAGE}/winsdk/msvc/lib" \
            "${STAGE}/winsdk/sdk/Include/${SDK_VERSION}" \
            "${STAGE}/winsdk/sdk/Lib/${SDK_VERSION}/ucrt" \
            "${STAGE}/winsdk/sdk/Lib/${SDK_VERSION}/um"
        cp -R "${MSVC}/include" "${STAGE}/winsdk/msvc/"
        for ARCH in x64 arm64; do
            cp -R "${MSVC}/lib/${ARCH}" "${STAGE}/winsdk/msvc/lib/"
            cp -R "${KITS}/Lib/${SDK_VERSION}/ucrt/${ARCH}" "${STAGE}/winsdk/sdk/Lib/${SDK_VERSION}/ucrt/"
            cp -R "${KITS}/Lib/${SDK_VERSION}/um/${ARCH}" "${STAGE}/winsdk/sdk/Lib/${SDK_VERSION}/um/"
        done
        for PART in ucrt um shared; do
            cp -R "${KITS}/Include/${SDK_VERSION}/${PART}" "${STAGE}/winsdk/sdk/Include/${SDK_VERSION}/"
        done
        archive winsdk
        ;;
    *)
        echo "Unsupported SDK '${KIND}'." >&2
        exit 1
        ;;
esac

cat "${OUTPUT}/sdk.txt"
