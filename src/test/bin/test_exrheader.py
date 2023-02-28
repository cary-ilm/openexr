#!/usr/bin/env python

import sys, os
from subprocess import PIPE, run

print(f"testing exrheader in python: {sys.argv}")

exrheader = f"{sys.argv[1]}/exrheader"
src_dir = f"{sys.argv[2]}"

# no args = usage message
result = run ([exrheader], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

# -h = usage message
result = run ([exrheader, "-h"], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

def find_line(keyword, lines):
    for line in lines:
        if keyword in line:
            return line
    return None

# attributes
image = f"{src_dir}/GrayRampsHorizontal.exr"
result = run ([exrheader, image], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)
output = result.stdout.split('\n')
print(f"result.stdout={result.stdout}")

assert ("2, flags 0x0" in find_line("file format version:", output))
assert ("pxr24" in find_line ("compression", output))
assert ("(0 0) - (799 799)" in find_line ("dataWindow", output))
assert ("(0 0) - (799 799)" in find_line ("displayWindow", output))
assert ("increasing y" in find_line ("lineOrder", output))
assert ("1" in find_line ("pixelAspectRatio", output))
assert ("(0 0)" in find_line ("screenWindowCenter", output))
assert ("1" in find_line ("screenWindowWidth", output))
assert ("scanlineimage" in find_line ("type (type string)", output))

print("success")









