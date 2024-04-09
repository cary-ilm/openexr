#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

import OpenEXR, Imath
from array import array

width = 10
height = 10
size = width * height

FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)
h = OpenEXR.Header(width,height)
h['channels'] = {'R' : Imath.Channel(FLOAT),
                 'G' : Imath.Channel(FLOAT),
                 'B' : Imath.Channel(FLOAT),
                 'A' : Imath.Channel(FLOAT)} 
o = OpenEXR.OutputFile("hello.exr", h)
r = array('f', [n for n in range(size*0,size*1)]).tobytes()
g = array('f', [n for n in range(size*1,size*2)]).tobytes()
b = array('f', [n for n in range(size*2,size*3)]).tobytes()
a = array('f', [n for n in range(size*3,size*4)]).tobytes()
channels = {'R' : r, 'G' : g, 'B' : b, 'A' : a}
o.writePixels(channels)
o.close()
