#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os
from subprocess import PIPE, run

print(f"testing exrcheck: {' '.join(sys.argv)}")

exrcheck = sys.argv[1]
image_dir = sys.argv[2]
image_files = sys.argv[3:]

def run_exrcheck(cmd):
    result = run (cmd, stdout=PIPE, stderr=PIPE, universal_newlines=True)
    cmd_string = " ".join(result.args)
    print(cmd_string)
    if result.returncode != 0:
        print(f"error: {cmd_string} failed: returncode={result.returncode}")
        print(f"stdout:\n{result.stdout}")
        print(f"stderr:\n{result.stderr}")
        sys.exit(1)
    
for exr_file in image_files:

    exr_path = f"{image_dir}/{exr_file}"

    run_exrcheck([exrcheck, exr_path])
    run_exrcheck([exrcheck, "-m", exr_path])
    run_exrcheck([exrcheck, "-t", exr_path])
    run_exrcheck([exrcheck, "-s", exr_path])
    run_exrcheck([exrcheck, "-c", exr_path])

print("success.")

