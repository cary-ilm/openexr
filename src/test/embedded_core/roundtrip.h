//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

// Helpers shared by the app and the plugin. Each compiles its own
// copy against its own OpenEXR installation, so these functions
// exercise whichever OpenEXR the including module is linked against.

#pragma once

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfRgbaFile.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <typeinfo>

namespace
{

const int width  = 64;
const int height = 32;

half
pixelValue (int x, int y, int seed)
{
    return half (float ((x * 7 + y * 13 + seed) % 256) / 255.0f);
}

void
writeImage (const char* filename, int seed)
{
    Imf::Array2D<Imf::Rgba> pixels (height, width);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            half v = pixelValue (x, y, seed);
            pixels[y][x] = Imf::Rgba (v, v, v, half (1.0f));
        }

    Imf::RgbaOutputFile file (
        filename, width, height, Imf::WRITE_RGBA, 1, IMATH_NAMESPACE::V2f (0, 0),
        1, Imf::INCREASING_Y, Imf::ZIP_COMPRESSION);
    file.setFrameBuffer (&pixels[0][0], 1, width);
    file.writePixels (height);
}

bool
checkImage (const char* filename, int seed)
{
    Imf::RgbaInputFile file (filename);
    IMATH_NAMESPACE::Box2i dw = file.dataWindow ();
    if (dw.max.x - dw.min.x + 1 != width || dw.max.y - dw.min.y + 1 != height)
    {
        std::cerr << filename << ": unexpected data window" << std::endl;
        return false;
    }

    Imf::Array2D<Imf::Rgba> pixels (height, width);
    file.setFrameBuffer (&pixels[0][0], 1, width);
    file.readPixels (dw.min.y, dw.max.y);

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (pixels[y][x].r != pixelValue (x, y, seed))
            {
                std::cerr << filename << ": unexpected pixel value at " << x
                          << "," << y << std::endl;
                return false;
            }
    return true;
}

// The mangled name of Imf::Header, which encodes the real (internal)
// namespace of the OpenEXR library this module was compiled against.
std::string
headerTypeName ()
{
    return typeid (Imf::Header).name ();
}

} // namespace
