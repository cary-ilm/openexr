#!/usr/bin/env bash

#
# Validate the libary symlinks:
#   * The actual elf binary is, e.g. libIlmThread-3_1.so.29.0.0
#   * The symlinks are:
#       libIlmThread.so -> libIlmThread-3_1.so
#       libIlmThread-3_1.so -> libIlmThread-3_1.so.29
#       libIlmThread-3_1.so.29 -> libIlmThread-3_1.so.29.0.0
#
# Extract the version by compiling a program that prints the
# OPENEXR_VERSION_STRING. This also validates that the program
# compiles and executes with the info from pkg-config.
# 

BUILD_ROOT=$1
SRC_ROOT=$2

echo "Running validate_openexr_libs.sh..."
echo "BUILD_ROOT=$BUILD_ROOT"
echo "SRC_ROOT=$SRC_ROOT"
echo "find:"
find $BUILD_ROOT -print

pkgconfig=`find $BUILD_ROOT -name OpenEXR.pc`
echo "pkgconfig=$pkgconfig"

if [[ "$pkgconfig" == "" ]]; then
    echo "Can't find OpenEXR.pc"
    exit -1
fi    
export PKG_CONFIG_PATH=`dirname $pkgconfig`
echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"

CXX_FLAGS=`pkg-config OpenEXR --cflags`
LD_FLAGS=`pkg-config OpenEXR --libs --static`

TMP_DIR=`mktemp -d /var/tmp/validate_XXX`

echo -e '#include <ImfHeader.h>\n#include <OpenEXRConfig.h>\n#include <stdio.h>\nint main() { puts(OPENEXR_PACKAGE_STRING); Imf::Header h; return 0; }' > $TMP_DIR/validate.cpp

g++ $CXX_FLAGS $TMP_DIR/validate.cpp -o $TMP_DIR/validate $LD_FLAGS

# Execute the program

export LD_LIBRARY_PATH=$BUILD_ROOT/lib
validate=`$TMP_DIR/validate`
status=$?

echo $validate

rm -rf $TMP_DIR

if [[ "$status" != "0" ]]; then
   echo "validate failed: $validate"
   exit -1
fi

# Get the suffix, e.g. -2_5_d, and determine if there's also a _d
libsuffix=`pkg-config OpenEXR --variable=libsuffix`
if [[ $libsuffix != `basename ./$libsuffix _d` ]]; then
    _d="_d"
else
    _d=""
fi

# Validate each of the libs
libs=`pkg-config OpenEXR --libs-only-l | sed -e s/-l//g`
for lib in $libs; do

    base=`echo $lib | cut -d- -f1`
    suffix=`echo $lib | cut -d- -f2`

    if [[ -f $BUILD_ROOT/lib/lib$base$_d.so ]]; then 
        libbase=`readlink $BUILD_ROOT/lib/lib$base$_d.so`
        libcurrent=`readlink $BUILD_ROOT/lib/$libbase`
        libversion=`readlink $BUILD_ROOT/lib/$libcurrent`
        file $BUILD_ROOT/lib/$libversion | grep -q "ELF"

        if [[ "$?" != 0 ]]; then
            echo "Broken libs: lib$base.so -> $libbase -> $libcurrent -> $libversion"
            exit -1
        fi

        echo "lib$base.so -> $libbase -> $libcurrent -> $libversion"

    elif [[ ! -f $BUILD_ROOT/lib/lib$lib.a ]]; then
        echo "No static lib: $BUILD_ROOT/lib/lib$lib.a"
    else
        echo "Static lib lib$lib.a"
    fi

done

# Confirm no broken .so symlinks 
file $BUILD_ROOT/lib/lib* | grep -q broken 
if [[ "$?" == "0" ]]; then
  echo "broken symbolic link"
  exit -1
fi

if [[ "$SRC_ROOT" != "" ]]; then
    version=`pkg-config OpenEXR --modversion`
    notes=`grep "\* \[Version $version\]" $SRC_ROOT/CHANGES.md | head -1`
    if [[ "$notes" == "" ]]; then
        echo "No release notes."
    else
        echo "Release notes: $notes"
    fi
fi
   
echo "ok."
