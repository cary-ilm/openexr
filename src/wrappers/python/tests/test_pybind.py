#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

from __future__ import print_function
import sys
import os

import OpenEXR
import Imath

#print(f"OpenEXR: {OpenEXR.__version__} {OpenEXR.__file__}")
print(f"OpenEXR: {OpenEXR.__file__}")

filename = "multipart.exr"
filename = "GoldenGate.exr"
filename = "10x100.exr"
filename = "test.exr"

def test_file():

    i = OpenEXR.File(filename)

    print(f"parts:")
    parts = i.parts()
    for p in parts:
        print(f"  part: {p.name} {p.type} {p.compression} {p.width}x{p.height}")
        h = p.header()
        for a in h:
            print(f"    {a}: {h[a]}")
    for c in p.channels():
         pixels = c.pixels
         print(f"  channel {c.name} {pixels.shape}")
    #     for y in range(0,min(p.height,100)):
    #         s = f"{c.name}[{y}]:"
    #         for x in range(0,min(p.width,100)):
    #             s += f" {pixels[x][y]}"
    #         print(s)
    
    print(f"writing out.exr")
    i.write("out.exr")


    print("reading out.exr")
    i = OpenEXR.File("out.exr")
    print(f"parts:")
    parts = i.parts()
    for p in parts:
        print(f"  part: {p.name} {p.type} {p.compression} {p.width}x{p.height}")
        h = p.header()
        for a in h:
            print(f"    {a}: {h[a]}")
    for c in p.channels():
         pixels = c.pixels
         print(f"  channel {c.name} {pixels.shape}")
    
    print(f"writing out2.exr")
    i.write("out2.exr")

def test_keycode():
    k = OpenEXR.KeyCode()
    print(f"filmMfcCode={k.filmMfcCode}")
    print(f"filmType={k.filmType}")
    print(f"prefix={k.prefix}")
    print(f"count={k.count}")
    print(f"perfOffset={k.perfOffset}")
    print(f"perfsPerFrame={k.perfsPerFrame}")
    print(f"perfsPerCount={k.perfsPerCount}")

def test_rational():
    r = OpenEXR.Rational(1,2)
    print(f"r.n={r.n}")
    print(f"r.d={r.d}")

if os.path.isfile(filename):
    test_file()
    print("ok")
else:    
    print(f"skipping {sys.argv[0]}: no such file: {filename}")

    



