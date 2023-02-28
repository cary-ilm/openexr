#!/usr/bin/env python

import sys, os
from subprocess import PIPE, run

print(f"testing exrinfo in python: {sys.argv}")

exrinfo = f"{sys.argv[1]}/exrinfo"
src_dir = f"{sys.argv[2]}"

result = run ([exrinfo, "-h"], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)
assert(result.stderr.startswith ("Usage: "))

image = f"{src_dir}/GrayRampsHorizontal.exr"
result = run ([exrinfo, image, "-a", "-v"], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)
output = result.stdout.split('\n')
assert ('pxr24' in output[1])
assert ('800 x 800' in output[2])
assert ('800 x 800' in output[3])
assert ('1 channels' in output[4])

with  open(image, 'rb') as f:
    data = f.read()
result = run ([exrinfo, '-', "-a", "-v"], input=data, stdout=PIPE, stderr=PIPE)
print(" ".join(result.args))
assert(result.returncode == 0)
output = result.stdout.decode().split('\n')
assert ('pxr24' in output[1])
assert ('800 x 800' in output[2])
assert ('800 x 800' in output[3])
assert ('1 channels' in output[4])

print("success")
