#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

from __future__ import print_function
import sys
import os
import tempfile
import atexit
import unittest
import numpy as np

import OpenEXR

def test_rgb(type):

    # Construct an RGB channel
    
    height = 5
    width = 4
    nrgba = 3
    size = width * height * nrgba
    RGB = np.array([i for i in range(0,size)], dtype=type).reshape((height, width, nrgba))
    channels = { "RGB" : OpenEXR.Channel(RGB) }

    header = {}
    outfile = OpenEXR.File(header, channels)

    outfile.write("out.exr")

    # 
    # Read as separate channels
    #
    
    infile = OpenEXR.File("out.exr")

    R = infile.channels()["R"].pixels
    G = infile.channels()["G"].pixels
    B = infile.channels()["B"].pixels

    shape = R.shape
    width = shape[1]
    height = shape[0]
    for y in range(0,height):
        for x in range(0,width):
            r = R[y][x]
            g = G[y][x]
            b = B[y][x]
            assert r == RGB[y][x][0]
            assert g == RGB[y][x][1]
            assert b == RGB[y][x][2]
            
    #
    # Read as RGB channel
    #
    
    infile = OpenEXR.File("out.exr", rgba=True)

    inRGB = infile.channels()["RGB"].pixels
    shape = inRGB.shape
    width = shape[1]
    height = shape[0]
    assert shape[2] == 3
    
    assert np.array_equal(inRGB, RGB)

def test_rgba(type):

    # Construct an RGB channel
    
    height = 6
    width = 5
    nrgba = 4
    size = width * height * nrgba
    RGBA = np.array([i for i in range(0,size)], dtype=type).reshape((height, width, nrgba))
    channels = { "RGBA" : OpenEXR.Channel(RGBA) }

    header = {}
    outfile = OpenEXR.File(header, channels)

    outfile.write("out.exr")

    # 
    # Read as separate channels
    #
    
    infile = OpenEXR.File("out.exr")

    R = infile.channels()["R"].pixels
    G = infile.channels()["G"].pixels
    B = infile.channels()["B"].pixels
    A = infile.channels()["A"].pixels

    shape = R.shape
    width = shape[1]
    height = shape[0]
    for y in range(0,height):
        for x in range(0,width):
            r = R[y][x]
            g = G[y][x]
            b = B[y][x]
            a = A[y][x]
            assert r == RGBA[y][x][0]
            assert g == RGBA[y][x][1]
            assert b == RGBA[y][x][2]
            assert a == RGBA[y][x][3]
            
    #
    # Read as RGBA channel
    #
    
    infile = OpenEXR.File("out.exr", rgba=True)

    inRGBA = infile.channels()["RGBA"].pixels
    shape = inRGBA.shape
    width = shape[1]
    height = shape[0]
    assert shape[2] == 4
    
    assert np.array_equal(inRGBA, RGBA)

def test_rgba_prefix(type):

    # Construct an RGB channel
    
    height = 6
    width = 5
    nrgba = 4
    size = width * height
    RGBA = np.array([i for i in range(0,size*nrgba)], dtype=type).reshape((height, width, nrgba))
    Z = np.array([i for i in range(0,size)], dtype=type).reshape((height, width))
    channels = { "left" : OpenEXR.Channel(RGBA), "left.Z" : OpenEXR.Channel(Z) }

    header = {}
    outfile = OpenEXR.File(header, channels)

    print(f"write out.exr")
    outfile.write("out.exr")

    # 
    # Read as separate channels
    #

    print(f"read out.exr as single channels")
    infile = OpenEXR.File("out.exr")

    R = infile.channels()["left.R"].pixels
    G = infile.channels()["left.G"].pixels
    B = infile.channels()["left.B"].pixels
    A = infile.channels()["left.A"].pixels
    Z = infile.channels()["left.Z"].pixels

    shape = R.shape
    width = shape[1]
    height = shape[0]
    for y in range(0,height):
        for x in range(0,width):
            r = R[y][x]
            g = G[y][x]
            b = B[y][x]
            a = A[y][x]
            assert r == RGBA[y][x][0]
            assert g == RGBA[y][x][1]
            assert b == RGBA[y][x][2]
            assert a == RGBA[y][x][3]
            
    #
    # Read as RGBA channel
    #
    
    print(f"read out.exr as rgba channels")
    infile = OpenEXR.File("out.exr", rgba=True)

    inRGBA = infile.channels()["left"].pixels
    shape = inRGBA.shape
    width = shape[1]
    height = shape[0]
    assert shape[2] == 4
    inZ = infile.channels()["left.Z"].pixels
    
    assert np.array_equal(inRGBA, RGBA)


test_rgba_prefix('uint32')
test_rgba('uint32')
test_rgba('f')
test_rgb('uint32')
test_rgb('f')

print("OK")



