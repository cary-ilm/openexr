#!/usr/bin/env bash

if [[ $OSTYPE == *darwin* ]]; then
    BUILD_ROOT=/Users/cary/builds/src/cary-ilm/openexr/pybind11-Release
else
    BUILD_ROOT=/home/cary/builds/src/cary-ilm/openexr/pybind11-Release
fi

env PYTHONPATH=$BUILD_ROOT/src/wrappers/python python3 test_pybind.py

