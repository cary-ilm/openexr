//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <ImfTimeCodeAttribute.h>
#include <ImfTileDescription.h>
#include <ImfRational.h>
#include <ImfKeyCode.h>
#include <ImfPreviewImage.h>

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>
#include <ImathMath.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

namespace pybind11 {
namespace detail {

    // From https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/master/src/python/py_oiio.h
    //
    // This half casting support for numpy was all derived from discussions
    // here: https://github.com/pybind/pybind11/issues/1776

    // Similar to enums in `pybind11/numpy.h`. Determined by doing:
    // python3 -c 'import numpy as np; print(np.dtype(np.float16).num)'
    constexpr int NPY_FLOAT16 = 23;

    template<> struct npy_format_descriptor<half> {
        static pybind11::dtype dtype()
        {
            handle ptr = npy_api::get().PyArray_DescrFromType_(NPY_FLOAT16);
            return reinterpret_borrow<pybind11::dtype>(ptr);
        }
        static std::string format()
        {
            // following: https://docs.python.org/3/library/struct.html#format-characters
            return "e";
        }
        static constexpr auto name = _("float16");
    };

}  // namespace detail
}  // namespace pybind11

template <class T>
bool
array_equals(const py::buffer_info& a, const py::buffer_info& b, const std::string& name);

class PyPreviewImage
{
public:
    PyPreviewImage() 
    {
    }
    
    static constexpr uint32_t style = py::array::c_style | py::array::forcecast;
    static constexpr size_t stride = sizeof(PreviewRgba);

    PyPreviewImage(unsigned int width, unsigned int height,
                   const PreviewRgba* data = nullptr)
        : pixels(py::array_t<PreviewRgba,style>(std::vector<size_t>({height, width}),
                                                std::vector<size_t>({stride*width, stride}),
                                                data))
    {
    }
    
    PyPreviewImage(const py::array_t<PreviewRgba>& p)
        : pixels(p)
    {
    }

    inline bool operator==(const PyPreviewImage& other) const;

    py::array_t<PreviewRgba> pixels;
};
    

inline std::ostream&
operator<< (std::ostream& s, const PreviewRgba& p)
{
    s << " (" << int(p.r)
      << "," << int(p.g)
      << "," << int(p.b)
      << "," << int(p.a)
      << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const PyPreviewImage& P)
{
    auto width = P.pixels.shape(1);
    auto height = P.pixels.shape(0);
    
    s << "PreviewImage(" << width << ", " << height << "," << std::endl;
    py::buffer_info buf = P.pixels.request();
    const PreviewRgba* rgba = static_cast<PreviewRgba*>(buf.ptr);
    for (ssize_t y=0; y<height; y++)
    {
        for (ssize_t x=0; x<width; x++)
            s << rgba[y*width+x];
        s << std::endl;
    }
    return s;
}
    
inline bool
PyPreviewImage::operator==(const PyPreviewImage& other) const
{
    py::buffer_info buf = pixels.request();
    py::buffer_info obuf = other.pixels.request();
    
    const PreviewRgba* apixels = static_cast<PreviewRgba*>(buf.ptr);
    const PreviewRgba* bpixels = static_cast<PreviewRgba*>(obuf.ptr);
    for (ssize_t i = 0; i < buf.size; i++)
        if (!(apixels[i] == bpixels[i]))
        {
            std::cout << "PreviewImage differs: pixel " << i << " differs: " << apixels[i] << " " << bpixels[i] << std::endl;
            return false;
        }
    return true;
}


//
// PyDouble supports the "double" attribute.
//
// When reading an attribute of type "double", a python object of type
// PyDouble is created, so that when the header is written, it will be
// of type double, since python makes no distinction between float and
// double numerical types.
//

class PyDouble
{
public:
    PyDouble(double x) : d(x)  {}

    bool operator==(const PyDouble& other) const { return d == other.d; }
    
    double d;
};
                         
class PyChromaticities
{
  public:
    PyChromaticities(float rx, float ry,
                     float gx, float gy, 
                     float bx, float by, 
                     float wx, float wy)
        : red_x(rx), red_y(ry),
          green_x(gx), green_y(gy),
          blue_x(bx), blue_y(by),
          white_x(wx), white_y(wy)
        {
        }

    bool operator==(const PyChromaticities& other) const
        {
            return (red_x == other.red_x &&
                    red_y == other.red_y &&
                    green_x == other.green_x &&
                    green_y == other.green_y &&
                    blue_x == other.blue_x &&
                    blue_y == other.blue_y &&
                    white_x == other.white_x &&
                    white_y == other.white_y);
        }
    
    float red_x;
    float red_y;
    float green_x;
    float green_y;
    float blue_x;
    float blue_y;
    float white_x;
    float white_y;
};

inline std::ostream&
operator<< (std::ostream& s, const PyChromaticities& c)
{
    s << "(" << c.red_x
      << ", " << c.red_y
      << ", " << c.green_x 
      << ", " << c.green_y
      << ", " << c.blue_x
      << ", " << c.blue_y
      << ", " << c.white_x
      << ", " << c.white_y
      << ")";
    return s;
}
    
inline std::ostream&
operator<< (std::ostream& s, const Rational& v)
{
    s << v.n << "/" << v.d;
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const KeyCode& v)
{
    s << "(" << v.filmMfcCode()
      << ", " << v.filmType()
      << ", " << v.prefix()
      << ", " << v.count()
      << ", " << v.perfOffset()
      << ", " << v.perfsPerFrame()
      << ", " << v.perfsPerCount()
      << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const TimeCode& v)
{
    s << "(" << v.hours()
      << ", " << v.minutes()
      << ", " << v.seconds()
      << ", " << v.frame()
      << ", " << v.dropFrame()
      << ", " << v.colorFrame()
      << ", " << v.fieldPhase()
      << ", " << v.bgf0()
      << ", " << v.bgf1()
      << ", " << v.bgf2()
      << ")";
    return s;
}


inline std::ostream&
operator<< (std::ostream& s, const TileDescription& v)
{
    s << "TileDescription(" << v.xSize
      << ", " << v.ySize
      << ", " << py::cast(v.mode)
      << ", " << py::cast(v.roundingMode)
      << ")";

    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const Box2i& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const Box2f& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}

