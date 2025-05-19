#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exrmaketiled: {' '.join(sys.argv)}")

exrmaketiled = sys.argv[1]
exrinfo = sys.argv[2]
image_dir = sys.argv[3]
version = sys.argv[4]

image = f"{image_dir}/TestImages/GammaChart.exr"

assert(os.path.isfile(exrmaketiled)), "\nMissing " + exrmaketiled
assert(os.path.isfile(exrinfo)), "\nMissing " + exrinfo
assert(os.path.isdir(image_dir)), "\nMissing " + image_dir
assert(os.path.isfile(image)), "\nMissing " + image

fd, outimage = tempfile.mkstemp(".exr")
os.close(fd)

def cleanup():
    print(f"deleting {outimage}")
atexit.register(cleanup)

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

# no args = usage message
result = do_run ([exrmaketiled], True)
assert result.stderr.startswith ("Usage: ")

# -h = usage message
result = do_run ([exrmaketiled, "-h"])
assert result.stdout.startswith ("Usage: ")

result = do_run ([exrmaketiled, "--help"])
assert result.stdout.startswith ("Usage: ")

# --version
result = do_run ([exrmaketiled, "--version"])
assert result.stdout.startswith ("exrmaketiled")
assert version in result.stdout

result = do_run ([exrmaketiled, image, outimage])
assert os.path.isfile(outimage)

result = do_run ([exrinfo, "-v", outimage])
assert 'tiled image has levels: x 1 y 1' in result.stdout

print("success")
