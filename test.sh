#!/usr/bin/env bash

BUILD_TYPE="Release"
BUILD_TYPE="Debug"

if [[ $OSTYPE == *darwin* ]]; then
    BUILD_ROOT=/Users/cary/builds/src/cary-ilm/openexr/pybind11-$BUILD_TYPE
else
    BUILD_ROOT=/home/cary/builds/src/cary-ilm/openexr/pybind11-$BUILD_TYPE
fi

ln -s $BUILD_ROOT/src/wrappers/python/OpenEXR_d.so $BUILD_ROOT/src/wrappers/python/OpenEXR.so

#ASAN_OPTIONS="verify_asan_link_order=0:detect_leaks=0 LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/11/libasan.so"
ASAN_OPTIONS=""

set -x

#env $ASAN_OPTIONS PYTHONPATH=$BUILD_ROOT/src/wrappers/python pytest -s `pwd`/src/wrappers/python/tests
#env $ASAN_OPTIONS PYTHONPATH=$BUILD_ROOT/src/wrappers/python python3 ~/src/cary-ilm/openexr/src/wrappers/python/tests/test_images.py 
env $ASAN_OPTIONS PYTHONPATH=$BUILD_ROOT/src/wrappers/python python3 ~/src/cary-ilm/openexr/src/wrappers/python/tests/test_unittest.py
#env $ASAN_OPTIONS PYTHONPATH=$BUILD_ROOT/src/wrappers/python python3 ~/src/cary-ilm/openexr/src/wrappers/python/tests/test_exceptions.py 


