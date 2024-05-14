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


#infile = OpenEXR.File("/home/cary/src/cary-ilm/openexr-images/MultiView/Balls.exr")
infile = OpenEXR.RgbaFile("/home/cary/src/cary-ilm/openexr-images/MultiView/Balls.exr")
for n,c in infile.channels().items():
    print(f"channel {n} ndim={c.pixels.ndim}")
