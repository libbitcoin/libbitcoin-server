#!/bin/bash
# Download the named release files with their signatures and verify each.
# usage: fetch.sh <tag> <directory> <file>...
# environment: GH_TOKEN, PUBLIC_KEY, GITHUB_REPOSITORY
set -euo pipefail

TAG="$1"
DIRECTORY="$2"
shift 2

PATTERNS=()
for FILE in "$@"; do
    PATTERNS+=(--pattern "${FILE}" --pattern "${FILE}.asc")
done

gh release download "${TAG}" --repo "${GITHUB_REPOSITORY}" --dir "${DIRECTORY}" "${PATTERNS[@]}"

echo "${PUBLIC_KEY}" | gpg --batch --import
for FILE in "$@"; do
    gpg --batch --verify "${DIRECTORY}/${FILE}.asc" "${DIRECTORY}/${FILE}"
done
