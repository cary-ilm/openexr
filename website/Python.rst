..
  SPDX-License-Identifier: BSD-3-Clause
  Copyright Contributors to the OpenEXR Project.

.. _Python:

Python Module
#############

The OpenEXR python module provides basic access to reading and writing
OpenEXR files in python.  The ``OpenEXR.File`` object serves as an
in-memory represenation of the image file contents, with a dictionary
of name/value pairs for the image metadata and a dict of name/numpy
arrays for pixel image data.

The OpenEXR module is not an image processing utility. It provides no
operations on image data other than reading and writing. For image
processing or data conversion, use another module such as ``OpenImageIO``.

A basic example of writing a file with pixel data from numpy arrays is:

    infile = OpenEXR.File("image.exr")

    width = 200
    height = 100
    R = np.array(dtype='uint32').reshape((height, width))
    G = np.array(dtype='uint32').reshape((height, width))
    B = np.array(dtype='uint32').reshape((height, width))
    A = np.array(dtype='uint32').reshape((height, width))
    for y in range(0, height):
        for x in range(0, width):
            R[y][x] = x/width
            G[y][x] = x/width
            B[y][x] = x/width
            A[y][x] = x/width
        
    channels = {
       "R" : OpenEXR.Channel(R),
       "G" : OpenEXR.Channel(G),
       "B" : OpenEXR.Channel(B),
       "A" : OpenEXR.Channel(A),
    }

    header = {
        { "compression" : OpenEXR.ZIP_COMPRESSION,
          "type" : OpenEXR.scanlineimage
        }
    }

    outfile = OpenEXR.File(header, channels)
    outfile.write("out.exr")
    
The corresponding example of reading the image is:

    infile = OpenEXR.File("out.exr")

    h = infile.header()
    print(f"type={header.type()}")
    print(f"compression={header.compression()}")

    R = infile.channels()["R"].pixels
    G = infile.channels()["G"].pixels
    B = infile.channels()["B"].pixels
    A = infile.channels()["A"].pixels
    for y in range(0, height):
        for x in range(0, width):
            print(f"pixel[{y}][{x}]=(R[y][x], G[y][x], B[y][x], A[y][x])"


The OpenEXR module supports reading and writing the full range of
OpenEXR image formats:

* scanline, tiled, and deep images
* single-part and multi-part images

Reading an Image
================

To read an image from disk, construct an ``OpenEXR.File`` object with
the filename as the argument.

Accessing Image Data
====================

The `OpenEXR.File` object as as single data ``parts``, which is a list
of ``OpenEXR.Part`` objects, each of which holds the data for a single
part of a multi-part file.

The ``Part`` object has two member fields:

* ``header`` - a dict of image metadata, where the key is the
  attribute name and the value is the attribute value.
  
* ``channels`` - a dict of pixel data, where the key is the channel
  name and the value is a `Channel` object holding the pixel data in a
  numpy array. 
  
A single-part file has a single part. For convience, the ``File``
object has a ``header()`` and ``channels()`` methods that return the
header and channels for the first part, or the only part for a
single-part file.

The ``OpenEXR.Part`` object has convenience methods for accessing data
from the header:

* ``name()`` - Return ``header['name']``, the part name, a string.
* ``type()`` - Return ``header['type']``, the storage type, i.e. scanline, tiled, deep.
* ``width()`` - Return the width of the part's pixel data
* ``height()`` - Return the height of the part's pixel data
* ``compression()`` - Return ``header['compression`]``, the part's compression method

    for part in file.parts:

       name = part.name()
       compression = part.compression()

The ``Channel`` object has a ``pixels`` member that is a numpy array
of the channel data.  The numpy array's ``dtype`` specifies the type
of pixel data. Supported types are ``uint32``, ``half``, and
``float``.

The shape of the numpy array specifies the image size. The pixel data
is stored in column-major order, so the first dimension of the array
is the y/row index, and the second dimension is the x/column index.

All channels of a given part must have the same image size.

The ``Channel`` object also holds fields for the x/y sampling rate,
``xSampling`` and ``ySampling``, as well as the ``pLinear`` setting,
used by XXX.

Writing an Image
================

To write an image, construct an ``OpenEXR.File`` object with dicts for
the header and channels, and call its ``write`` method with the output
filename.

    outfile = OpenEXR.File()

    part = OpenEXR.Part()
    part.header()["timeCode"] = OpenEXR.TimeCode()

    width = 5
    height = 10
    size = width * height
 
    A = np.array([i/size for i in range(0,size)], dtype='uint32').reshape((height, width))
    
    part.channels = {'A' : Channel(A) }
    
    outfile.parts = [ part ]

    outfile.write("out.exr")

    
