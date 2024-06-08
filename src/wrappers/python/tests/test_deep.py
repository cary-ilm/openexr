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

def print_deep(outfile):
    for n,c in outfile.channels().items():
        for y in range(c.pixels.shape[0]):
            for x in range(c.pixels.shape[1]):
                d = c.pixels[y,x]
                print(f"pixels[{y},{x}]: {d}")

def compare_files(lhs, rhs):

    for Plhs, Prhs in zip(lhs.parts,rhs.parts):
        compare_parts(Plhs, Prhs)

def compare_parts(lhs, rhs):

    if len(lhs.channels) != len(rhs.channels):
        raise Exception(f"#channels in {lhs.name} differs: {len(lhs.channels)} {len(rhs.channels)}")

    for c in lhs.channels.keys():
        compare_channels(lhs.channels[c], rhs.channels[c])

def compare_channels(lhs, rhs):

    if (lhs.name != rhs.name or
        lhs.type() != rhs.type() or
        lhs.xSampling != rhs.xSampling or
        lhs.ySampling != rhs.ySampling):
        raise Exception(f"channel {lhs.name} differs: {lhs.__repr__()} {rhs.__repr__()}")

def compare_channels(lhs, rhs):

    if lhs.pixels.shape != rhs.pixels.shape:
        raise Exception(f"channel {lhs.name}: image size differs: {lhs.pixels.shape} vs. {rhs.pixels.shape}")
        
    height = lhs.pixels.shape[0]
    width = lhs.pixels.shape[1]
    for y in range(height):
        for x in range(width):
            l = lhs.pixels[y,x]
            r = rhs.pixels[y,x]
            print(f"pixel[{y},{x}] l={l} {type(l)} r={r} {type(r)}")
            if l is None and r is None:
                print(f"> None.")
                continue
            close = np.isclose(l, r, 1e-5)
            print(f"close[{y},{x}] {close}")
            if not np.all(close):
                for i in np.argwhere(close==False):
                    y,x = i
                    if math.isfinite(lhs.pixels[y,x]) and math.isfinite(rhs.pixels[y,x]):
                        raise Exception(f"channel {lhs.name}: deep pixels {i} differ: {lhs.pixels[y,x]} {rhs.pixels[y,x]}")

class TestDeep(unittest.TestCase):

    def test_write_deep(self):

        dataWindow = ((100,100), (103,105))
        height = dataWindow[1][1] - dataWindow[0][1] + 1
        width = dataWindow[1][0] - dataWindow[0][0] + 1
        
        U = np.empty((height, width), dtype=object)
        H = np.empty((height, width), dtype=object)
        F = np.empty((height, width), dtype=object)
        for y in range(height):
            for x in range(width):
                i = y*width+x
                l = i % 3 
                if l == 0:
                    U[y, x] = np.array([i], dtype='uint32')
                    H[y, x] = np.array([i], dtype='float16')
                    F[y, x] = np.array([i], dtype='float32')
                else:
                    U[y, x] = None
                    H[y, x] = None
                    F[y, x] = None
        
        channels = { "U" : U, "H" : H, "F" : F }
        header = { "compression" : OpenEXR.ZIPS_COMPRESSION,
                   "type" : OpenEXR.deepscanline,
                   "dataWindow" : dataWindow}

        filename = "write_deep.exr"
        with OpenEXR.File(header, channels) as outfile:
            outfile.write(filename)

        with OpenEXR.File(filename) as infile:
            compare_files(infile, outfile)

if __name__ == '__main__':
    unittest.main()
    print("OK")


