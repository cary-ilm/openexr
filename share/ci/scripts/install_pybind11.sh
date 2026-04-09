#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

set -ex

PYBIND11_VERSION="$1"

# See install_imath.sh: avoid broken Git Bash sudo on Windows runners.
uname_s="$(uname -s 2>/dev/null)"
if [[ "${uname_s}" == MINGW* ]] || [[ "${uname_s}" == MSYS* ]]; then
  SUDO=""
elif command -v sudo >/dev/null 2>&1; then
  SUDO=sudo
else
  SUDO=""
fi

git clone https://github.com/pybind/pybind11.git
cd pybind11

if [ "$PYBIND11_VERSION" == "latest" ]; then
    LATEST_TAG=$(git describe --abbrev=0 --tags)
    git checkout tags/${LATEST_TAG} -b ${LATEST_TAG}
else
    git checkout tags/v${PYBIND11_VERSION} -b v${PYBIND11_VERSION}
fi

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DPYBIND11_INSTALL=ON \
      -DPYBIND11_TEST=OFF \
      ../.
$SUDO cmake --build . \
      --target install \
      --config Release \
      --parallel 2

cd ../..
rm -rf pybind11
