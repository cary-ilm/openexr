#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

# Validate OPENEXR_FORCE_EMBEDDED_CORE (see "Linking Multiple OpenEXR
# Versions in the Executable" in website/install.rst):
#
# 1. Build and install a custom OpenEXR from this source tree,
#    configured as a plugin would need it: embedded core, custom
#    internal namespaces, and a custom library suffix.
# 2. Check its installed libraries: no OpenEXRCore shared library, no
#    exported OpenEXRCore (exr_*) symbols, and no symbols in the
#    default namespaces.
# 3. Build a plugin against the custom installation and an application
#    against the standard installation, and run the application, which
#    loads the plugin and uses both libraries in the same process.
#
# This is Linux-only: the symbol clashes it guards against are a
# consequence of the ELF global symbol namespace.
#
# Usage: test_embedded_core.sh <source-dir> <standard-install-dir> <work-dir> [extra cmake args...]
#
# The standard installation must be a shared-library build of the
# same source. Uses $CMAKE if set, otherwise cmake.

set -ex

SRC=$(cd "$1" && pwd)
STANDARD_INSTALL=$(cd "$2" && pwd)
WORK=$3
shift 3

CMAKE=${CMAKE:-cmake}
BUILD_TYPE=${BUILD_TYPE:-Release}

mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
CUSTOM_INSTALL=$WORK/install

# 1. The custom build: just the libraries.
"$CMAKE" -S "$SRC" -B "$WORK/build" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_INSTALL_PREFIX="$CUSTOM_INSTALL" \
      -DBUILD_SHARED_LIBS=ON \
      -DOPENEXR_FORCE_EMBEDDED_CORE=ON \
      -DOPENEXR_INTERNAL_IMF_NAMESPACE=Imf_embedded \
      -DIEX_INTERNAL_NAMESPACE=Iex_embedded \
      -DILMTHREAD_INTERNAL_NAMESPACE=IlmThread_embedded \
      -DOPENEXR_NAMESPACE_CUSTOM=1 \
      -DIEX_NAMESPACE_CUSTOM=1 \
      -DILMTHREAD_NAMESPACE_CUSTOM=1 \
      -DOPENEXR_LIB_SUFFIX=_embedded \
      -DOPENEXR_BUILD_TOOLS=OFF \
      -DOPENEXR_BUILD_EXAMPLES=OFF \
      -DOPENEXR_INSTALL_PKG_CONFIG=OFF \
      -DOPENEXR_INSTALL_DOCS=OFF \
      -DBUILD_TESTING=OFF \
      "$@"
"$CMAKE" --build "$WORK/build" --target install --config "$BUILD_TYPE" --parallel "$(nproc)"

# 2. Check the custom installation's libraries.
set +x
status=0
LIBS=$(find "$CUSTOM_INSTALL" -name 'lib*.so*' -type f)
if [ -z "$LIBS" ]; then
    echo "error: no shared libraries installed in $CUSTOM_INSTALL"
    exit 1
fi
if find "$CUSTOM_INSTALL" -name 'libOpenEXRCore*.so*' | grep -q .; then
    echo "error: an OpenEXRCore shared library was installed"
    status=1
fi
for lib in $LIBS; do
    echo "checking $lib"
    exr_syms=$(nm -D --defined-only "$lib" | awk '{print $NF}' | grep '^exr_' || true)
    if [ -n "$exr_syms" ]; then
        echo "error: $lib exports OpenEXRCore symbols:"
        echo "$exr_syms" | head -10
        status=1
    fi
    default_ns=$(nm -D --defined-only -C "$lib" | grep -E '(Imf|Iex|IlmThread)_[0-9]+_[0-9]+::' || true)
    if [ -n "$default_ns" ]; then
        echo "error: $lib exports symbols in a default namespace:"
        echo "$default_ns" | head -10
        status=1
    fi
done
if [ $status -ne 0 ]; then
    exit $status
fi
set -x

# 3. Build the plugin against the custom installation and the
# application against the standard installation, then run.
"$CMAKE" -S "$SRC/src/test/embedded_core/plugin" -B "$WORK/plugin" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_PREFIX_PATH="$CUSTOM_INSTALL"
"$CMAKE" --build "$WORK/plugin" --config "$BUILD_TYPE"

"$CMAKE" -S "$SRC/src/test/embedded_core/app" -B "$WORK/app" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_PREFIX_PATH="$STANDARD_INSTALL"
"$CMAKE" --build "$WORK/app" --config "$BUILD_TYPE"

"$WORK/app/exrapp" "$WORK/plugin/libexrplugin.so" "$WORK"
