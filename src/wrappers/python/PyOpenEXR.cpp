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

#include <typeinfo>
#include <sys/types.h>

#include "openexr.h"

#include <ImfTileDescription.h>
#include <ImfRational.h>
#include <ImfKeyCode.h>
#include <ImfPreviewImage.h>

#include <ImfRgbaFile.h>
#include <ImfTiledRgbaFile.h>
#include <ImfMultiPartInputFile.h>
#include <ImfMultiPartOutputFile.h>
#include <ImfInputPart.h>
#include <ImfOutputPart.h>
#include <ImfTiledOutputPart.h>
#include <ImfDeepScanLineOutputPart.h>
#include <ImfDeepTiledOutputPart.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfChannelList.h>
#include <ImfArray.h>
#include <ImfPartType.h>

#include <ImfBoxAttribute.h>
#include <ImfChannelListAttribute.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfHeader.h>
#include <ImfIntAttribute.h>
#include <ImfKeyCodeAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfPreviewImageAttribute.h>
#include <ImfRationalAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfStringVectorAttribute.h>
#include <ImfFloatVectorAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfVecAttribute.h>

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>
#include <ImathMath.h>

namespace py = pybind11;
using namespace py::literals;

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

extern bool init_OpenEXR_old(PyObject* module);

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

namespace {

#include "PyOpenEXR.h"
    
//
// Create a PyFile out of a list of parts (i.e. a multi-part file)
//

PyFile::PyFile(const py::list& p)
    : parts(p)
{
    for (size_t part_index = 0; part_index < parts.size(); part_index++)
    {
        auto p = parts[part_index];
        if (!py::isinstance<PyPart>(p))
            throw std::invalid_argument("must be a list of OpenEXR.Part() objects");

        auto P = p.cast<PyPart&>();
        P.part_index = part_index;
    }
}

//
// Create a PyFile out of a single part: header, channels,
// type, and compression (i.e. a single-part file)
//

PyFile::PyFile(const py::dict& header, const py::dict& channels, exr_storage_t type, Compression compression)
{
    parts.append(py::cast<PyPart>(PyPart(header, channels, type, compression, "")));
}

//
// Read a PyFile from the given filename
//


PyFile::PyFile(const std::string& filename)
    : filename (filename)
{
    MultiPartInputFile infile(filename.c_str());

    for (int part_index = 0; part_index < infile.parts(); part_index++)
    {
        const Header& header = infile.header(part_index);

        PyPart P;

        P.part_index = part_index;
        
        const Box2i& dw = header.dataWindow();
        auto width = static_cast<size_t>(dw.max.x - dw.min.x + 1);
        auto height = static_cast<size_t>(dw.max.y - dw.min.y + 1);

        for (auto a = header.begin(); a != header.end(); a++)
        {
            std::string name = a.name();
            const Attribute& attribute = a.attribute();
            P.header[py::str(name)] = get_attribute_object(name, &attribute);
        }

        std::vector<size_t> shape ({height, width});

        FrameBuffer frameBuffer;

        for (auto c = header.channels().begin(); c != header.channels().end(); c++)
        {
            PyChannel C;
            
            C.name = c.name();
            C.xSampling = c.channel().xSampling;
            C.ySampling = c.channel().ySampling;
            C.pLinear = c.channel().pLinear;
                
            const auto style = py::array::c_style | py::array::forcecast;

            switch (c.channel().type)
            {
            case UINT:
                C.pixels = py::array_t<uint32_t,style>(shape);
                break;
            case HALF:
                C.pixels = py::array_t<half,style>(shape);
                break;
            case FLOAT:
                C.pixels = py::array_t<float,style>(shape);
                break;
            default:
                throw std::runtime_error("invalid pixel type");
            } // switch c->type

            py::buffer_info buf = C.pixels.request();

            frameBuffer.insert (C.name,
                                Slice::Make (c.channel().type,
                                             (void*) buf.ptr,
                                             dw, 0, 0,
                                             C.xSampling,
                                             C.ySampling));
            P.channels[py::str(c.name())] = C;
        } // for header.channels()

        InputPart part (infile, part_index);

        part.setFrameBuffer (frameBuffer);
        part.readPixels (dw.min.y, dw.max.y);

        parts.append(py::cast<PyPart>(PyPart(P)));
    } // for parts
}

bool
PyFile::operator==(const PyFile& other) const
{
    if (parts.size() != other.parts.size())
    {
        std::cout << "PyFile:: #parts differs." << std::endl;
        return false;
    }
    
    for (size_t part_index = 0; part_index<parts.size(); part_index++)
    {
        auto a = parts[part_index].cast<const PyPart&>();
        auto b = other.parts[part_index].cast<const PyPart&>();
        if (a != b)
        {
            std::cout << "PyFile: part " << part_index << " differs." << std::endl;
            return false;
        }
    }
    
    return true;
}       

void
validate_part_index(int part_index, size_t num_parts)
{
    if (0 < part_index)
    {
        std::stringstream s;
        s << "Invalid part index '" << part_index << "'";
        throw std::invalid_argument(s.str());
    }
    
    if (static_cast<size_t>(part_index) >= num_parts)
    {
        std::stringstream s;
        s << "Invalid part index '" << part_index
          << "': file has " << num_parts
          << " part";
        if (num_parts != 1)
            s << "s";
        s << ".";
        throw std::invalid_argument(s.str());
    }
}
    
py::dict&
PyFile::header(int part_index)
{
    validate_part_index(part_index, parts.size());
    return parts[part_index].cast<PyPart&>().header;
}

py::dict&
PyFile::channels(int part_index)
{
    validate_part_index(part_index, parts.size());
    return parts[part_index].cast<PyPart&>().channels;
}

//
// Write the PyFile to the given filename
//

void
PyFile::write(const char* outfilename)
{
    std::vector<Header> headers;

    for (size_t part_index = 0; part_index < parts.size(); part_index++)
    {
        const PyPart& P = parts[part_index].cast<const PyPart&>();
        
        Header header;

        header.setName (P.name());

        for (auto a : P.header)
        {
            auto name = py::str(a.first);
            py::object second = py::cast<py::object>(a.second);
            insert_attribute(header, name, second);
        }
        
        if (!P.header.contains("dataWindow"))
        {
            auto shape = P.shape();
            header.dataWindow().max = V2i(shape[1]-1,shape[0]-1);
        }
        
        for (auto c : P.channels)
        {
            auto C = py::cast<PyChannel&>(c.second);
            auto t = static_cast<PixelType>(C.pixelType());
            header.channels ().insert(C.name, Channel (t, C.xSampling, C.ySampling, C.pLinear));
        }

        header.setType(P.typeString());

        if (P.header.contains("tiles"))
        {
            auto td = P.header["tiles"].cast<const TileDescription&>();
            header.setTileDescription (td);
        }

        if (P.header.contains("lineOrder"))
        {
            auto lo = P.header["lineOrder"].cast<LineOrder&>();
            header.lineOrder() = static_cast<LineOrder>(lo);
        }

        header.compression() = P.compression();

        headers.push_back (header);
    }

    MultiPartOutputFile outfile(outfilename, headers.data(), headers.size());

    for (size_t part_index = 0; part_index < parts.size(); part_index++)
    {
        const PyPart& P = parts[part_index].cast<const PyPart&>();

        auto header = headers[part_index];
        const Box2i& dw = header.dataWindow();

        if (P.type() == EXR_STORAGE_SCANLINE ||
            P.type() == EXR_STORAGE_TILED)
        {
            FrameBuffer frameBuffer;
        
            for (auto c : P.channels)
            {
                auto C = c.second.cast<const PyChannel&>();
                frameBuffer.insert (C.name,
                                    Slice::Make (static_cast<PixelType>(C.pixelType()),
                                                 static_cast<void*>(C.pixels.request().ptr),
                                                 dw, 0, 0,
                                                 C.xSampling,
                                                 C.ySampling));
            }
                
            if (P.type() == EXR_STORAGE_SCANLINE)
            {
                OutputPart part(outfile, part_index);
                part.setFrameBuffer (frameBuffer);
                part.writePixels (P.height());
            }
            else
            {
                TiledOutputPart part(outfile, part_index);
                part.setFrameBuffer (frameBuffer);
                part.writeTiles (0, part.numXTiles() - 1, 0, part.numYTiles() - 1);
            }
        }
        else if (P.type() == EXR_STORAGE_DEEP_SCANLINE ||
                 P.type() == EXR_STORAGE_DEEP_TILED)
        {
            DeepFrameBuffer frameBuffer;
        
            for (auto c : P.channels)
            {
                auto C = c.second.cast<const PyChannel&>();
                frameBuffer.insert (C.name,
                                    DeepSlice (static_cast<PixelType>(C.pixelType()),
                                               static_cast<char*>(C.pixels.request().ptr),
                                               0, 0, 0,
                                               C.xSampling,
                                               C.ySampling));
            }
        
            if (P.type() == EXR_STORAGE_DEEP_SCANLINE)
            {
                DeepScanLineOutputPart part(outfile, part_index);
                part.setFrameBuffer (frameBuffer);
                part.writePixels (P.height());
            }
            else 
            {
                DeepTiledOutputPart part(outfile, part_index);
                part.setFrameBuffer (frameBuffer);
                part.writeTiles (0, part.numXTiles() - 1, 0, part.numYTiles() - 1);
            }
        }
        else
            throw std::runtime_error("invalid type");
    }

    filename = outfilename;
}

//
// Helper routine to cast an objec to a type only if it's actually that type,
// since py::cast throws an runtime_error on unexpected type.
//

template <class T>
const T*
py_cast(const py::object& object)
{
    if (py::isinstance<T>(object))
        return py::cast<T*>(object);

    return nullptr;
}

//
// Helper routine to cast an objec to a type only if it's actually that type,
// since py::cast throws an runtime_error on unexpected type. This further cast
// the resulting pointer to a second type.
//

template <class T, class S>
const T*
py_cast(const py::object& object)
{
    if (py::isinstance<S>(object))
    {
        auto o = py::cast<S*>(object);
        return reinterpret_cast<const T*>(o);
    }

    return nullptr;
}

py::object
PyFile::get_attribute_object(const std::string& name, const Attribute* a)
{
    if (auto v = dynamic_cast<const Box2iAttribute*> (a))
        return py::cast(Box2i(v->value()));

    if (auto v = dynamic_cast<const Box2fAttribute*> (a))
        return py::cast(Box2f(v->value()));

    if (auto v = dynamic_cast<const ChannelListAttribute*> (a))
    {
        auto L = v->value();
        auto l = py::list();
        for (auto c = L.begin (); c != L.end (); ++c)
        {
            auto C = c.channel();
            l.append(py::cast(PyChannel(c.name(),
                                        C.xSampling,
                                        C.ySampling,
                                        C.pLinear)));
        }
        return l;
    }
    
    if (auto v = dynamic_cast<const ChromaticitiesAttribute*> (a))
    {
        PyChromaticities c(v->value().red.x,
                           v->value().red.y,
                           v->value().green.x,
                           v->value().green.y,
                           v->value().blue.x,
                           v->value().blue.y,
                           v->value().white.x,
                           v->value().white.y);
        return py::cast(c);
    }

    if (auto v = dynamic_cast<const CompressionAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const DoubleAttribute*> (a))
        return py::cast(PyDouble(v->value()));

    if (auto v = dynamic_cast<const EnvmapAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const FloatAttribute*> (a))
        return py::float_(v->value());

    if (auto v = dynamic_cast<const IntAttribute*> (a))
        return py::int_(v->value());

    if (auto v = dynamic_cast<const KeyCodeAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const LineOrderAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const M33fAttribute*> (a))
        return py::cast(M33f(v->value()[0][0],
                             v->value()[0][1],
                             v->value()[0][2],
                             v->value()[1][0],
                             v->value()[1][1],
                             v->value()[1][2],
                             v->value()[2][0],
                             v->value()[2][1],
                             v->value()[2][2]));

    if (auto v = dynamic_cast<const M33dAttribute*> (a))
        return py::cast(M33d(v->value()[0][0],
                             v->value()[0][1],
                             v->value()[0][2],
                             v->value()[1][0],
                             v->value()[1][1],
                             v->value()[1][2],
                             v->value()[2][0],
                             v->value()[2][1],
                             v->value()[2][2]));

    if (auto v = dynamic_cast<const M44fAttribute*> (a))
        return py::cast(M44f(v->value()[0][0],
                             v->value()[0][1],
                             v->value()[0][2],
                             v->value()[0][3],
                             v->value()[1][0],
                             v->value()[1][1],
                             v->value()[1][2],
                             v->value()[1][3],
                             v->value()[2][0],
                             v->value()[2][1],
                             v->value()[2][2],
                             v->value()[2][3],
                             v->value()[3][0],
                             v->value()[3][1],
                             v->value()[3][2],
                             v->value()[3][3]));

    if (auto v = dynamic_cast<const M44dAttribute*> (a))
        return py::cast(M44d(v->value()[0][0],
                             v->value()[0][1],
                             v->value()[0][2],
                             v->value()[0][3],
                             v->value()[1][0],
                             v->value()[1][1],
                             v->value()[1][2],
                             v->value()[1][3],
                             v->value()[2][0],
                             v->value()[2][1],
                             v->value()[2][2],
                             v->value()[2][3],
                             v->value()[3][0],
                             v->value()[3][1],
                             v->value()[3][2],
                             v->value()[3][3]));

    if (auto v = dynamic_cast<const PreviewImageAttribute*> (a))
    {
        auto I = v->value();
        return py::cast(PyPreviewImage(I.width(), I.height(), I.pixels()));
    }

    if (auto v = dynamic_cast<const StringAttribute*> (a))
    {
        if (name == "type")
        {
            //
            // The "type" attribute comes through as a string,
            // but we want it to be the OpenEXR.Storage enum.
            //
                  
            exr_storage_t t = EXR_STORAGE_LAST_TYPE;
            if (v->value() == SCANLINEIMAGE) // "scanlineimage")
                t = EXR_STORAGE_SCANLINE;
            else if (v->value() == TILEDIMAGE) // "tiledimage")
                t = EXR_STORAGE_TILED;
            else if (v->value() == DEEPSCANLINE) // "deepscanline")
                t = EXR_STORAGE_DEEP_SCANLINE;
            else if (v->value() == DEEPTILE) // "deeptile") 
                t = EXR_STORAGE_DEEP_TILED;
            else
                throw std::invalid_argument("unrecognized image 'type' attribute");
            return py::cast(t);
        }
        return py::str(v->value());
    }

    if (auto v = dynamic_cast<const StringVectorAttribute*> (a))
    {
        auto l = py::list();
        for (auto i = v->value().begin (); i != v->value().end(); i++)
            l.append(py::str(*i));
        return l;
    }

    if (auto v = dynamic_cast<const FloatVectorAttribute*> (a))
    {
        auto l = py::list();
        for (auto i = v->value().begin(); i != v->value().end(); i++)
            l.append(py::float_(*i));
        return l;
    }

    if (auto v = dynamic_cast<const RationalAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const TileDescriptionAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const TimeCodeAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const V2iAttribute*> (a))
        return py::cast(V2i(v->value().x, v->value().y));

    if (auto v = dynamic_cast<const V2fAttribute*> (a))
        return py::cast(V2f(v->value().x, v->value().y));

    if (auto v = dynamic_cast<const V2dAttribute*> (a))
        return py::cast(V2d(v->value().x, v->value().y));

    if (auto v = dynamic_cast<const V3iAttribute*> (a))
        return py::cast(V3i(v->value().x, v->value().y, v->value().z));
    
    if (auto v = dynamic_cast<const V3fAttribute*> (a))
        return py::cast(V3f(v->value().x, v->value().y, v->value().z));
    
    if (auto v = dynamic_cast<const V3dAttribute*> (a))
        return py::cast(V3d(v->value().x, v->value().y, v->value().z));
    
    throw std::runtime_error("unrecognized attribute type");
    
    return py::none();
}
    
void
PyFile::insert_attribute(Header& header, const std::string& name, const py::object& object)
{
    if (auto v = py_cast<Box2i>(object))
        header.insert(name, Box2iAttribute(*v));
    else if (auto v = py_cast<Box2f>(object))
        header.insert(name, Box2fAttribute(*v));
    else if (py::isinstance<py::list>(object))
    {
        auto list = py::cast<py::list>(object);
        auto size = list.size();
        if (size == 0)
            throw std::runtime_error("invalid empty list is header: can't deduce attribute type");

        if (py::isinstance<py::float_>(list[0]))
        {
            // float vector
            std::vector<float> v = list.cast<std::vector<float>>();
            header.insert(name, FloatVectorAttribute(v));
        }
        else if (py::isinstance<py::str>(list[0]))
        {
            // string vector
            std::vector<std::string> v = list.cast<std::vector<std::string>>();
            header.insert(name, StringVectorAttribute(v));
        }
        else if (py::isinstance<PyChannel>(list[0]))
        {
            //
            // Channel list: don't create an explicit chlist attribute here,
            // since the channels get created elswhere.
        }
    }
    else if (auto v = py_cast<PyChromaticities>(object))
    {
        Chromaticities c(V2f(v->red_x, v->red_y),
                         V2f(v->green_x, v->green_y),
                         V2f(v->blue_x, v->blue_y),
                         V2f(v->white_x, v->white_y));
        header.insert(name, ChromaticitiesAttribute(c));
    }
    else if (auto v = py_cast<Compression>(object))
        header.insert(name, CompressionAttribute(static_cast<Compression>(*v)));
    else if (auto v = py_cast<Envmap>(object))
        header.insert(name, EnvmapAttribute(static_cast<Envmap>(*v)));
    else if (py::isinstance<py::float_>(object))
        header.insert(name, FloatAttribute(py::cast<py::float_>(object)));
    else if (py::isinstance<PyDouble>(object))
        header.insert(name, DoubleAttribute(py::cast<PyDouble>(object).d));
    else if (py::isinstance<py::int_>(object))
        header.insert(name, IntAttribute(py::cast<py::int_>(object)));
    else if (auto v = py_cast<KeyCode>(object))
        header.insert(name, KeyCodeAttribute(*v));
    else if (auto v = py_cast<LineOrder>(object))
        header.insert(name, LineOrderAttribute(static_cast<LineOrder>(*v)));
    else if (auto v = py_cast<M33f>(object))
        header.insert(name, M33fAttribute(*v));
    else if (auto v = py_cast<M33d>(object))
        header.insert(name, M33dAttribute(*v));
    else if (auto v = py_cast<M44f>(object))
        header.insert(name, M44fAttribute(*v));
    else if (auto v = py_cast<M44d>(object))
        header.insert(name, M44dAttribute(*v));
    else if (auto v = py_cast<PyPreviewImage>(object))
    {
        py::buffer_info buf = v->pixels.request();
        auto pixels = static_cast<PreviewRgba*>(buf.ptr);
        auto height = v->pixels.shape(0);
        auto width = v->pixels.shape(1);
        PreviewImage p(width, height, pixels);
        header.insert(name, PreviewImageAttribute(p));
    }
    else if (auto v = py_cast<Rational>(object))
        header.insert(name, RationalAttribute(*v));
    else if (auto v = py_cast<TileDescription>(object))
        header.insert(name, TileDescriptionAttribute(*v));
    else if (auto v = py_cast<TimeCode>(object))
        header.insert(name, TimeCodeAttribute(*v));
    else if (auto v = py_cast<V2i>(object))
        header.insert(name, V2iAttribute(*v));
    else if (auto v = py_cast<V2f>(object))
        header.insert(name, V2fAttribute(*v));
    else if (auto v = py_cast<V2d>(object))
        header.insert(name, V2dAttribute(*v));
    else if (auto v = py_cast<V3i>(object))
        header.insert(name, V3iAttribute(*v));
    else if (auto v = py_cast<V3f>(object))
        header.insert(name, V3fAttribute(*v));
    else if (auto v = py_cast<V3d>(object))
        header.insert(name, V3dAttribute(*v));
    else if (auto v = py_cast<exr_storage_t>(object))
    {
        std::string type;
        switch (*v)
        {
        case EXR_STORAGE_SCANLINE:
            type = SCANLINEIMAGE;
            break;
        case EXR_STORAGE_TILED:
            type = TILEDIMAGE;
            break;
        case EXR_STORAGE_DEEP_SCANLINE:
            type = DEEPSCANLINE;
            break;
        case EXR_STORAGE_DEEP_TILED:
            type = DEEPTILE;
            break;
        case EXR_STORAGE_LAST_TYPE:
        default:
            throw std::runtime_error("unknown storage type");
            break;
        }
        header.setType(type);
    }
    else if (py::isinstance<py::str>(object))
        header.insert(name, StringAttribute(py::str(object)));
    else
    {
        std::stringstream s;
        s << "unknown attribute type: " << py::str(object);
        throw std::runtime_error(s.str());
    }
}

//
// Construct a part from explicit header and channel data.
// 
// Used to construct a file for writing.
//

PyPart::PyPart(const py::dict& header, const py::dict& channels,
               exr_storage_t type, Compression compression, const std::string& name)
    : header(header), channels(channels), part_index(0)
{
    if (name != "")
        header[py::str("name")] = py::str(name);
    
    if (type >= EXR_STORAGE_LAST_TYPE)
        throw std::invalid_argument("invalid storage type");
    header[py::str("type")] = py::cast(type);
    
    if (compression >= NUM_COMPRESSION_METHODS)
        throw std::invalid_argument("invalid compression type");
    header[py::str("compression")] = py::cast(compression);
    
    for (auto a : header)
    {
        if (!py::isinstance<py::str>(a.first))
            throw std::invalid_argument("header key must be string (attribute name)");
        
        // TODO: confirm it's a valid attribute value
        py::object second = py::cast<py::object>(a.second);
    }
    
    //
    // Validate that all channel dict keys are strings, and initialze the
    // channel name field.
    //
    
    for (auto c : channels)
    {
        if (!py::isinstance<py::str>(c.first))
            throw std::invalid_argument("channels key must be string (channel name)");

        c.second.cast<PyChannel&>().name = py::str(c.first);
    }

    auto s = shape();

    if (!header.contains("dataWindow"))
        header["dataWindow"] = py::cast(Box2i(V2i(0,0), V2i(s[1]-1,s[0]-1)));

    if (!header.contains("displayWindow"))
        header["displayWindow"] = py::cast(Box2i(V2i(0,0), V2i(s[1]-1,s[0]-1)));
}

bool
is_required_attribute(const std::string& name)
{
    return (name == "channels" ||
            name == "compression" ||
            name == "dataWindow" ||
            name == "displayWindow" ||
            name == "lineOrder" || 
            name == "pixelAspectRatio" ||
            name == "screenWindowCenter" ||
            name == "screenWindowWidth" ||
            name == "tiles" ||
            name == "type" ||
            name == "name" ||
            name == "version" ||
            name == "chunkCount");
}
            
bool
equal_header(const py::dict& A, const py::dict& B)
{
    std::set<std::string> names;
    
    for (auto a : A)
        names.insert(py::str(a.first));
    for (auto b : B)
        names.insert(py::str(b.first));
    
    for (auto name : names)
    {
        if (name == "channels")
            continue;
                
        if (!A.contains(name))
        {
            if (is_required_attribute(name))
                continue;
            return false;
        }

        if (!B.contains(name))
        {
            if (is_required_attribute(name))
                continue;
            return false;
        }
            
        py::object a = A[py::str(name)];
        py::object b = B[py::str(name)];
        if (!a.equal(b))
        {
            if (py::isinstance<py::float_>(a))
            {                
                float f = py::cast<py::float_>(a);
                float of = py::cast<py::float_>(b);
                if (f == of)
                    return true;
                
                if (equalWithRelError(f, of, 1e-8f))
                {
                    float df = f - of;
                    std::cout << "float values are very close: "
                              << std::scientific << std::setprecision(12)
                              << f << " "
                              << of << " ("
                              << df << ")"
                              << std::endl;
                    return true;
                }
            }
            return false;
        }
    }

    return true;
}
    

bool
PyPart::operator==(const PyPart& other) const
{
    if (!equal_header(header, other.header))
    {
        std::cout << "PyPart: !equal_header" << std::endl;
        return false;
    }
        
    //
    // The channel dicts might not be in alphabetical order
    // (they're sorted on write), so don't just compare the dicts
    // directly, compare each entry by key/name.
    //
    
    if (channels.size() != other.channels.size())
    {
        std::cout << "PyPart: #channels differs." << std::endl;
        return false;
    }
        
    for (auto c : channels)
    {
        auto name = py::str(c.first);
        auto C = c.second.cast<const PyChannel&>();
        auto O = other.channels[py::str(name)].cast<const PyChannel&>();
        if (C != O)
        {
            std::cout << "channel " << name << " differs." << std::endl;
            return false;
        }
    }
        
    return true;
}

template <class T>
bool
both_nans(T a, T b)
{
    return std::isnan(a) && std::isnan(b);
}

template <>
bool
both_nans<half>(half a, half b)
{
    return a.isNan() && b.isNan();
}

template <>
bool
both_nans<uint32_t>(uint32_t a, uint32_t b)
{
    return false;
}

template <class T>
bool
array_equals(const py::buffer_info& a, const py::buffer_info& b,
             const std::string& name, int width, int height)
{
    const T* apixels = static_cast<const T*>(a.ptr);
    const T* bpixels = static_cast<const T*>(b.ptr);

    for (int y=0; y<height; y++)
        for (int x=0; x<width; x++)
        {
            int i = y * width + x;
            if (!(apixels[i] == bpixels[i]))
            {
                if (both_nans(apixels[i], bpixels[i]))
                    continue;
                
                std::cout << "i=" << i
                          << " a[" << y
                          << "][" << x
                          << "] = " << apixels[i]
                          << " b=" << bpixels[i]
                          << std::endl;
                return false;
            }
        }

    return true;
}

void
PyChannel::validate_pixel_array()
{
    if (!(py::isinstance<py::array_t<uint32_t>>(pixels) ||
          py::isinstance<py::array_t<half>>(pixels) ||
          py::isinstance<py::array_t<float>>(pixels)))
        throw std::invalid_argument("invalid pixel array: unrecognized type: must be uint32, half, or float");

    if (pixels.ndim() != 2)
        throw std::invalid_argument("invalid pixel array: must be 2D numpy array");
}

bool
PyChannel::operator==(const PyChannel& other) const
{
    if (name == other.name && 
        xSampling == other.xSampling && 
        ySampling == other.ySampling &&
        pLinear == other.pLinear &&
        pixels.ndim() == other.pixels.ndim() && 
        pixels.size() == other.pixels.size())
    {
        if (pixels.size() == 0)
            return true;
        
        py::buffer_info buf = pixels.request();
        py::buffer_info obuf = other.pixels.request();

        int width = pixels.shape(1);
        int height = pixels.shape(0);
        if (py::isinstance<py::array_t<uint32_t>>(pixels) && py::isinstance<py::array_t<uint32_t>>(other.pixels))
            if (array_equals<uint32_t>(buf, obuf, name, width, height))
                return true;
        if (py::isinstance<py::array_t<half>>(pixels) && py::isinstance<py::array_t<half>>(other.pixels))
            if (array_equals<half>(buf, obuf, name, width, height))
                return true;
        if (py::isinstance<py::array_t<float>>(pixels) && py::isinstance<py::array_t<float>>(other.pixels))
            if (array_equals<float>(buf, obuf, name, width, height))
                return true;
    }

    return false;
}
        
V2i
PyPart::shape() const
{
    V2i S(0, 0);
        
    std::string channel_name; // first channel name

    for (auto c : channels)
    {
        auto C = py::cast<PyChannel&>(c.second);

        if (C.pixels.ndim() != 2)
            throw std::invalid_argument("error: channel must have a 2D array");

        V2i c_S(C.pixels.shape(0), C.pixels.shape(1));
            
        if (S == V2i(0, 0))
        {
            S = c_S;
            channel_name = C.name;
        }
        
        if (S != c_S)
        {
            std::stringstream s;
            s << "channel shapes differ: " << channel_name
              << "=" << S
              << ", " << C.name
              << "=" << c_S;
            throw std::invalid_argument(s.str());
        }
    }                

    return S;
}

size_t
PyPart::width() const
{
    return shape()[1];
}

size_t
PyPart::height() const
{
    return shape()[0];
}

std::string
PyPart::name() const
{
    if (header.contains("name"))
        return py::str(header["name"]);
    return "";
}

Compression
PyPart::compression() const
{
    if (header.contains("compression"))
        return header["compression"].cast<Compression>();
    return ZIP_COMPRESSION;
}

exr_storage_t
PyPart::type() const
{
    if (header.contains("type"))
        return header[py::str("type")].cast<exr_storage_t>();
    return EXR_STORAGE_SCANLINE;
}

std::string
PyPart::typeString() const
{
    switch (type())
    {
      case EXR_STORAGE_SCANLINE:
          return SCANLINEIMAGE;
      case EXR_STORAGE_TILED:
          return TILEDIMAGE;
      case EXR_STORAGE_DEEP_SCANLINE:
          return DEEPSCANLINE;
      case EXR_STORAGE_DEEP_TILED:
          return DEEPTILE;
      default:
          throw std::runtime_error("invalid type");
    }       
    return SCANLINEIMAGE;
}

PixelType
PyChannel::pixelType() const
{
    auto buf = pybind11::array::ensure(pixels);
    if (buf)
    {
        if (py::isinstance<py::array_t<uint32_t>>(buf))
            return UINT;
        if (py::isinstance<py::array_t<half>>(buf))
            return HALF;      
        if (py::isinstance<py::array_t<float>>(buf))
            return FLOAT;
    }
    return NUM_PIXELTYPES;
}

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
        .value("NUM_ROUNDING_MODES", NUM_ROUNDINGMODES)
        .export_values();

