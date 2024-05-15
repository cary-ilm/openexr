#!/usr/bin/env bash


BUILD_TYPE="Debug"
BUILD_TYPE="Release"

REPO="src/cary-ilm/openexr"
BRANCH="pybind11"

ASAN_OPTIONS=""

if [[ $1 == sanitize ]]; then
    echo "sanitize!"
    BUILD_TYPE="Debug-sanitize"
    ASAN_OPTIONS="verify_asan_link_order=0:detect_leaks=0 LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/11/libasan.so"
fi

BUILD_ROOT=$HOME/builds/$REPO/$BRANCH-$BUILD_TYPE
SRC_ROOT=$HOME/$REPO
TEST_ROOT=$SRC_ROOT/src/wrappers/python/tests
PYTHONPATH=$BUILD_ROOT/src/wrappers/python

OPENEXR_TEST_IMAGE_REPO="https://raw.githubusercontent.com/AcademySoftwareFoundation/openexr-images/main"

#OPENEXR_TEST_IMAGE_REPO="file:///$HOME/src/cary-ilm/openexr-images"

set -x

#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH pytest -s $TEST_ROOT
env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH OPENEXR_TEST_IMAGE_REPO=$OPENEXR_TEST_IMAGE_REPO pytest -s $TEST_ROOT
#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH python3 $TEST_ROOT/test_images.py 
#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH python3 $TEST_ROOT/test_exceptions.py 
#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH python3 $TEST_ROOT/test_unittest.py 
#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH python3 $TEST_ROOT/test_rgba.py 

#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH OPENEXR_TEST_IMAGE_REPO=$OPENEXR_TEST_IMAGE_REPO python3 $TEST_ROOT/test_images.py
#env $ASAN_OPTIONS PYTHONPATH=$PYTHONPATH python3 $TEST_ROOT/test_images.py 



