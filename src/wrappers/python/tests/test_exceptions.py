#!/usr/bin/env python3

#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

from __future__ import print_function
import sys
import os
import numpy as np

import unittest

import OpenEXR

class TestExceptions(unittest.TestCase):

    def test_File(self):
        
#        print(f"sys.argv={sys.argv} cwd: {os.getcwd()}")
#        if os.path.isdir("src/wrappers/python/tests"):
#            os.chdir("src/wrappers/python/tests")
        
        print(f"__file__={__file__}")
        test_dir = os.path.dirname(__file__)
        os.chdir(test_dir)
        
        for n,v in os.environ.items():
            print(f"os.environ[{n}]={v}")
            
        # invalid argument
        with self.assertRaises(TypeError):
            f = OpenEXR.File(1)

        # invalid number of arguments
        with self.assertRaises(TypeError):
            f = OpenEXR.File("foo", "bar")
        
        # file not found
        filename = "nonexistentfile.exr"
        with self.assertRaisesRegex(RuntimeError,
                                    f".*error.*{filename}.*No such file"):
            f = OpenEXR.File(filename)

        # file exists but is not an image
        filename = sys.argv[0]
        with self.assertRaisesRegex(RuntimeError, 
                                    f".*error.*{filename}.*File is not"):
            f = OpenEXR.File(filename)

        # Empty file object (useful it's possible to assign to it later)
        f = OpenEXR.File()
        self.assertEqual(f.filename, "")
        
        # no parts
        self.assertEqual(f.parts,[])
        with self.assertRaises(ValueError):
            f.header()
        with self.assertRaises(ValueError):
            f.channels()

        # 1-part file
        filename = "test.exr"
        f = OpenEXR.File(filename)
        self.assertEqual(f.filename, filename)

        # filename must be a string
        with self.assertRaises(TypeError):
            f.filename = 1

        # invalid part
        with self.assertRaises(ValueError):
            f.header(-1)
        with self.assertRaises(ValueError):
            f.header(1)
        with self.assertRaises(ValueError):
            f.channels(-1)
        with self.assertRaises(ValueError):
            f.channels(1)
            
        self.assertEqual(len(f.parts),1)

        # invalid list (should be list of parts)
        with self.assertRaises(ValueError):
            f = OpenEXR.File([1,2])
        with self.assertRaises(ValueError):
            f = OpenEXR.File([OpenEXR.Part(),2])

        # empty header, empty channels
        f = OpenEXR.File({}, {})
        
        # bad header dict, bad channels dict
        with self.assertRaises(ValueError):
            f = OpenEXR.File({1:1}, {})
        with self.assertRaisesRegex(ValueError, f"channels key must"):
            f = OpenEXR.File({}, {1:1}) 
        with self.assertRaisesRegex(ValueError, f"channels value must.*Channel"):
            f = OpenEXR.File({}, {"A":1}) # bad value, shou

        # bad type, compression (invalid type and invalid value)
        with self.assertRaises(TypeError):
            f = OpenEXR.File({}, {}, type=42)
        with self.assertRaises(ValueError):
            f = OpenEXR.File({}, {}, type=OpenEXR.Storage(42))
        with self.assertRaises(ValueError):
            f = OpenEXR.File({}, {}, type=OpenEXR.NUM_STORAGE_TYPES)
        with self.assertRaises(TypeError):
            f = OpenEXR.File({}, {}, compression=42)
        with self.assertRaises(ValueError):
            f = OpenEXR.File({}, {}, compression=OpenEXR.Compression(42))
        with self.assertRaises(ValueError):
            f = OpenEXR.File({}, {}, compression=OpenEXR.NUM_COMPRESSION_METHODS)

    def test_Part(self):

        with self.assertRaises(TypeError):
            p = OpenEXR.Part(1)

        # bad header dict, bad channels dict
        with self.assertRaises(ValueError):
            p = OpenEXR.Part("party", {1:1}, {})
        with self.assertRaisesRegex(ValueError, f"channels key must"):
            p = OpenEXR.Part("party", {}, {1:1}) 
        with self.assertRaisesRegex(ValueError, f"channels value must.*Channel"):
            p = OpenEXR.Part("party", {}, {"A":1}) # bad value, shou

        # bad type, compression (invalid type and invalid value)
        with self.assertRaises(TypeError):
            p = OpenEXR.Part("party", {}, {}, type=42)
        with self.assertRaises(ValueError):
            p = OpenEXR.Part("party", {}, {}, type=OpenEXR.Storage(42))
        with self.assertRaises(ValueError):
            p = OpenEXR.Part("party", {}, {}, type=OpenEXR.NUM_STORAGE_TYPES)
        with self.assertRaises(TypeError):
            p = OpenEXR.Part("party", {}, {}, compression=42)
        with self.assertRaises(ValueError):
            p = OpenEXR.Part("party", {}, {}, compression=OpenEXR.Compression(42))
        with self.assertRaises(ValueError):
            p = OpenEXR.Part("party", {}, {}, compression=OpenEXR.NUM_COMPRESSION_METHODS)

        # test default type, compression
        p = OpenEXR.Part("party", {}, {})
        self.assertEqual(p.name, "party")
        self.assertEqual(p.type, OpenEXR.scanlineimage)
        self.assertEqual(p.compression, OpenEXR.ZIP_COMPRESSION)
        self.assertEqual(p.width, 0)
        self.assertEqual(p.height, 0)
        self.assertEqual(p.header, {})
        self.assertEqual(p.channels, {})

        # test non-default type, compression
        p = OpenEXR.Part("party", {}, {}, OpenEXR.tiledimage, OpenEXR.NO_COMPRESSION)
        self.assertEqual(p.type, OpenEXR.tiledimage)
        self.assertEqual(p.compression, OpenEXR.NO_COMPRESSION)
        # test assignment of type, compression
        p.type = OpenEXR.scanlineimage
        p.compression = OpenEXR.ZIP_COMPRESSION
        self.assertEqual(p.type, OpenEXR.scanlineimage)
        self.assertEqual(p.compression, OpenEXR.ZIP_COMPRESSION)

        with self.assertRaises(AttributeError):
            p.width = 42
        with self.assertRaises(AttributeError):
            p.height = 42

    def test_Channel(self):

        OpenEXR.Channel()
        
if __name__ == '__main__':
    unittest.main()