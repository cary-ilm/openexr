#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

from __future__ import print_function
import sys
import os
import random
from array import array

import OpenEXRp11 as OpenEXR
import Imath

print(f"OpenEXR: {OpenEXR.__version__} {OpenEXR.__file__}")

filename = "tiled.exr"
filename = "test.exr"
filename = "multipart.exr"
filename = "10x100.exr"

i = OpenEXR.File(filename)
print(f"parts:")
parts = i.parts()
for p in parts:
    print(f"  part: {p.name} {p.type} {p.compression} {p.width}x{p.height}")
    h = p.header()
    for a in h:
        print(f"    {a}: {h[a]}")
        if a == "tiles":
            t = h[a]
            lm = t.mode
            rm = t.roundingMode
            print(f">> tile description: level mode={lm}, round mode={rm}")

    for c in p.channels():
        pixels = c.pixels
        for y in range(0,p.height):
            s = f"{c.name}[{y}]:"
            for x in range(0,p.width):
                s += f" {pixels[x][y]}"
            print(s)
    
#print(f"header:")
#h = i.header()
#for a in h:
#    print(f"  {a}: {h[a]}")

#print(f"channels:")
#for c in i.channels():
#    print(f"  {c.name}: {c.type}")
#
#print("ok")

exit(0)

k = OpenEXR.KeyCode()
print(f"filmMfcCode={k.filmMfcCode}")
print(f"filmType={k.filmType}")
print(f"prefix={k.prefix}")
print(f"count={k.count}")
print(f"perfOffset={k.perfOffset}")
print(f"perfsPerFrame={k.perfsPerFrame}")
print(f"perfsPerCount={k.perfsPerCount}")
exit(0)

r = OpenEXR.Rational(1,2)
print(f"r.n={r.n}")
print(f"r.d={r.d}")
exit(0)