    py::enum_<LevelMode>(m, "LevelMode")
        .value("ONE_LEVEL", ONE_LEVEL)
        .value("MIPMAP_LEVELS", MIPMAP_LEVELS)
        .value("RIPMAP_LEVELS", RIPMAP_LEVELS)
        .value("NUM_LEVEL_MODES", NUM_LEVELMODES)
        .export_values();

    py::enum_<LineOrder>(m, "LineOrder")
        .value("INCREASING_Y", INCREASING_Y)
        .value("DECREASING_Y", DECREASING_Y)
        .value("RANDOM_Y", RANDOM_Y)
        .value("NUM_LINE_ORDERS", NUM_LINEORDERS)
        .export_values();

    py::enum_<PixelType>(m, "PixelType")
        .value("UINT", UINT)
        .value("HALF", HALF)
        .value("FLOAT", FLOAT)
        .value("NUM_PIXELTYPES", NUM_PIXELTYPES)
        .export_values();

    py::enum_<Compression>(m, "Compression")
        .value("NO_COMPRESSION", NO_COMPRESSION)
        .value("RLE_COMPRESSION", RLE_COMPRESSION)
        .value("ZIPS_COMPRESSION", ZIPS_COMPRESSION)
        .value("ZIP_COMPRESSION", ZIP_COMPRESSION)
        .value("PIZ_COMPRESSION", PIZ_COMPRESSION)
        .value("PXR24_COMPRESSION", PXR24_COMPRESSION)
        .value("B44_COMPRESSION", B44_COMPRESSION)
        .value("B44A_COMPRESSION", B44A_COMPRESSION)
        .value("DWAA_COMPRESSION", DWAA_COMPRESSION)
        .value("DWAB_COMPRESSION", DWAB_COMPRESSION)
        .value("NUM_COMPRESSION_METHODS", NUM_COMPRESSION_METHODS)
        .export_values();
    
