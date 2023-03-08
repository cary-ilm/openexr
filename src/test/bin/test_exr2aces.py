#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exr2aces: {sys.argv}")

exr2aces = f"{sys.argv[1]}/exr2aces"
exrheader = f"{sys.argv[1]}/exrheader"
image_dir = f"{sys.argv[2]}"

# no args = usage message
result = run ([exr2aces], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

# -h = usage message
result = run ([exr2aces, "-h"], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

def find_line(keyword, lines):
    for line in lines:
        if line.startswith(keyword):
            return line
    return None

fd, outimage = tempfile.mkstemp(".exr")

def cleanup():
    print(f"deleting {outimage}")
atexit.register(cleanup)

image = f"{image_dir}/TestImages/GrayRampsHorizontal.exr"
result = run ([exr2aces, "-v", image, outimage], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)

result = run ([exrheader, outimage], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)

# confirm the output has the proper chromaticities
assert("chromaticities" in result.stdout)
assert("red   (0.7347 0.2653)" in result.stdout)
assert("green (0 1)" in result.stdout)
assert("blue  (0.0001 -0.077)" in result.stdout)
assert("white (0.32168 0.33767)" in result.stdout)

print("success")









