//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/eval.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>

#include "openexr.h"

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>
#include <ImathMath.h>

#include <typeinfo>

#include "PyFile.h"
#include "PyPart.h"
#include "PyChannel.h"
#include "PyAttributes.h"

namespace py = pybind11;
using namespace py::literals;

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

extern bool init_OpenEXR_old(PyObject* module);

namespace {

template <class T>
std::string
repr(const T& v)
{
    std::stringstream s;
    s << v;
    return s.str();
}


} // namespace

PYBIND11_MODULE(OpenEXR, m)
{
    using namespace py::literals;

    m.doc() = "openexr doc";
    m.attr("__version__") = OPENEXR_VERSION_STRING;
    m.attr("OPENEXR_VERSION") = OPENEXR_VERSION_STRING;

    //
    // Add symbols from the legacy implementation of the bindings for
    // backwards compatibility
    //
    
    init_OpenEXR_old(m.ptr());

    //
    // Enums
    //
    
    py::enum_<LevelRoundingMode>(m, "LevelRoundingMode")
        .value("ROUND_UP", ROUND_UP)
        .value("ROUND_DOWN", ROUND_DOWN)
        .value("NUM_ROUNDINGMODES", NUM_ROUNDINGMODES)
        .export_values();

    py::enum_<LevelMode>(m, "LevelMode")
        .value("ONE_LEVEL", ONE_LEVEL)
        .value("MIPMAP_LEVELS", MIPMAP_LEVELS)
        .value("RIPMAP_LEVELS", RIPMAP_LEVELS)
        .value("NUM_LEVELMODES", NUM_LEVELMODES)
        .export_values();

    py::enum_<exr_lineorder_t>(m, "LineOrder")
        .value("INCREASING_Y", EXR_LINEORDER_INCREASING_Y)
        .value("DECREASING_Y", EXR_LINEORDER_DECREASING_Y)
        .value("RANDOM_Y", EXR_LINEORDER_RANDOM_Y)
        .value("NUM_LINEORDERS", EXR_LINEORDER_LAST_TYPE)
        .export_values();

    py::enum_<exr_pixel_type_t>(m, "PixelType")
        .value("UINT", EXR_PIXEL_UINT)
        .value("HALF", EXR_PIXEL_HALF)
        .value("FLOAT", EXR_PIXEL_FLOAT)
        .value("NUM_PIXELTYPES", EXR_PIXEL_LAST_TYPE)
        .export_values();

    py::enum_<exr_compression_t>(m, "Compression")
        .value("NO_COMPRESSION", EXR_COMPRESSION_NONE)
        .value("RLE_COMPRESSION", EXR_COMPRESSION_RLE)
        .value("ZIPS_COMPRESSION", EXR_COMPRESSION_ZIPS)
        .value("ZIP_COMPRESSION", EXR_COMPRESSION_ZIP)
        .value("PIZ_COMPRESSION", EXR_COMPRESSION_PIZ)
        .value("PXR24_COMPRESSION", EXR_COMPRESSION_PXR24)
        .value("B44_COMPRESSION", EXR_COMPRESSION_B44)
        .value("B44A_COMPRESSION", EXR_COMPRESSION_B44A)
        .value("DWAA_COMPRESSION", EXR_COMPRESSION_DWAA)
        .value("DWAB_COMPRESSION", EXR_COMPRESSION_DWAB)
        .value("NUM_COMPRESSION_METHODS", EXR_COMPRESSION_LAST_TYPE)
        .export_values();
    
    py::enum_<exr_envmap_t>(m, "EnvMap")
        .value("EXR_ENVMAP_LATLONG", EXR_ENVMAP_LATLONG)
        .value("EXR_ENVMAP_CUBE", EXR_ENVMAP_CUBE)    
        .value("EXR_ENVMAP_LAST_TYPE", EXR_ENVMAP_LAST_TYPE)
        .export_values();

    py::enum_<exr_storage_t>(m, "Storage")
        .value("scanlineimage", EXR_STORAGE_SCANLINE)
        .value("tiledimage,", EXR_STORAGE_TILED)
        .value("deepscanline,", EXR_STORAGE_DEEP_SCANLINE)
        .value("deeptile,", EXR_STORAGE_DEEP_TILED)
        .value("NUM_STORAGE_TYPES", EXR_STORAGE_LAST_TYPE)
        .export_values();

    //
    // Classes for attribute types
    //
    
    py::class_<TileDescription>(m, "TileDescription")
        .def(py::init())
        .def("__repr__", [](TileDescription& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("xSize", &TileDescription::xSize)
        .def_readwrite("ySize", &TileDescription::ySize)
        .def_readwrite("mode", &TileDescription::mode)
        .def_readwrite("roundingMode", &TileDescription::roundingMode)
        ;       

    py::class_<Rational>(m, "Rational")
        .def(py::init())
        .def(py::init<int,unsigned int>())
        .def("__repr__", [](const Rational& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("n", &Rational::n)
        .def_readwrite("d", &Rational::d)
        ;
    
    py::class_<KeyCode>(m, "KeyCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int>())
        .def(py::self == py::self)
        .def("__repr__", [](const KeyCode& v) { return repr(v); })
        .def_property("filmMfcCode", &KeyCode::filmMfcCode, &KeyCode::setFilmMfcCode)
        .def_property("filmType", &KeyCode::filmType, &KeyCode::setFilmType)
        .def_property("prefix", &KeyCode::prefix, &KeyCode::setPrefix)
        .def_property("count", &KeyCode::count, &KeyCode::setCount)
        .def_property("perfOffset", &KeyCode::perfOffset, &KeyCode::setPerfOffset)
        .def_property("perfsPerFrame", &KeyCode::perfsPerFrame, &KeyCode::setPerfsPerFrame) 
        .def_property("perfsPerCount", &KeyCode::perfsPerCount, &KeyCode::setPerfsPerCount)
        ; 

    py::class_<TimeCode>(m, "TimeCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int>())
        .def("__repr__", [](const TimeCode& v) { return repr(v); })
        .def(py::self == py::self)
        .def_property("hours", &TimeCode::hours, &TimeCode::setHours)
        .def_property("minutes", &TimeCode::minutes, &TimeCode::setMinutes)
        .def_property("seconds", &TimeCode::seconds, &TimeCode::setSeconds)
        .def_property("frame", &TimeCode::frame, &TimeCode::setFrame)
        .def_property("dropFrame", &TimeCode::dropFrame, &TimeCode::setDropFrame)
        .def_property("colorFrame", &TimeCode::colorFrame, &TimeCode::setColorFrame)
        .def_property("fieldPhase", &TimeCode::fieldPhase, &TimeCode::setFieldPhase)
        .def_property("bgf0", &TimeCode::bgf0, &TimeCode::setBgf0)
        .def_property("bgf1", &TimeCode::bgf1, &TimeCode::setBgf1)
        .def_property("bgf2", &TimeCode::bgf2, &TimeCode::setBgf2)
        .def_property("binaryGroup", &TimeCode::binaryGroup, &TimeCode::setBinaryGroup)
        .def_property("userData", &TimeCode::userData, &TimeCode::setUserData)
        .def("timeAndFlags", &TimeCode::timeAndFlags)
        .def("setTimeAndFlags", &TimeCode::setTimeAndFlags)
        ;

    py::class_<PyChromaticities>(m, "Chromaticities")
        .def(py::init<float,float,float,float,float,float,float,float>())
        .def(py::self == py::self)
        .def("__repr__", [](const PyChromaticities& v) { return repr(v); })
        .def_readwrite("red_x", &PyChromaticities::red_x)
        .def_readwrite("red_y", &PyChromaticities::red_y)
        .def_readwrite("green_x", &PyChromaticities::green_x)
        .def_readwrite("green_y", &PyChromaticities::green_y)
        .def_readwrite("blue_x", &PyChromaticities::blue_x)
        .def_readwrite("blue_y", &PyChromaticities::blue_y)
        .def_readwrite("white_x", &PyChromaticities::white_x)
        .def_readwrite("white_y", &PyChromaticities::white_y)
        ;

    py::class_<PreviewRgba>(m, "PreviewRgba")
        .def(py::init())
        .def(py::init<unsigned char,unsigned char,unsigned char,unsigned char>())
        .def(py::self == py::self)
        .def_readwrite("r", &PreviewRgba::r)
        .def_readwrite("g", &PreviewRgba::g)
        .def_readwrite("b", &PreviewRgba::b)
        .def_readwrite("a", &PreviewRgba::a)
        ;
    
    PYBIND11_NUMPY_DTYPE(PreviewRgba, r, g, b, a);
    
    py::class_<PyPreviewImage>(m, "PreviewImage")
        .def(py::init())
        .def(py::init<int,int>())
        .def(py::init<py::array_t<PreviewRgba>>())
        .def("__repr__", [](const PyPreviewImage& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("pixels", &PyPreviewImage::pixels)
        ;
    
    py::class_<PyDouble>(m, "Double")
        .def(py::init<double>())
        .def("__repr__", [](const PyDouble& d) { return repr(d.d); })
        .def(py::self == py::self)
        ;

    //
    // Stand-in Imath classes - these should really come from the Imath module.
    //
    
    py::class_<V2i>(m, "V2i")
        .def(py::init())
        .def(py::init<int,int>())
        .def("__repr__", [](const V2i& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V2i::x)
        .def_readwrite("y", &Imath::V2i::y)
        ;

    py::class_<V2f>(m, "V2f")
        .def(py::init())
        .def(py::init<float,float>())
        .def("__repr__", [](const V2f& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V2f::x)
        .def_readwrite("y", &Imath::V2f::y)
        ;

    py::class_<V2d>(m, "V2d")
        .def(py::init())
        .def(py::init<double,double>())
        .def("__repr__", [](const V2d& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V2d::x)
        .def_readwrite("y", &Imath::V2d::y)
        ;

    py::class_<V3i>(m, "V3i")
        .def(py::init())
        .def(py::init<int,int,int>())
        .def("__repr__", [](const V3i& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V3i::x)
        .def_readwrite("y", &Imath::V3i::y)
        .def_readwrite("z", &Imath::V3i::z)
        ;

    py::class_<V3f>(m, "V3f")
        .def(py::init())
        .def(py::init<float,float,float>())
        .def("__repr__", [](const V3f& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V3f::x)
        .def_readwrite("y", &Imath::V3f::y)
        .def_readwrite("z", &Imath::V3f::z)
        ;

    py::class_<V3d>(m, "V3d")
        .def(py::init())
        .def(py::init<double,double,double>())
        .def("__repr__", [](const V3d& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("x", &Imath::V3d::x)
        .def_readwrite("y", &Imath::V3d::y)
        .def_readwrite("z", &Imath::V3d::z)
        ;

    py::class_<Box2i>(m, "Box2i")
        .def(py::init())
        .def(py::init<V2i,V2i>())
        .def("__repr__", [](const Box2i& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("min", &Box2i::min)
        .def_readwrite("max", &Box2i::max)
        ;
    
    py::class_<Box2f>(m, "Box2f")
        .def(py::init())
        .def(py::init<V2f,V2f>())
        .def("__repr__", [](const Box2f& v) { return repr(v); })
        .def(py::self == py::self)
        .def_readwrite("min", &Box2f::min)
        .def_readwrite("max", &Box2f::max)
        ;
    
    py::class_<M33f>(m, "M33f")
        .def(py::init())
        .def(py::init<float,float,float,float,float,float,float,float,float>())
        .def("__repr__", [](const M33f& m) { return repr(m); })
        .def(py::self == py::self)
        ;
    
    py::class_<M33d>(m, "M33d")
        .def(py::init())
        .def(py::init<double,double,double,double,double,double,double,double,double>())
        .def("__repr__", [](const M33d& m) { return repr(m); })
        .def(py::self == py::self)
        ;
    
    py::class_<M44f>(m, "M44f")
        .def(py::init<float,float,float,float,
                      float,float,float,float,
                      float,float,float,float,
                      float,float,float,float>())
        .def(py::self == py::self)
        .def("__repr__", [](const M44f& m) { return repr(m); })
        ;
    
    py::class_<M44d>(m, "M44d")
        .def(py::init<double,double,double,double,
                      double,double,double,double,
                      double,double,double,double,
                      double,double,double,double>())
        .def("__repr__", [](const M44d& m) { return repr(m); })
        .def(py::self == py::self)
        ;
    
    //
    // The File API: Channel, Part, and File
    //
    
    py::class_<PyChannel>(m, "Channel")
        .def(py::init())
        .def(py::init<const char*,exr_pixel_type_t,int,int>())
        .def(py::init<const char*,py::array>())
        .def(py::init<const char*,py::array,int,int>())
        .def("__repr__", [](const PyChannel& c) { return repr(c); })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("name", &PyChannel::name)
        .def("type", &PyChannel::pixelType) 
        .def_readwrite("xSampling", &PyChannel::xSampling)
        .def_readwrite("ySampling", &PyChannel::ySampling)
        .def_readwrite("pixels", &PyChannel::pixels)
        ;
    
    py::class_<PyPart>(m, "Part")
        .def(py::init())
        .def(py::init<const char*,py::dict,py::dict,exr_storage_t,exr_compression_t>())
        .def("__repr__", [](const PyPart& p) { return repr(p); })
        .def(py::self == py::self)
        .def_readwrite("name", &PyPart::name)
        .def_readwrite("type", &PyPart::type)
        .def_readwrite("width", &PyPart::width)
        .def_readwrite("height", &PyPart::height)
        .def_readwrite("compression", &PyPart::compression)
        .def_readwrite("header", &PyPart::header)
        .def_readwrite("channels", &PyPart::channels)
        ;

    py::class_<PyFile>(m, "File")
        .def(py::init<std::string>())
        .def(py::init<py::dict,py::dict,exr_storage_t,exr_compression_t>())
        .def(py::init<py::list>())
        .def(py::self == py::self)
        .def_readwrite("filename", &PyFile::filename)
        .def_readwrite("parts", &PyFile::parts)
        .def("header", &PyFile::header)
        .def("channels", &PyFile::channels)
        .def("write", &PyFile::write)
        ;
}