    py::enum_<Envmap>(m, "Envmap")
        .value("ENVMAP_LATLONG", ENVMAP_LATLONG)
        .value("ENVMAP_CUBE", ENVMAP_CUBE)    
        .value("NUM_ENVMAPTYPES", NUM_ENVMAPTYPES)
        .export_values();

    py::enum_<exr_storage_t>(m, "Storage")
        .value("scanlineimage", EXR_STORAGE_SCANLINE)
        .value("tiledimage", EXR_STORAGE_TILED)
        .value("deepscanline", EXR_STORAGE_DEEP_SCANLINE)
        .value("deeptile", EXR_STORAGE_DEEP_TILED)
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
        .def(py::init<int,int,bool>(),
             py::arg("xSampling"),
             py::arg("ySampling"),
             py::arg("pLinear")=false)
        .def(py::init<py::array>())
        .def(py::init<py::array,int,int,bool>(),
             py::arg("pixels"),
             py::arg("xSampling"),
             py::arg("ySampling"),
             py::arg("pLinear")=false)
        .def(py::init<const char*>())
        .def(py::init<const char*,int,int,bool>(),
             py::arg("name"),
             py::arg("xSampling"),
             py::arg("ySampling"),
             py::arg("pLinear")=false)
        .def(py::init<const char*,py::array>())
        .def(py::init<const char*,py::array,int,int,bool>(),
             py::arg("name"),
             py::arg("pixels"),
             py::arg("xSampling"),
             py::arg("ySampling"),
             py::arg("pLinear")=false)
        .def("__repr__", [](const PyChannel& c) { return repr(c); })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("name", &PyChannel::name)
        .def("type", &PyChannel::pixelType)
        .def_readwrite("xSampling", &PyChannel::xSampling)
        .def_readwrite("ySampling", &PyChannel::ySampling)
        .def_readwrite("pLinear", &PyChannel::pLinear)
        .def_readwrite("pixels", &PyChannel::pixels)
        .def_readonly("channel_index", &PyChannel::channel_index)
        ;
    
