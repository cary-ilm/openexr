#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

# This script is used by the ci_workflow.yml/ci_steps.yml CI to
# install libdeflate. This is a part of the process that validates that
# OpenEXR's cmake properly locates the libdeflate dependency, either
# finding this installation or fetching the library from github to
# build internally.

set -ex

TAG="$1"

# The sudo is nececessary since the installation goes to /usr/local.
# Skip sudo on Windows (Git Bash/MSYS), which doesn't need it: some
# runner images (e.g. windows-11-arm) have Windows' own sudo, but
# disabled, so finding it doesn't mean it works.
if [ "${OS:-}" = "Windows_NT" ]; then
  SUDO=""
else
  SUDO=$(command -v sudo >/dev/null 2>&1 && echo sudo || echo "")
fi

# Resolve cmake's absolute path up front: sudo's secure_path can
# differ from the invoking user's PATH.
CMAKE=$(command -v cmake)

git clone https://github.com/ebiggers/libdeflate
cd libdeflate

git checkout ${TAG}

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
$SUDO "$CMAKE" --build . \
      --target install \
      --config Release \
      --parallel 2

cd ../..
rm -rf libdeflate
