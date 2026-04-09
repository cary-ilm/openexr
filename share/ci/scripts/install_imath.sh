#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

# This script is used by the ci_workflow.yml/ci_steps.yml CI to
# install Imath. This is a part of the process that validates that
# OpenEXR's cmake properly locates the Imath dependency, either
# finding this installation or fetching the library from github to
# build internally.

set -ex

TAG="$1"

# sudo is for installs under /usr/local on Unix. Git Bash on Windows
# (including windows-11-arm) has a sudo stub that is disabled on GitHub
# runners, so do not use it there.
uname_s="$(uname -s 2>/dev/null)"
if [[ "${uname_s}" == MINGW* ]] || [[ "${uname_s}" == MSYS* ]]; then
  SUDO=""
elif command -v sudo >/dev/null 2>&1; then
  SUDO=sudo
else
  SUDO=""
fi

CMAKE_WIN_ARCH=()
case "${uname_s}" in
  MINGW*|MSYS*)
    if [[ "${RUNNER_ARCH:-}" == "ARM64" ]] ||
       [[ "$(uname -m 2>/dev/null)" =~ ^(aarch64|arm64|ARM64)$ ]]; then
      CMAKE_WIN_ARCH=("-A" "ARM64")
    fi
    ;;
esac

git clone https://github.com/AcademySoftwareFoundation/Imath.git
cd Imath

git checkout ${TAG}

cmake -S . -B _build "${CMAKE_WIN_ARCH[@]}" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
$SUDO cmake --build _build \
      --target install \
      --config Release \
      --parallel 2

cd ..
rm -rf Imath