    py::class_<PyPart>(m, "Part")
        .def(py::init())
        .def(py::init<py::dict,py::dict,exr_storage_t,Compression,std::string>(),
             py::arg("header"),
             py::arg("channels"),
             py::arg("type")=EXR_STORAGE_SCANLINE,
             py::arg("compression")=ZIP_COMPRESSION,
             py::arg("name")="")
        .def("__repr__", [](const PyPart& p) { return repr(p); })
        .def(py::self == py::self)
        .def("name", &PyPart::name)
        .def("type", &PyPart::type)
        .def("width", &PyPart::width)
        .def("height", &PyPart::height)
        .def("compression", &PyPart::compression)
        .def_readwrite("header", &PyPart::header)
        .def_readwrite("channels", &PyPart::channels)
        .def_readonly("part_index", &PyPart::part_index)
        ;

    py::class_<PyFile>(m, "File")
        .def(py::init<>())
        .def(py::init<std::string>())
        .def(py::init<py::dict,py::dict,exr_storage_t,Compression>(),
             py::arg("header"),
             py::arg("channels"),
             py::arg("type")=EXR_STORAGE_SCANLINE,
             py::arg("compression")=ZIP_COMPRESSION)
        .def(py::init<py::list>())
        .def(py::self == py::self)
        .def_readwrite("filename", &PyFile::filename)
        .def_readwrite("parts", &PyFile::parts)
        .def("header", &PyFile::header, py::arg("part_index") = 0)
        .def("channels", &PyFile::channels, py::arg("part_index") = 0)
        .def("write", &PyFile::write)
        ;
}

