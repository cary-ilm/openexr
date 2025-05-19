#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os
from subprocess import PIPE, run

print(f"testing exrinfo: {' '.join(sys.argv)}")

exrinfo = sys.argv[1]
image_dir = sys.argv[2]
version = sys.argv[3]

def do_run(cmd, expect_error = False):
    cmd_string = " ".join(cmd)
    print(cmd_string)
    result = run (cmd, stdout=PIPE, stderr=PIPE, universal_newlines=True)
    if expect_error and result.returncode == 0:
        print(f"error: {cmd_string} did not fail as expected")
        print(f"stdout:\n{result.stdout}")
        print(f"stderr:\n{result.stderr}")
        sys.exit(1)
    if result.returncode != 0 or :
        print(f"error: {cmd_string} failed: returncode={result.returncode}")
        print(f"stdout:\n{result.stdout}")
        print(f"stderr:\n{result.stderr}")
        sys.exit(1)
    return result

result = do_run ([exrinfo, "-h"], True)
assert result.stdout.startswith ("Usage: ")

result = do_run ([exrinfo, "--help"])
assert result.stdout.startswith ("Usage: ")

# --version
result = do_run ([exrinfo, "--version"])
assert result.stdout.startswith ("exrinfo")
assert version in result.stdout

image = f"{image_dir}/TestImages/GrayRampsHorizontal.exr"
result = do_run ([exrinfo, image, "-a", "-v"])
output = result.stdout.split('\n')
try:
    assert ('pxr24' in output[1])
    assert ('800 x 800' in output[2])
    assert ('800 x 800' in output[3])
    assert ('1 channels' in output[4])
except AssertionError:
    print(result.stdout)
    raise

# test image as stdio
with open(image, 'rb') as f:
    data = f.read()
result = do_run ([exrinfo, '-', "-a", "-v"])
output = result.stdout.decode().split('\n')
try:
    assert ('pxr24' in output[1])
    assert ('800 x 800' in output[2])
    assert ('800 x 800' in output[3])
    assert ('1 channels' in output[4])
except AssertionError:
    print(result.stdout)
    raise

print("success")

