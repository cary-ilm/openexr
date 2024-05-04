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

def required_attribute(name):
    return (name == "channels" or
            name == "compression" or
            name == "dataWindow" or
            name == "displayWindow" or
            name == "lineOrder" or 
            name == "pixelAspectRatio" or
            name == "screenWindowCenter" or
            name == "screenWindowWidth" or
            name == "tiles" or
            name == "type" or
            name == "name" or
            name == "version" or
            name == "chunkCount")

def compare_files(A, B):

    print(f"compare_files: {A} {B}")
    
    if len(A.parts) != len(B.parts):
        print(f"#parts differs: {len(A.parts)} {len(B.parts)}")
        return False
    
    for PA, PB in zip(A.parts,B.parts):
        if compare_parts(PA, PB):
            return False

    return True

def compare_parts(A, B):

    akeys = set(A.header.keys())
    bkeys = set(B.header.keys())
    
    for k in akeys-bkeys:
        if not required_attribute(k):
            print("Attribute {k} is not in both headers")
            return False
        
    for k in bkeys-akeys:
        if not required_attribute(k):
            print("Attribute {k} is not in both headers")
            return False
        
    for k in akeys.intersection(bkeys):
        if k == "preview" or k == "float":
            continue
        if A.header[k] != B.header[k]:
            print(f"attribute {k} {type(A.header[k])} differs: {A.header[k]} {B.header[k]}")
            return False

    if len(A.channels) != len(B.channels):
        print(f"#channels in {A.name} differs: {len(A.channels)} {len(B.channels)}")
        return False

    for c in A.channels.keys():
        if compare_channels(A.channels[c], B.channels[c]):
            return False

    return True

def compare_channels(A, B):

    if (A.name != B.name or
        A.type() != B.type() or
        A.xSampling != B.xSampling or
        A.ySampling != B.ySampling):
        print(f"channel {A.name} differs: {A.__repr__()} {B.__repr__()}")
        return False

    return True
        
def print_file(f, print_pixels = False):
    
    print(f"file {f.filename}")
    print(f"parts:")
    parts = f.parts
    for p in parts:
        print(f"  part: {p.name} {p.type} {p.compression} height={p.height} width={p.width}")
        h = p.header
        for a in h:
            print(f"  header[{a}] {h[a]}")
        for n,c in p.channels.items():
            print(f"  channel[{c.name}] shape={c.pixels.shape} strides={c.pixels.strides} {c.type()} {c.pixels.dtype}")
            if print_pixels:
                for y in range(0,c.pixels.shape[0]):
                    s = f"    {c.name}[{y}]:"
                    for x in range(0,c.pixels.shape[1]):
                        s += f" {c.pixels[y][x]}"
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

def test_write_uint():

    # Construct a file from scratch and write it.
    
    width = 5
    height = 10
    size = width * height
    R = np.array([i for i in range(0,size)], dtype='uint32').reshape((height, width))
    G = np.array([i*10 for i in range(0,size)], dtype='uint32').reshape((height, width))
    B = np.array([i*100 for i in range(0,size)], dtype='uint32').reshape((height, width))
    A = np.array([-i*100 for i in range(0,size)], dtype='uint32').reshape((height, width))
    A = np.array([i*5 for i in range(0,size)], dtype='uint32').reshape((height, width))
    channels = {
        "R" : OpenEXR.Channel(R, 1, 1),
        "G" : OpenEXR.Channel(G, 1, 1),
        "B" : OpenEXR.Channel(B, 1, 1),
        "A" : OpenEXR.Channel(A, 1, 1), 
    }

    header = {}

    print_pixels = True
    
    outfilename = "test_write_uint.exr"
    outfile = OpenEXR.File(header, channels,
                           OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)

    assert outfile.channels()['A'].name == "A"

    print(f"writing {outfilename}")
    outfile.write(outfilename)
    print(f"writing {outfilename} done.")
    
    print(f"File outfile:")
    print_file(outfile, print_pixels)
    
    # Verify reading it back gives the same data
    print(f"reading {outfilename}")
    infile = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done.")

    print(f"File infile:")
    print_file(infile, print_pixels)

    compare_files(infile, outfile)
    
    assert infile == outfile

def test_write_half():

    # Construct a file from scratch and write it.
    
    width = 10
    height = 20
    size = width * height
    R = np.array([i for i in range(0,size)], dtype='e').reshape((height, width))
    G = np.array([i*10 for i in range(0,size)], dtype='e').reshape((height, width))
    B = np.array([i*100 for i in range(0,size)], dtype='e').reshape((height, width))
    A = np.array([-i*100 for i in range(0,size)], dtype='e').reshape((height, width))
    channels = {
        "A" : OpenEXR.Channel("A", A, 1, 1), 
        "B" : OpenEXR.Channel("B", B, 1, 1),
        "G" : OpenEXR.Channel("G", G, 1, 1),
        "R" : OpenEXR.Channel("R", R, 1, 1)
    }

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
    infile = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done.")

    compare_files(infile, outfile)
    
    assert infile == outfile

def equalWithRelError (x1, x2, e):
    
#    return ((x1 > x2) ? x1 - x2 : x2 - x1) <= e * ((x1 > 0) ? x1 : -x1)
    return ((x1 - x2) if (x1 > x2) else (x2 - x1)) <= e * (x1 if (x1 > 0) else -x1)

