#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os
from subprocess import PIPE, run

print(f"testing exrcheck: {' '.join(sys.argv)}")

exrcheck = sys.argv[1]
image_dir = sys.argv[2]
image_files = sys.argv[3:]

if not os.path.isfile(exrcheck) or not os.access(exrcheck, os.X_OK):
    print(f"error: no such file: {exrcheck}")
    sys.exit(1)

def do_run(cmd, expect_error = False):
    cmd_string = " ".join(cmd)
    print(cmd_string)
    result = run (cmd, stdout=PIPE, stderr=PIPE, universal_newlines=True)
    if expect_error and result.returncode == 0:
        print(f"error: {cmd_string} did not fail as expected")
        print(f"stdout:\n{result.stdout}")
        print(f"stderr:\n{result.stderr}")
        sys.exit(1)
    if result.returncode != 0:
        print(f"error: {cmd_string} failed: returncode={result.returncode}")
        print(f"stdout:\n{result.stdout}")
        print(f"stderr:\n{result.stderr}")
        sys.exit(1)
    return result

for exr_file in image_files:

    exr_path = f"{image_dir}/{exr_file}"

    do_run([exrcheck, exr_path], True)
    do_run([exrcheck, "-m", exr_path])
    do_run([exrcheck, "-t", exr_path])
    do_run([exrcheck, "-s", exr_path])
    do_run([exrcheck, "-c", exr_path])

print("success.")
sys.exit(0)

