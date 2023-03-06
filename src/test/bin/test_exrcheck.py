#!/usr/bin/env python

import sys, os
from subprocess import PIPE, run

print(f"testing exrcheck in python: {sys.argv}")
print(f"cwd: {os.getcwd()}")
      
bin_dir = sys.argv[1]
src_dir = sys.argv[2]
exr_file = sys.argv[3]

exrinfo = f"{bin_dir}/exrinfo"
exrcheck = f"{bin_dir}/exrcheck"

result = run (["file", exr_file], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(f"file {' '.join(result.args[1:])}")
print(result.stdout)

result = run ([exrcheck, exr_file], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(f"exrcheck {' '.join(result.args[1:])}")
print(result.stdout)

print("success.")
