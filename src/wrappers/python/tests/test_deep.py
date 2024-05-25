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

class TestDeep(unittest.TestCase):

    def test_read_deep(self):

#        OpenEXR.writeDeepExample()
#        OpenEXR.readDeepExample()
        f = OpenEXR.File("test.deep.exr")
        print("ok")
        
if __name__ == '__main__':
    unittest.main()
    print("OK")


