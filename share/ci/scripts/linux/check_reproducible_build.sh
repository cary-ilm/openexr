#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.
#
# Build and install OpenEXR in a fixed directory with all dependencies
# pinned, and check the result. There are three modes:
#
#   (default)         Build twice and verify that the two installed
#                     trees are bit-for-bit identical.
#   --manifest FILE   Build once and write the sha256 of every installed
#                     file to FILE. The release workflow publishes this.
#   --verify FILE     Build once and compare against a manifest, i.e.
#                     reproduce a published release.
#
# This is the same procedure an external party follows to reproduce
# a build; see "Reproducible Builds" in website/install.rst.
#
# Usage: check_reproducible_build.sh [--manifest FILE | --verify FILE] [source-dir]
#
# Environment:
#   BUILD_DIR   Build directory (default /tmp/openexr-build). It must be
#               the same for both builds, since the toolchain may record it.
#   OUT_DIR     Where the two installed trees are written
#               (default /tmp/openexr-repro)
#   IMATH_TAG   Imath tag or SHA (default is the one used by CI)
#   BUILD_SHARED_LIBS  ON (default) for shared libraries, OFF for static

set -euo pipefail

MODE=twice
MANIFEST=
if [[ "${1:-}" == "--manifest" || "${1:-}" == "--verify" ]]; then
    MODE=${1#--}
    MANIFEST=${2:?$1 requires a file}
    [[ "$MANIFEST" = /* ]] || MANIFEST=$PWD/$MANIFEST
    shift 2
fi
SRC=$(cd "${1:-.}" && pwd)
BUILD_DIR=${BUILD_DIR:-/tmp/openexr-build}
OUT_DIR=${OUT_DIR:-/tmp/openexr-repro}
BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS:-ON}
IMATH_TAG=${IMATH_TAG:-5f27ba266d3ea1565e912570c30b5eafc89959f1} # v3.2.3

# Fix timestamps embedded by tools to the date of the last commit.
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
    SOURCE_DATE_EPOCH=$(git -C "$SRC" log -1 --pretty=%ct)
fi
export SOURCE_DATE_EPOCH

if command -v sha256sum >/dev/null; then SUM="sha256sum"; else SUM="shasum -a 256"; fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# GNU ar records member timestamps and owners unless told otherwise,
# which makes static archives differ between builds (-D = deterministic).
AR_FLAGS=()
if [[ "$(uname -s)" == "Linux" ]]; then
    for lang in C CXX; do
        AR_FLAGS+=("-DCMAKE_${lang}_ARCHIVE_CREATE=<CMAKE_AR> qcD <TARGET> <LINK_FLAGS> <OBJECTS>"
                   "-DCMAKE_${lang}_ARCHIVE_APPEND=<CMAKE_AR> qD <TARGET> <LINK_FLAGS> <OBJECTS>"
                   "-DCMAKE_${lang}_ARCHIVE_FINISH=<CMAKE_RANLIB> -D <TARGET>")
    done
fi

build_once() {
    local dest=$1
    rm -rf "$BUILD_DIR"
    cmake -S "$SRC" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        ${AR_FLAGS[@]+"${AR_FLAGS[@]}"} \
        -DBUILD_SHARED_LIBS="$BUILD_SHARED_LIBS" \
        -DBUILD_TESTING=OFF \
        -DOPENEXR_BUILD_EXAMPLES=OFF \
        -DOPENEXR_FORCE_INTERNAL_IMATH=ON \
        -DOPENEXR_IMATH_TAG="$IMATH_TAG" \
        -DOPENEXR_FORCE_INTERNAL_DEFLATE=ON \
        -DOPENEXR_FORCE_INTERNAL_OPENJPH=ON \
        -DOPENEXR_FORCE_INTERNAL_ZSTD=ON
    cmake --build "$BUILD_DIR" --parallel
    DESTDIR="$dest" cmake --install "$BUILD_DIR"
}

# Print "checksum  path" for every installed file, in a stable order.
manifest_of() {
    (cd "$1" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 $SUM)
}

case "$MODE" in
manifest)
    build_once "$OUT_DIR/1"
    manifest_of "$OUT_DIR/1" > "$MANIFEST"
    echo "Wrote $MANIFEST ($(wc -l < "$MANIFEST") files)"
    ;;
verify)
    build_once "$OUT_DIR/1"
    manifest_of "$OUT_DIR/1" > "$OUT_DIR/1.sha256"
    if diff -u "$MANIFEST" "$OUT_DIR/1.sha256"; then
        echo "OK: this build matches $MANIFEST ($(wc -l < "$MANIFEST") files)"
    else
        echo "FAIL: this build differs from $MANIFEST" >&2
        exit 1
    fi
    ;;
twice)
    build_once "$OUT_DIR/1"
    build_once "$OUT_DIR/2"
    echo "Checksums of build 1:"
    manifest_of "$OUT_DIR/1" | tee "$OUT_DIR/1.sha256"
    manifest_of "$OUT_DIR/2" > "$OUT_DIR/2.sha256"
    if diff -u "$OUT_DIR/1.sha256" "$OUT_DIR/2.sha256"; then
        echo "OK: both builds are bit-for-bit identical ($(wc -l < "$OUT_DIR/1.sha256") files)"
    else
        echo "FAIL: the two builds differ" >&2
        exit 1
    fi
    ;;
esac
