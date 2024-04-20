#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

from __future__ import print_function
import sys
import os
import numpy as np

import OpenEXR

print(f"OpenEXR: {OpenEXR.__version__} {OpenEXR.__file__}")

filename = "multipart.exr"
filename = "GoldenGate.exr"
filename = "10x100.exr"
filename = "test.exr"

def test_read_write():

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
    
    print(f"writing write.exr")
    i.write("write.exr")


    print("reading write.exr")
    i = OpenEXR.File("write.exr")
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
    
def test_attributes():
    i = OpenEXR.File(filename)
    print(f"writing out.exr")
    i.write("out.exr")

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
#    test_file()
    test_attributes()
    print("ok")
else:    
    print(f"skipping {sys.argv[0]}: no such file: {filename}")

def test_write:

    H = {}
    H["chromaticities"] = OpenEXR.Chromaticities(1,2,3,4,5,6)
    H["box2i"] = OpenEXR.Box2(V2i(0,1),V2i(2,3))
    H["box2f"] = OpenEXR.Box2f(V2f(0,1),V2f(2,3))
    H["compression"] = OpenEXR.ZIPS_COMPRESSION
    H["float"] = 4.2
    H["int"] = 42
    H["keycode"] = OpenEXR.KeyCode(1,2,3,4,5,6,7)
    H["lineorer"] = OpenEXR.INCREASING_Y
    H["m33f"] = OpenEXR.M33f()
    H["m33d"] = OpenEXR.M33d()
    H["m44f"] = OpenEXR.M44f()
    H["m44d"] = OpenEXR.M44d()
    H["preview"] = OpenEXR.PreviewImage()
    H["rational"] = OpenEXR.Rational(1,3)
    H["string"] = "stringy"
    H["timecode"] = OpenEXR.TimeCode(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18)
    H["v2i"] = OpenEXR.V2i(1,2)
    H["v2f"] = OpenEXR.V2f(1.2,3.4)
    H["v2d"] = OpenEXR.V2d(1.2,3.4)
    H["v3i"] = OpenEXR.V3i(1,2)
    H["v3f"] = OpenEXR.V3f(1.2,3.4)
    H["v3d"] = OpenEXR.V3d(1.2,3.4)
    
    width = 10
    height = 20
    size = width
    R = np.array([i for i in range(0,size)], dtype=float)
    G = np.array([i*10 for i in range(0,size)], dtype=float)
    B = np.array([i*100 for i in range(0,size)], dtype=float)
    A = np.array([i*1000 for i in range(0,size)], dtype=float)
    channels = [ Channel("R", R), Channel("G", G), Channel("B", B), Channel("A", A) ] 
    o = OpenEXR.File(H, channels)
    o.write("1part.exr")

    OpenEXR.write("1part.exr", H, channels)

    P1 = Part(H, "P1", channels)
    P2 = Part(H, "P2", channels)
    parts = [P, P1, P2]
    o = OpenEXR.File(parts)
    o.write("3part.exr")
    OpenEXR.write("3part.exr", [P, P1, P2])
    


    


