#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

set -ex

IMATH_VERSION="$1"

if [[ $OSTYPE == "msys" ]]; then
    SUDO=""
else
    SUDO="sudo"
fi

git clone https://github.com/AcademySoftwareFoundation/Imath.git
cd Imath

if [ "$IMATH_VERSION" == "latest" ]; then
    LATEST_TAG=$(git describe --abbrev=0 --tags)
    git checkout tags/${LATEST_TAG} -b ${LATEST_TAG}
else
    git checkout tags/v${IMATH_VERSION} -b v${IMATH_VERSION}
fi

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
$SUDO cmake --build . \
      --target install \
      --config Release \
      --parallel 2

cd ../..
rm -rf Imath
