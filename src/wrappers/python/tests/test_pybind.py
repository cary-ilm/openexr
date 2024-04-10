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

filename = "test.exr"
filename = "multipart.exr"
i = OpenEXR.File(filename)
print(f"numparts: {i.numparts()}")
print(f"parts:")
parts = i.parts()
for p in parts:
    print(f"  part: {p}")
    h = p.attributes()
    for a in h:
        print(f"    {a} {h[a]}")

    
print(f"attributes:")
h = i.attributes()
for a in h:
    print(f"  {a} {h[a]}")

print("ok")
