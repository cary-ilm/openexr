#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exrmakepreview: {' '.join(sys.argv)}")

exrmakepreview = sys.argv[1]
exrinfo = sys.argv[2]
image_dir = sys.argv[3]
version = sys.argv[4]

assert(os.path.isfile(exrmakepreview)), "\nMissing " + exrmakepreview
assert(os.path.isfile(exrinfo)), "\nMissing " + exrinfo
assert(os.path.isdir(image_dir)), "\nMissing " + image_dir


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
result = do_run ([exrmakepreview], True)
assert result.stderr.startswith ("Usage: ")

# -h = usage message
result = do_run ([exrmakepreview, "-h"])
assert result.stdout.startswith ("Usage: ")

result = do_run ([exrmakepreview, "--help"])
assert result.stdout.startswith ("Usage: ")

# --version
result = do_run ([exrmakepreview, "--version"])
assert result.stdout.startswith ("exrmakepreview")
assert version in result.stdout


def find_line(keyword, lines):
    for line in lines:
        if line.startswith(keyword):
            return line
    return None

fd, outimage = tempfile.mkstemp(".exr")
os.close(fd)

def cleanup():
    print(f"deleting {outimage}")
atexit.register(cleanup)

image = f"{image_dir}/TestImages/GrayRampsHorizontal.exr"
result = do_run ([exrmakepreview, "-w", "50", "-e", "1", "-v", image, outimage])

result = do_run ([exrinfo, "-v", outimage])
output = result.stdout.split('\n')
assert "preview 50 x 50" in find_line("  preview", output)

print("success")









