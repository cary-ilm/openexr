#!/usr/bin/env bash

if [[ $OSTYPE == *darwin* ]]; then
    BUILD_ROOT=/Users/cary/builds/src/cary-ilm/openexr/pybind11-Release
else
    BUILD_ROOT=/home/cary/builds/src/cary-ilm/openexr.2/pybind11-Release
fi

env ASAN_OPTIONS="verify_asan_link_order=0:detect_leaks=0" PYTHONPATH=$BUILD_ROOT/src/wrappers/python python3 test_pybind.py