def test_modify_in_place():

    #
    # Test modifying header attributes in place
    #

    filename = "test.exr"
    f = OpenEXR.File(filename)
    
    # set the value of an existing attribute
    par = 2.3
    f.parts[0].header["pixelAspectRatio"] = par

    # add a new attribute
    f.parts[0].header["foo"] = "bar"

    dt = np.dtype({
            "names": ["r", "g", "b", "a"],
            "formats": ["u4", "u4", "u4", "u4"],
            "offsets": [0, 4, 8, 12],
        })
    pwidth = 3
    pheight = 3
    psize = pwidth * pheight
    P = np.array([ [(0,0,0,0), (1,1,1,1), (2,2,2,2) ],
                   [(3,3,3,3), (4,4,4,4), (5,5,5,5) ],
                   [(6,6,6,6), (7,7,7,7), (8,8,8,8) ] ], dtype=dt).reshape((pwidth,pheight))
    f.parts[0].header["preview"] = OpenEXR.PreviewImage(P)

    # Modify a pixel value
    f.parts[0].channels["R"].pixels[0][1] = 42.0
    f.parts[0].channels["G"].pixels[2][3] = 666.0
    
    # write to a new file
    modified_filename = "modified.exr"
    f.write(modified_filename)
    
    # read the new file
    m = OpenEXR.File(modified_filename)

    # validate the values are the same
    eps = 1e-5
    assert equalWithRelError(m.parts[0].header["pixelAspectRatio"], par, eps)
    assert m.parts[0].header["foo"] == "bar"
    assert np.array_equal(m.parts[0].header["preview"].pixels, P)
    
    assert equalWithRelError(m.parts[0].channels["R"].pixels[0][1], 42.0, eps)
    assert equalWithRelError(m.parts[0].channels["G"].pixels[2][3], 666.0, eps)

def test_preview_image():

    dt = np.dtype({
            "names": ["r", "g", "b", "a"],
            "formats": ["u4", "u4", "u4", "u4"],
            "offsets": [0, 4, 8, 12],
        })
    pwidth = 3
    pheight = 3
    psize = pwidth * pheight
    P = np.array([(i,i,i,i) for i in range(0,psize)], dtype=dt).reshape((pwidth,pheight))

    header = {}
    header["preview"] = OpenEXR.PreviewImage(P)

    width = 10
    height = 20
    size = width * height
    Z = np.array([i for i in range(0,size)], dtype='f').reshape((height, width))
    
    channels = { "Z" : OpenEXR.Channel("Z", Z, 1, 1) }

    outfile = OpenEXR.File(header, channels,
                           OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)
    outfilename = "test_preview_image.exr"
    outfile.write(outfilename)

    infile = OpenEXR.File(outfilename)

    Q = infile.header()["preview"].pixels
    
    assert np.array_equal(P, Q)
    assert infile == outfile
    
def test_write_float():

    # Construct a file from scratch and write it.
    
    width = 50
    height = 1
    size = width * height
    R = np.array([i for i in range(0,size)], dtype='f').reshape((height, width))
    G = np.array([i*10 for i in range(0,size)], dtype='f').reshape((height, width))
    B = np.array([i*100 for i in range(0,size)], dtype='f').reshape((height, width))
    A = np.array([i*1000 for i in range(0,size)], dtype='f').reshape((height, width))
    channels = {
        "R" : OpenEXR.Channel("R", R, 1, 1),
        "G" : OpenEXR.Channel("G", G, 1, 1),
        "B" : OpenEXR.Channel("B", B, 1, 1),
        "A" : OpenEXR.Channel("A", A, 1, 1)
    }

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
    print_file(outfile)

    outfile.write(outfilename)
    print(f"writing {outfilename} done.")
    
    print_file(outfile)
    
    # Verify reading it back gives the same data
    print(f"reading {outfilename}")
    infile = OpenEXR.File(outfilename)
    print(f"reading {outfilename} done.")

    compare_files(infile, outfile)

    assert infile == outfile
    
def test_write_2part():

    #
    # Construct a 2-part file by replicating the header and channels
    #
    
    width = 10
    height = 20
    size = width * height
    R = np.array([i for i in range(0,size)], dtype='f').reshape((height, width))
    G = np.array([i*10 for i in range(0,size)], dtype='f').reshape((height, width))
    B = np.array([i*100 for i in range(0,size)], dtype='f').reshape((height, width))
    A = np.array([i*1000 for i in range(0,size)], dtype='f').reshape((height, width))
    channels = {
        "R" : OpenEXR.Channel("R", R, 1, 1),
        "G" : OpenEXR.Channel("G", G, 1, 1),
        "B" : OpenEXR.Channel("B", B, 1, 1),
        "A" : OpenEXR.Channel("A", A, 1, 1)
    }

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
    
    P1 = OpenEXR.Part("P1", header, channels,
                      OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)
    P2 = OpenEXR.Part("P2", header, channels,
                      OpenEXR.scanlineimage, OpenEXR.ZIP_COMPRESSION)
    parts = [P1, P2]
    outfilename2 = "test_write2.exr"
    outfile2 = OpenEXR.File(parts)
    outfile2.write(outfilename2)
    
    # Verify reading it back gives the same data
    i = OpenEXR.File(outfilename2)
    assert i == outfile2

    print_file(i)

if os.path.isfile(filename):
    test_modify_in_place()
    test_preview_image()
    test_write_uint()
    test_write_half()
    test_write_float()
    test_write_2part()
    test_read_write()

    print("ok")
else:    
    print(f"skipping {sys.argv[0]}: no such file: {filename}")



    


