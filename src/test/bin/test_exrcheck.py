#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os
from subprocess import PIPE, run

print(f"testing exrcheck: {sys.argv}")
      
bin_dir = sys.argv[1]
exrcheck = f"{bin_dir}/exrcheck"

for exr_file in sys.argv[3:]:
    result = run ([exrcheck, exr_file], stdout=PIPE, stderr=PIPE, universal_newlines=True)
    print(f"exrcheck {' '.join(result.args[1:])}")
    assert(result.returncode == 0)
    print(result.stdout)

print("success.")
