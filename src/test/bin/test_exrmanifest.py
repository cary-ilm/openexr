#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exrmanifest: {sys.argv}")

src_dir = os.path.dirname (sys.argv[0])
exrmanifest = sys.argv[1]
exrinfo = sys.argv[2]
image_dir = sys.argv[3]
version = sys.argv[4]

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

# no args = usage message, error
result = do_run ([exrmanifest])
assert result.stderr.startswith ("Usage: ")

# -h = usage message
result = do_run ([exrmanifest, "-h"])
assert result.stdout.startswith ("Usage: ")

result = do_run ([exrmanifest, "--help"])
assert result.stdout.startswith ("Usage: ")

# --version
result = do_run ([exrmanifest, "--version"])
assert result.stdout.startswith ("exrmanifest")
assert version in result.stdout), "\n"+result.stdout

# invalid arguments
result = do_run ([exrmanifest, "foo.exr", "bar.exr"])
assert result.returncode != 0), "\n"+result.stderr

for test_image in ["11.deep.exr", "42.deep.exr", "64.deep.exr", "multivariate.deep.exr", "objectid.deep.exr"]:
    test_file = src_dir + "/test_images/" + test_image
    result = do_run ([exrmanifest, test_file])
    stdout_is = result.stdout
    with open (test_file + ".txt", 'r') as file:
        stdout_should_be = file.read()
        assert stdout_is == stdout_should_be

print("success")









