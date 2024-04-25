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

#print(f"OpenEXR: {OpenEXR.__version__} {OpenEXR.__file__}")

filename = "multipart.exr"
filename = "GoldenGate.exr"
filename = "10x100.exr"
filename = "test.exr"

def print_file(f, print_pixels = False):
    
    print(f"file {f.filename}")
    print(f"parts:")
    parts = f.parts()
    for p in parts:
        print(f"  part: {p.name} {p.type} {p.compression} {p.width}x{p.height}")
        h = p.attributes
        for a in h:
            print(f"  header[{a}] {h[a]}")
        for c in p.channels:
            pixels = c.pixels
            print(f"  channel[{c.name}] {pixels.shape} {pixels.dtype}")
            if print_pixels:
                for y in range(0,p.height):
                    s = f"    {c.name}[{y}]:"
                    for x in range(0,p.width):
                        s += f" {pixels[x][y]}"
                    print(s)
    
def test_read_write():

    #
    # Read a file and write it back out, then read the freshly-written
    # file to validate it's the same.
    #
    
    infilename = "test.exr"
    infile = OpenEXR.File(infilename)

    outfilename = "test_read_write.exr"
    print(f"writing {outfilename}")
    infile.write(outfilename)
    print(f"writing {outfilename} done.")

    print(f"reading {outfilename}")
    outfile = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done")
    
    assert outfile == infile

    print("ok")
    
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

def test_write_half():

    # Construct a file from scratch and write it.
    
    width = 10
    height = 20
    size = width * height
    R = np.array([i for i in range(0,size)], dtype='e').reshape((width, height))
    G = np.array([i*10 for i in range(0,size)], dtype='e').reshape((width, height))
    B = np.array([i*100 for i in range(0,size)], dtype='e').reshape((width, height))
    A = np.array([-i*100 for i in range(0,size)], dtype='e').reshape((width, height))
    channels = [ OpenEXR.Channel("A", OpenEXR.HALF, 1, 1, A), 
                 OpenEXR.Channel("B", OpenEXR.HALF, 1, 1, B),
                 OpenEXR.Channel("G", OpenEXR.HALF, 1, 1, G),
                 OpenEXR.Channel("R", OpenEXR.HALF, 1, 1, R) ]

    header = {}

    outfilename = "test_write_half.exr"
    outfile = OpenEXR.File(header, channels,
                           OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)
    print(f"writing {outfilename}")
    outfile.write(outfilename)
    print(f"writing {outfilename} done.")
    
    print_file(outfile, True)
    
    # Verify reading it back gives the same data
    print(f"reading {outfilename}")
    i = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done.")

    assert i == outfile

def test_write():

    # Construct a file from scratch and write it.
    
    width = 10
    height = 20
    size = width * height
    R = np.array([i for i in range(0,size)], dtype=float).reshape((width, height))
    G = np.array([i*10 for i in range(0,size)], dtype=float).reshape((width, height))
    B = np.array([i*100 for i in range(0,size)], dtype=float).reshape((width, height))
    A = np.array([i*1000 for i in range(0,size)], dtype=float).reshape((width, height))
    channels = [ OpenEXR.Channel("R", OpenEXR.FLOAT, 1, 1, R),
                 OpenEXR.Channel("G", OpenEXR.FLOAT, 1, 1, G),
                 OpenEXR.Channel("B", OpenEXR.FLOAT, 1, 1, B),
                 OpenEXR.Channel("A", OpenEXR.FLOAT, 1, 1, A) ] 

    pwidth = 3
    pheight = 3
    psize = pwidth * pheight

    dt = np.dtype({
            "names": ["r", "g", "b", "a"],
            "formats": ["u4", "u4", "u4", "u4"],
            "offsets": [0, 4, 8, 12],
        })
    P = np.array([(i,i,i,i) for i in range(0,psize)], dtype=dt).reshape((pwidth,pheight))
    print(f"P: {P}")

    header = {}
    header["floatvector"] = [1.0, 2.0, 3.0]
    header["stringvector"] = ["do", "re", "me"]
    header["chromaticities"] = OpenEXR.Chromaticities(1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0)
    header["box2i"] = OpenEXR.Box2i(OpenEXR.V2i(0,1),OpenEXR.V2i(2,3))
    header["box2f"] = OpenEXR.Box2f(OpenEXR.V2f(0,1),OpenEXR.V2f(2,3))
    header["compression"] = OpenEXR.ZIPS_COMPRESSION
    header["double"] = OpenEXR.Double(42000)
    header["float"] = 4.2
    header["int"] = 42
    header["keycode"] = OpenEXR.KeyCode(0,0,0,0,0,4,64)
    header["lineorder"] = OpenEXR.INCREASING_Y
    header["m33f"] = OpenEXR.M33f(1,0,0,0,1,0,0,0,1)
    header["m33d"] = OpenEXR.M33d(1,0,0,0,1,0,0,0,1)
    header["m44f"] = OpenEXR.M44f(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
    header["m44d"] = OpenEXR.M44d(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
    header["preview"] = OpenEXR.PreviewImage(P)
    header["rational"] = OpenEXR.Rational(1,3)
    header["string"] = "stringy"
    header["timecode"] = OpenEXR.TimeCode(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18)
    header["v2i"] = OpenEXR.V2i(1,2)
    header["v2f"] = OpenEXR.V2f(1.2,3.4)
    header["v2d"] = OpenEXR.V2d(1.2,3.4)
    header["v3i"] = OpenEXR.V3i(1,2,3)
    header["v3f"] = OpenEXR.V3f(1.2,3.4,5.6)
    header["v3d"] = OpenEXR.V3d(1.2,3.4,5.6)
    
    outfilename = "test_write.exr"
    outfile = OpenEXR.File(header, channels,
                           OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)
    print(f"writing {outfilename}")
    outfile.write(outfilename)
    print(f"writing {outfilename} done.")
    
    print_file(outfile)
    
    # Verify reading it back gives the same data
    print(f"reading {outfilename}")
    i = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done.")

    assert i == outfile
    
    print_file(i)
    
    #
    # Construct a 2-part file by replicating the header and channels
    #
    
    P1 = OpenEXR.Part(header, channels,
                      OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION, "P1")
    P2 = OpenEXR.Part(header, channels,
                      OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION, "P2")
    parts = [P1, P2]
    outfilename2 = "test_write2.exr"
    outfile2 = OpenEXR.File(parts)
    outfile2.write(outfilename2)
    
    # Verify reading it back gives the same data
    i = OpenEXR.File(outfilename2)
    assert i == outfile2

    print_file(i)

if os.path.isfile(filename):
#    test_write_half()
    test_write()
#    test_read_write()
    print("ok")
else:    
    print(f"skipping {sys.argv[0]}: no such file: {filename}")



    


