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

#include <ImfTimeCodeAttribute.h>
#include <ImfTileDescription.h>
#include <ImfRational.h>
#include <ImfKeyCode.h>
#include <ImfPreviewImage.h>

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>
#include <ImathMath.h>

#include <typeinfo>
#include <sys/types.h>

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
#include <ImfMultiPartInputFile.h>
#include <ImfPreviewImageAttribute.h>
#include <ImfRationalAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfStringVectorAttribute.h>
#include <ImfFloatVectorAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfVecAttribute.h>

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
    
template <class T> const T* py_cast(const py::object& object);
template <class T, class S> const T* py_cast(const py::object& object);

void
diff(const char* func, int line)
{
    std::cout << func << " " << line << std::endl;
}
    
static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
    const char* filename = "";
    exr_get_file_name (f, &filename);
    std::stringstream s;
    s << "error \"" << filename << "\": " << msg;
    throw std::runtime_error(s.str());
}

void set_attribute(Header& header, const std::string& name, py::object object);

void
PyFile::multiPartOutputFile(const char* filename)
{
    std::vector<Header> headers;

    for (size_t p=0; p<parts.size(); p++)
    {
        PyPart& P = py::cast<PyPart&>(parts[p]);
        
        Header header (P.width,
                       P.height,
                       1.f,
                       IMATH_NAMESPACE::V2f (0, 0),
                       1.f,
                       INCREASING_Y,
                       ZIPS_COMPRESSION);

        header.setName (P.name);

        for (auto a : P.header)
        {
            auto name = py::str(a.first);
            py::object second = py::cast<py::object>(a.second);
            set_attribute(header, name, second);
        }
        
        for (auto c : P.channels)
        {
            auto C = py::cast<PyChannel&>(c.second);
            auto t = static_cast<PixelType>(C.pixelType());
            header.channels ().insert(C.name, Channel (t, C.xSampling, C.ySampling, C.pLinear));
        }

        switch (P.type)
        {
        case EXR_STORAGE_SCANLINE:
            header.setType ("scanlineimage");
            break;
        case EXR_STORAGE_TILED:
            header.setType ("tiledimage");
            break;
        case EXR_STORAGE_DEEP_SCANLINE:
            header.setType("deepscanlineimage");
            break;
        case EXR_STORAGE_DEEP_TILED:
            header.setType("deeptiledimage");
            break;
        case EXR_STORAGE_LAST_TYPE:
        default:
            throw std::runtime_error("unknown storage type");
            break;
        }

        if (P.header.contains("tiles"))
        {
            auto td = P.header["tiles"].cast<const TileDescription&>();
            header.setTileDescription (td);
        }

        if (P.header.contains("lineOrder"))
        {
            auto lo = P.header["lineOrder"].cast<exr_lineorder_t&>();
            header.lineOrder() = static_cast<LineOrder>(lo);
        }

        headers.push_back (header);
    }

    MultiPartOutputFile outfile(filename, headers.data(), headers.size());

    for (size_t p=0; p<parts.size(); p++)
    {
        PyPart& P = py::cast<PyPart&>(parts[p]);

        auto header = headers[p];
        const Box2i& dw = header.dataWindow();

        std::set<std::pair<int,int>> samplingFactors;

        if (P.type == EXR_STORAGE_SCANLINE ||
            P.type == EXR_STORAGE_TILED)
        {
            for (auto c : P.channels)
            {
                auto C = py::cast<PyChannel&>(c.second);
                samplingFactors.insert(std::pair<int,int>(C.xSampling, C.ySampling));
            }
            
            for (auto s : samplingFactors)
            {
                FrameBuffer frameBuffer;
        
                for (auto c : P.channels)
                {
                    auto C = py::cast<PyChannel&>(c.second);
                    if (true/*C.xSampling == s.first && C.ySampling == s.second*/)
                        frameBuffer.insert (C.name,
                                            Slice::Make (static_cast<PixelType>(C.pixelType()),
                                                         static_cast<void*>(C.pixels.request().ptr),
                                                         dw, 0, 0,
                                                         C.xSampling,
                                                         C.ySampling));
                }
                
                if (P.type == EXR_STORAGE_SCANLINE)
                {
                    OutputPart part(outfile, p);
                    part.setFrameBuffer (frameBuffer);
                    part.writePixels (P.height);
                }
                else if (P.type == EXR_STORAGE_TILED)
                {
                    TiledOutputPart part(outfile, p);
                    part.setFrameBuffer (frameBuffer);
                    part.writeTiles (0, part.numXTiles() - 1, 0, part.numYTiles() - 1);
                }
            }
        }
        else if (P.type == EXR_STORAGE_DEEP_SCANLINE ||
                 P.type == EXR_STORAGE_DEEP_TILED)
        {
            for (auto c : P.channels)
            {
                auto C = py::cast<PyChannel&>(c.second);
                samplingFactors.insert(std::pair<int,int>(C.xSampling, C.ySampling));
            }
            
            for (auto s : samplingFactors)
            {
                DeepFrameBuffer frameBuffer;
        
                for (auto c : P.channels)
                {
                    auto C = py::cast<PyChannel&>(c.second);
                    if (true/*C.xSampling == s.first && C.ySampling == s.second*/)
                        frameBuffer.insert (C.name,
                                            DeepSlice (static_cast<PixelType>(C.pixelType()),
                                                       static_cast<char*>(C.pixels.request().ptr),
                                                       0, 0, 0,
                                                       C.xSampling,
                                                       C.ySampling));
                }
        
                if (P.type == EXR_STORAGE_DEEP_SCANLINE)
                {
                    DeepScanLineOutputPart part(outfile, p);
                    part.setFrameBuffer (frameBuffer);
                    part.writePixels (P.height);
                }
                else if (P.type == EXR_STORAGE_DEEP_TILED)
                {
                    DeepTiledOutputPart part(outfile, p);
                    part.setFrameBuffer (frameBuffer);
                    part.writeTiles (0, part.numXTiles() - 1, 0, part.numYTiles() - 1);
                }
            }
        }
    }
}
    
py::object
get_attribute_object(const char* name, const Attribute* a)
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
        return py::cast(exr_compression_t(v->value()));

    if (auto v = dynamic_cast<const DoubleAttribute*> (a))
        return py::cast(PyDouble(v->value()));

    if (auto v = dynamic_cast<const EnvmapAttribute*> (a))
        return py::cast(exr_envmap_t(v->value()));

    if (auto v = dynamic_cast<const FloatAttribute*> (a))
        return py::float_(v->value());

    if (auto v = dynamic_cast<const IntAttribute*> (a))
        return py::int_(v->value());

    if (auto v = dynamic_cast<const KeyCodeAttribute*> (a))
        return py::cast(v->value());

    if (auto v = dynamic_cast<const LineOrderAttribute*> (a))
        return py::cast(exr_lineorder_t(v->value()));

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
        if (!strcmp(name, "type"))
        {
            //
            // The "type" attribute comes through as a string,
            // but we want it to be the OpenEXR.Storage enum.
            //
                  
            exr_storage_t t = EXR_STORAGE_LAST_TYPE;
            if (v->value() == "scanlineimage")
                t = EXR_STORAGE_SCANLINE;
            else if (v->value() == "tiledimage")
                t = EXR_STORAGE_TILED;
            else if (v->value() == "deepscanline")
                t = EXR_STORAGE_DEEP_SCANLINE;
            else if (v->value() == "deeptile") 
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
    
    return py::none();
}
    
void
set_attribute(Header& header, const std::string& name, py::object object)
{
#if XXX
    std::cout << "header " << header.name()
              << " set_attribute: name=" << name
              << " value=" << py::str(object)
              << std::endl;
#endif
    if (auto v = py_cast<Box2i>(object))
    {
        header.insert(name, Box2iAttribute(*v));
    }
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
    else if (auto v = py_cast<exr_compression_t>(object))
        header.insert(name, CompressionAttribute(static_cast<Compression>(*v)));
    else if (auto v = py_cast<exr_envmap_t>(object))
        header.insert(name, EnvmapAttribute(static_cast<Envmap>(*v)));
    else if (py::isinstance<py::float_>(object))
        header.insert(name, FloatAttribute(py::cast<py::float_>(object)));
    else if (py::isinstance<PyDouble>(object))
        header.insert(name, DoubleAttribute(py::cast<PyDouble>(object).d));
    else if (py::isinstance<py::int_>(object))
        header.insert(name, IntAttribute(py::cast<py::int_>(object)));
    else if (auto v = py_cast<KeyCode>(object))
        header.insert(name, KeyCodeAttribute(*v));
    else if (auto v = py_cast<exr_lineorder_t>(object))
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
        const char* type;
        switch (*v)
        {
        case EXR_STORAGE_SCANLINE:
            type = "scanlineimage";
            break;
        case EXR_STORAGE_TILED:
            type = "tiledimage";
            break;
        case EXR_STORAGE_DEEP_SCANLINE:
            type = "deepscanlineimage";
            break;
        case EXR_STORAGE_DEEP_TILED:
            type = "deeptiledimage";
            break;
        case EXR_STORAGE_LAST_TYPE:
        default:
            throw std::runtime_error("unknown storage type");
            break;
        }
        header.insert(name, StringAttribute(type));
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


void
PyFile::multiPartInputFile(const char* filename)
{
    MultiPartInputFile infile(filename);

    for (int part_index = 0; part_index < infile.parts(); part_index++)
    {
        const Header& header = infile.header(part_index);

        PyPart P;

        std::stringstream s;
        s << "Part" << part_index;
        P.name = s.str();

        P.part_index = part_index;
        
        const Box2i& dw = header.dataWindow();
        P.width = static_cast<size_t>(dw.max.x - dw.min.x + 1);
        P.height = static_cast<size_t>(dw.max.y - dw.min.y + 1);
        
        P.compression = static_cast<exr_compression_t>(header.compression());

        if (header.type() == "scanlineimage")
            P.type = EXR_STORAGE_SCANLINE;
        else if (header.type() == "tiledimage")
            P.type = EXR_STORAGE_TILED;
        
        for (auto a = header.begin(); a != header.end(); a++)
        {
            const char* name = a.name();
            const Attribute& attribute = a.attribute();
            P.header[name] = get_attribute_object(name, &attribute);
            if (!strcmp(name, "name"))
                P.name = header.name();
        }

        std::vector<size_t> shape ({P.height, P.width});

        InputPart part (infile, part_index);

        std::set<std::pair<int,int>> samplingFactors;
        for (auto c = header.channels().begin(); c != header.channels().end(); c++)
            samplingFactors.insert(std::pair<int,int>(c.channel().xSampling, c.channel().ySampling));
        for (auto s : samplingFactors)
        {
            FrameBuffer frameBuffer;

            for (auto c = header.channels().begin(); c != header.channels().end(); c++)
            {
#if XXX
                if (c.channel().xSampling != s.first ||
                    c.channel().ySampling != s.second)
                    continue;
#endif                
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

            part.setFrameBuffer (frameBuffer);
            part.readPixels (dw.min.y, dw.max.y);
        }
        
        parts.append(py::cast<PyPart>(PyPart(P)));
    } // for parts
}
    
void
PyFile::TiledRgbaInputFile(const char* filename)
{
    Imf::TiledRgbaInputFile file(filename);

    const Box2i&       dw = file.dataWindow ();

    int width  = dw.max.x - dw.min.x + 1;
    int height = dw.max.y - dw.min.y + 1;
    int dwx    = dw.min.x;
    int dwy    = dw.min.y;

    Array2D<Rgba> pixels (height, width);
    file.setFrameBuffer (&pixels[-dwy][-dwx], 1, width);
    file.readTiles (0, file.numXTiles () - 1, 0, file.numYTiles () - 1);

    std::cout << "TiledRgbaInputFile: height=" << height << " width=" << width << std::endl;
    for (int y=0; y<height; y++)
        for (int x=0; x<width; x++)
        {
            int i = y * width + x;
            std::cout << i << " pixels[" << y
                      << "][" << x
                      << "]=(" << pixels[y][x].r
                      << ", " << pixels[y][x].g
                      << ", " << pixels[y][x].b
                      << ")" << std::endl;
        }
}
    
void
PyFile::RgbaInputFile(const char* filename)
{
    Imf::RgbaInputFile file(filename);
    Imath::Box2i       dw = file.dataWindow();
    int                width  = dw.max.x - dw.min.x + 1;
    int                height = dw.max.y - dw.min.y + 1;

    Imf::Array2D<Imf::Rgba> pixels(height, width);
        
    file.setFrameBuffer(&pixels[0][0], 1, width);
    file.readPixels(dw.min.y, dw.max.y);

    std::cout << "RgbaInputFile: height=" << height << " width=" << width << std::endl;

    for (int y=0; y<height; y++)
        for (int x=0; x<width; x++)
        {
            int i = y * width + x;
            std::cout << i << " pixels[" << y
                      << "][" << x
                      << "]=(" << pixels[y][x].r
                      << ", " << pixels[y][x].g
                      << ", " << pixels[y][x].b
                      << ")" << std::endl;
        }
}

//
// Create a PyFile out of a list of parts (i.e. a multi-part file)
//

PyFile::PyFile(const py::list& p)
    : parts(p)
{
    for (auto v : parts)
        if (!py::isinstance<PyPart>(*v))
            throw std::invalid_argument("must be a list of OpenEXR.Part() objects");
}

//
// Create a PyFile out of a single part: header, channels,
// type, and compression (i.e. a single-part file)
//

PyFile::PyFile(const py::dict& header, const py::dict& channels,
               exr_storage_t type, exr_compression_t compression)
{
    parts.append(py::cast<PyPart>(PyPart("Part0", header, channels, type, compression)));
}

//
// Read a PyFile from the given filename
//


PyFile::PyFile(const std::string& filename)
    : filename (filename)
{
    exr_result_t              rv;
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    rv = exr_start_read (&f, filename.c_str(), &cinit);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::stringstream s;
        s << "can't open " << filename << " for reading";
        throw std::runtime_error(s.str());
    }
    

    //
    // Read the parts
    //
        
    int numparts;
        
    rv = exr_get_count (f, &numparts);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error ("read error");

    parts = py::list();
    
    for (int part_index = 0; part_index < numparts; ++part_index)
        parts.append(PyPart(f, part_index));
}

bool
PyFile::operator==(const PyFile& other) const
{
    if (parts.size() != other.parts.size())
    {
        diff(__PRETTY_FUNCTION__, __LINE__);
        return false;
    }
    
    for (size_t i = 0; i<parts.size(); i++)
        if (!(py::cast<const PyPart&>(parts[i]) == py::cast<const PyPart&>(other.parts[i])))
        {
            diff(__PRETTY_FUNCTION__, __LINE__);
            return false;
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
    //
    // Open the file for writing
    //

    exr_context_t f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    exr_result_t result = exr_start_write(&f, outfilename, EXR_WRITE_FILE_DIRECTLY, &cinit);
    if (result != EXR_ERR_SUCCESS)
    {
        std::stringstream s;
        s << "can't open " << outfilename << " for write";
        throw std::runtime_error(s.str());
    }

    exr_set_longname_support(f, 1);

    //
    // Set up the parts
    //
    
    for (size_t p=0; p<parts.size(); p++)
    {
        PyPart& P = py::cast<PyPart&>(parts[p]);
        
        P.set_attributes(f);
        P.add_channels(f);
        
        result = exr_set_version(f, p, 1); // 1 is the latest version
        if (result != EXR_ERR_SUCCESS) 
            throw std::runtime_error("error writing version");
    }

    //
    // Write the header
    //
    
    result = exr_write_header(f);
    if (result != EXR_ERR_SUCCESS)
        throw std::runtime_error("error writing header");

    //
    // Write the parts
    //
    
    for (size_t p=0; p<parts.size(); p++)
    {
        auto P = py::cast<const PyPart&>(parts[p]);
        P.write(f);
    }

    result = exr_finish(&f);

    filename = outfilename;
}

//
// Construct a part from explicit header and channel data.
// 
// Used to construct a file for writing.
//

PyPart::PyPart(const char* name, const py::dict& header, const py::dict& channels,
               exr_storage_t type, exr_compression_t compression)
    : name(name), type(type), width(0), height(0),
      compression(compression),
      header(header), channels(channels), part_index(0)
{
    if (type >= EXR_STORAGE_LAST_TYPE)
        throw std::invalid_argument("invalid storage type");
    
    if (compression >= EXR_COMPRESSION_LAST_TYPE)
        throw std::invalid_argument("invalid compression type");

    for (auto a : header)
    {
        if (!py::isinstance<py::str>(a.first))
            throw std::invalid_argument("header key must be string (attribute name)");
        
        // TODO: confirm it's a valid attribute value
        py::object second = py::cast<py::object>(a.second);
    }
    
    //
    // Confirm all the channels have 2 dimensions and the same size
    //
    
    for (auto c : channels)
    {
        if (!py::isinstance<py::str>(c.first))
            throw std::invalid_argument("channels key must be string (channel name)");
        auto name = py::str(c.first);

        if (!py::isinstance<const PyChannel&>(c.second))
            throw std::invalid_argument("channels value must be a OpenEXR.Channel() object");

        //
        // Initialize the channel's name field to match the dict key.
        //
        
        py::cast<PyChannel&>(channels[c.first]).name = name;

        std::string channel_name;
        auto C = py::cast<const PyChannel&>(c.second);
        if (C.pixels.ndim() == 2)
        {
            uint32_t w = C.pixels.shape(1);
            uint32_t h = C.pixels.shape(0);

            if (width == 0)
                width = w;
            if (height == 0)
                height = h;
            
            if (w != width)
            {
                std::stringstream s;
                s << "channel widths differ: " << channel_name << "=" << width << ", " << C.name << "=" << w;
                throw std::invalid_argument(s.str());
            }
            if (h != height)
            {
                std::stringstream s;
                s << "channel heights differ: " << channel_name << "=" << height << ", " << C.name << "=" << h;
                throw std::invalid_argument(s.str());
            }                

            channel_name = C.name;
        }
        else
            throw std::invalid_argument("error: channel must have a 2D array");
    }

#if TODO    
    if (!header.contains("dataWindow"))
        header["dataWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));

    if (!header.contains("displayWindow"))
        header["displayWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));
#endif
}
    
//
// Read a part from a file.
// 
// This is invoked from File(filename) when reading an image from a
// file.
//

PyPart::PyPart(exr_context_t f, int part_index)
    : width(0), height(0), part_index(part_index)
{
    exr_result_t rv;

    //
    // Read the attributes into the header
    //
    
    int32_t attrcount;
    rv = exr_get_attribute_count(f, part_index, &attrcount);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error ("read error");
    
    for (int32_t a = 0; a < attrcount; ++a)
    {
        std::string s;
        py::object attr = get_attribute_object(f, a, s);
        header[s.c_str()] = attr;
        if (s == "name")
            name = py::str(attr);
    }

    //
    // Read the type (i.e. scanline, tiled, deep, etc)
    //
    
    rv = exr_get_storage (f, part_index, &type);
    if (rv != EXR_ERR_SUCCESS)
        return;

    //
    // Read the compression type
    //
        
    rv = exr_get_compression(f, part_index, &compression);
    if (rv != EXR_ERR_SUCCESS)
        return;
    
    //
    // Read the part
    //
        
    if (type == EXR_STORAGE_SCANLINE) 
        read_scanline_part (f);
    else if (type == EXR_STORAGE_TILED)
        read_tiled_part (f);
    else if (type == EXR_STORAGE_DEEP_SCANLINE)
        throw std::runtime_error("deepscanline not implemented");
    else if (type == EXR_STORAGE_DEEP_TILED)
        throw std::runtime_error("deeptiled not implemented");
}

//
// Read an attribute from the file at the given index and create a
// corresponding python object.
//

py::object
PyPart::get_attribute_object(exr_context_t f, int32_t attr_index, std::string& name) 
{
    exr_result_t rv;

    const exr_attribute_t* attr;
    rv = exr_get_attribute_by_index(f, part_index, EXR_ATTR_LIST_FILE_ORDER, attr_index, &attr);
    if (rv != EXR_ERR_SUCCESS)
    {
        name = "";
        return py::none();
    }
    
    name = attr->name;
    
    switch (attr->type)
    {
      case EXR_ATTR_BOX2I:
          return py::cast(Box2i(V2i(attr->box2i->min),V2i(attr->box2i->max)));
      case EXR_ATTR_BOX2F:
          return py::cast(Box2f(V2f(attr->box2f->min),V2f(attr->box2f->max)));
      case EXR_ATTR_CHLIST:
          {
              auto l = py::list();
              for (int c = 0; c < attr->chlist->num_channels; ++c)
              {
                  auto e = attr->chlist->entries[c];
                  l.append(py::cast(PyChannel(e.name.str, e.x_sampling, e.y_sampling)));
              }
              return l;
          }
      case EXR_ATTR_CHROMATICITIES:
          {
              PyChromaticities c(attr->chromaticities->red_x,
                                 attr->chromaticities->red_y,
                                 attr->chromaticities->green_x,
                                 attr->chromaticities->green_y,
                                 attr->chromaticities->blue_x,
                                 attr->chromaticities->blue_y,
                                 attr->chromaticities->white_x,
                                 attr->chromaticities->white_y);
              return py::cast(c);
          }
      case EXR_ATTR_COMPRESSION: 
          return py::cast(exr_compression_t(attr->uc));
      case EXR_ATTR_DOUBLE:
          return py::cast(PyDouble(attr->d));
      case EXR_ATTR_ENVMAP:
          return py::cast(exr_envmap_t(attr->uc));
      case EXR_ATTR_FLOAT:
          return py::float_(attr->f);
      case EXR_ATTR_FLOAT_VECTOR:
          {
              auto l = py::list();
              for (int i = 0; i < attr->floatvector->length; ++i)
                  l.append(py::float_(attr->floatvector->arr[i]));
              return l;
          }
          break;
      case EXR_ATTR_INT:
          return py::int_(attr->i);
      case EXR_ATTR_KEYCODE:
          return py::cast(KeyCode(attr->keycode->film_mfc_code,
                                  attr->keycode->film_type,
                                  attr->keycode->prefix,   
                                  attr->keycode->count,            
                                  attr->keycode->perf_offset,      
                                  attr->keycode->perfs_per_frame,
                                  attr->keycode->perfs_per_count));
      case EXR_ATTR_LINEORDER:
          return py::cast(exr_lineorder_t(attr->uc));
      case EXR_ATTR_M33F:
          return py::cast(M33f(attr->m33f->m[0],
                               attr->m33f->m[1],
                               attr->m33f->m[2],
                               attr->m33f->m[3],
                               attr->m33f->m[4],
                               attr->m33f->m[5],
                               attr->m33f->m[6],
                               attr->m33f->m[7],
                               attr->m33f->m[8]));
      case EXR_ATTR_M33D:
          return py::cast(M33d(attr->m33d->m[0],
                               attr->m33d->m[1],
                               attr->m33d->m[2],
                               attr->m33d->m[3],
                               attr->m33d->m[4],
                               attr->m33d->m[5],
                               attr->m33d->m[6],
                               attr->m33d->m[7],
                               attr->m33d->m[8]));
      case EXR_ATTR_M44F:
          return py::cast(M44f(attr->m44f->m[0],
                               attr->m44f->m[1],
                               attr->m44f->m[2],
                               attr->m44f->m[3],
                               attr->m44f->m[4],
                               attr->m44f->m[5],
                               attr->m44f->m[6],
                               attr->m44f->m[7],
                               attr->m44f->m[8],
                               attr->m44f->m[9],
                               attr->m44f->m[10],
                               attr->m44f->m[11],
                               attr->m44f->m[12],
                               attr->m44f->m[13],
                               attr->m44f->m[14],
                               attr->m44f->m[15]));
      case EXR_ATTR_M44D:
          return py::cast(M44d(attr->m44d->m[0],
                               attr->m44d->m[1],
                               attr->m44d->m[2],
                               attr->m44d->m[3],
                               attr->m44d->m[4],
                               attr->m44d->m[5],
                               attr->m44d->m[6],
                               attr->m44d->m[7],
                               attr->m44d->m[8],
                               attr->m44d->m[9],
                               attr->m44d->m[10],
                               attr->m44d->m[11],
                               attr->m44d->m[12],
                               attr->m44d->m[13],
                               attr->m44d->m[14],
                               attr->m44d->m[15]));
      case EXR_ATTR_PREVIEW:
          {
              auto pixels = reinterpret_cast<const PreviewRgba*>(attr->preview->rgba);
              return py::cast(PyPreviewImage(attr->preview->width,
                                           attr->preview->height,
                                           pixels));
          }
      case EXR_ATTR_RATIONAL:
          return py::cast(Rational(attr->rational->num, attr->rational->denom));
          break;
      case EXR_ATTR_STRING:
          {
              if (name == "type")
              {
                  //
                  // The "type" attribute comes through as a string,
                  // but we want it to be the OpenEXR.Storage enum.
                  //
                  
                  exr_storage_t t = EXR_STORAGE_LAST_TYPE;
                  if (strcmp (attr->string->str, "scanlineimage") == 0)
                      t = EXR_STORAGE_SCANLINE;
                  else if (strcmp(attr->string->str, "tiledimage") == 0)
                      t = EXR_STORAGE_TILED;
                  else if (strcmp(attr->string->str, "deepscanline") == 0)
                      t = EXR_STORAGE_DEEP_SCANLINE;
                  else if (strcmp(attr->string->str, "deeptile") == 0)
                      t = EXR_STORAGE_DEEP_TILED;
                  else
                      throw std::invalid_argument("unrecognized image 'type' attribute");
                  return py::cast(t);
              }
              return py::str(attr->string->str);
          }
          
          break;
      case EXR_ATTR_STRING_VECTOR:
          {
              auto l = py::list();
              for (int i = 0; i < attr->stringvector->n_strings; ++i)
                  l.append(py::str(attr->stringvector->strings[i].str));
              return l;
          }
          break;
      case EXR_ATTR_TILEDESC: 
          {
              auto lm = LevelMode (EXR_GET_TILE_LEVEL_MODE (*(attr->tiledesc)));
              auto lrm = LevelRoundingMode(EXR_GET_TILE_ROUND_MODE (*(attr->tiledesc)));
              return py::cast(TileDescription(attr->tiledesc->x_size,
                                              attr->tiledesc->y_size,
                                              lm, lrm));
          }
      case EXR_ATTR_TIMECODE:
          return py::cast(TimeCode(attr->timecode->time_and_flags,
                                   attr->timecode->user_data));
      case EXR_ATTR_V2I:
          return py::cast(V2i(attr->v2i->x, attr->v2i->y));
      case EXR_ATTR_V2F:
          return py::cast(V2f(attr->v2f->x, attr->v2f->y));
      case EXR_ATTR_V2D:
          return py::cast(V2d(attr->v2d->x, attr->v2d->y));
      case EXR_ATTR_V3I:
          return py::cast(V3i(attr->v3i->x, attr->v3i->y, attr->v3i->z));
      case EXR_ATTR_V3F:
          return py::cast(V3f(attr->v3f->x, attr->v3f->y, attr->v3f->z));
      case EXR_ATTR_V3D:
          return py::cast(V3d(attr->v3d->x, attr->v3d->y, attr->v3d->z));
      case EXR_ATTR_OPAQUE: 
          return py::none();
      case EXR_ATTR_UNKNOWN:
      case EXR_ATTR_LAST_KNOWN_TYPE:
      default:
          throw std::invalid_argument("unknown attribute type");
          break;
    }
    return py::none();
}

//
// Read scanline data from a file.
//
// Called from Part(f)
//

void
PyPart::read_scanline_part (exr_context_t f)
{
    exr_result_t     rv;

    //
    // Get the width,height from the data window
    //

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part_index, &datawin);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad data window");

    width = (uint64_t) ((int64_t) datawin.max.x - (int64_t) datawin.min.x + 1);
    height = (uint64_t) ((int64_t) datawin.max.y - (int64_t) datawin.min.y + 1);

    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    int32_t lines_per_chunk;
    rv = exr_get_scanlines_per_chunk (f, part_index, &lines_per_chunk);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad scanlines per chunk");

    //
    // Read the chunks
    //

    for (uint64_t chunk = 0; chunk < height; chunk += lines_per_chunk)
    {
        exr_chunk_info_t cinfo = {0};
        int              y     = (int) chunk;

        rv = exr_read_scanline_chunk_info (f, part_index, y + datawin.min.y, &cinfo);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error("error reading scanline chunk");

        //
        // Initialze the decoder if not already done. This happens only on
        // the first chunk.
        //
        
        if (decoder.channels == NULL)
        {
            rv = exr_decoding_initialize (f, part_index, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error ("error initializing decoder");
            
            //
            // Initialize the channels dict, to be filled in below.
            //
            
            channels = py::dict();
            
            //
            // Build the channel list for the decoder and allocate each
            // channel's pixel data arrays. The decoder's array of
            // channels parallels the channels py::list, so
            // iterate through both in lock step.
            //
            
            for (int c = 0; c < decoder.channel_count; c++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];

                outc.decode_to_ptr     = (uint8_t*) 0x1000;
                outc.user_pixel_stride = outc.bytes_per_element * outc.x_samples;
                outc.user_line_stride  = outc.bytes_per_element * width / outc.x_samples;

                PyChannel C;
                C.name = outc.channel_name;
                C.xSampling  = outc.x_samples;
                C.ySampling  = outc.y_samples;
                C.pLinear = outc.p_linear;
                
                std::vector<size_t> shape, strides;
                shape.assign({ height, width });

                const auto style = py::array::c_style | py::array::forcecast;
                
                py::array pixels;
                switch (outc.data_type)
                {
                  case EXR_PIXEL_UINT:
                    strides.assign({ sizeof(uint32_t)*width, sizeof(uint32_t) });
                    C.pixels = py::array_t<uint32_t,style>(shape, strides);
                    break;
                  case EXR_PIXEL_HALF:
                    strides.assign({ sizeof(half)*width, sizeof(half) });
                    C.pixels = py::array_t<half,style>(shape, strides);
                    break;
                  case EXR_PIXEL_FLOAT:
                    strides.assign({ sizeof(float)*width, sizeof(float) });
                    C.pixels = py::array_t<float,style>(shape, strides);
                    break;
                  case EXR_PIXEL_LAST_TYPE:
                  default:
                      throw std::domain_error("invalid pixel type");
                      break;
                }

                channels[py::str(outc.channel_name)] = C;
            }

            rv = exr_decoding_choose_default_routines (f, part_index, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("error initialing decoder");
        }
        else
        {
            rv = exr_decoding_update (f, part_index, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("error updating decoder");
        }

        if (cinfo.type != EXR_STORAGE_DEEP_SCANLINE)
        {
            for (int16_t c = 0; c < decoder.channel_count; c++)
            {
                PyChannel& C = py::cast<PyChannel&>(channels[decoder.channels[c].channel_name]);
                exr_coding_channel_info_t& outc = decoder.channels[c];

                //
                // Set the decoder data pointer to the appropriate offset
                // into the pixel arrays
                //
                
                py::buffer_info buf = C.pixels.request();
                switch (outc.data_type)
                {
                  case EXR_PIXEL_UINT:
                      {
                          uint32_t* pixels = static_cast<uint32_t*>(buf.ptr);
                          outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[y*width]);
                      }
                      break;
                  case EXR_PIXEL_HALF:
                      {
                          half* pixels = static_cast<half*>(buf.ptr);
                          outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[y*width]);
                          outc.user_bytes_per_element = sizeof(half);
                      }
                      break;
                  case EXR_PIXEL_FLOAT:
                      {
                          float* pixels = static_cast<float*>(buf.ptr);
                          outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[y*width]);
                          outc.user_bytes_per_element = sizeof(float);
                      }
                      break;
                  case EXR_PIXEL_LAST_TYPE:
                  default:
                      throw std::domain_error("invalid pixel type");
                      break;
                }
            }
        }

        rv = exr_decoding_run (f, part_index, &decoder);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error("error in decoder");
    }

    exr_decoding_destroy (f, &decoder);
}

//
// Read scanline data from a file.
//
// Called from Part(f)
//

void
PyPart::read_tiled_part (exr_context_t f)
{
    exr_result_t rv;

    //
    // Get the width,height from the data window
    //

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part_index, &datawin);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad data window");

    width = datawin.max.x - datawin.min.x + 1;
    height = datawin.max.y - datawin.min.y + 1;

    uint32_t              txsz, tysz;
    exr_tile_level_mode_t levelmode;
    exr_tile_round_mode_t roundingmode;

    rv = exr_get_tile_descriptor (f, part_index, &txsz, &tysz, &levelmode, &roundingmode);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad tile descriptor");

    int32_t levelsx, levelsy;
    rv = exr_get_tile_levels (f, part_index, &levelsx, &levelsy);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad tile levels");

    //
    // Read the tiles
    //
    
    for (int32_t ylevel = 0; ylevel < levelsy; ++ylevel)
    {
        for (int32_t xlevel = 0; xlevel < levelsx; ++xlevel)
        {
            int32_t levw, levh;
            rv = exr_get_level_sizes (f, part_index, xlevel, ylevel, &levw, &levh);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("bad level sizes");

            int32_t curtw, curth;
            rv = exr_get_tile_sizes (f, part_index, xlevel, ylevel, &curtw, &curth);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("bad tile sizes");
            //
            // Initialze the decoder if not already done. This happens only on
            // the first chunk.
            //
        
            exr_chunk_info_t      cinfo;
            exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

            int tx = 0;
            int ty = 0;

            for (int64_t cury = 0; cury < levh; cury += curth, ++ty)
            {
                tx = 0;
                for (int64_t curx = 0; curx < levw; curx += curtw, ++tx)
                {
                    rv = exr_read_tile_chunk_info (f, part_index, tx, ty, xlevel, ylevel, &cinfo);
                    if (rv != EXR_ERR_SUCCESS)
                        throw std::runtime_error("error reading tile chunk");

                    if (decoder.channels == NULL)
                    {
                        rv = exr_decoding_initialize (f, part_index, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error initializing decoder");

                        //
                        // Initialize the channel dict, to be filled in below.
                        //
            
                        channels = py::list();

                        //
                        // Build the channel list for the decoder and allocate each
                        // channel's pixel data arrays. The decoder's array of
                        // channels parallels the channels py::list, so
                        // iterate through both in lock step.
                        //
            
                        for (int c = 0; c < decoder.channel_count; c++)
                        {
                            exr_coding_channel_info_t& outc = decoder.channels[c];
                            // fake addr for default routines
                            outc.decode_to_ptr = (uint8_t*) 0x1000;
                            outc.user_pixel_stride = outc.user_bytes_per_element;
                            outc.user_line_stride = outc.user_pixel_stride * curtw;

                            PyChannel C(outc.channel_name);
                            
                            std::vector<size_t> shape, strides;
                            shape.assign({ height, width });

                            const auto style = py::array::c_style | py::array::forcecast;
                
                            switch (outc.data_type)
                            {
                              case EXR_PIXEL_UINT:
                                  strides.assign({ sizeof(uint32_t)*width, sizeof(uint32_t) });
                                  C.pixels = py::array_t<uint32_t,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_HALF:
                                  strides.assign({ sizeof(half)*width, sizeof(half) });
                                  C.pixels = py::array_t<half,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_FLOAT:
                                  strides.assign({ sizeof(float)*width, sizeof(float) });
                                  C.pixels = py::array_t<float,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_LAST_TYPE:
                              default:      
                                  throw std::domain_error("invalid pixel type");
                                  break;
                            }

                            channels[py::str(outc.channel_name)] = C;
                        }

                        rv = exr_decoding_choose_default_routines (f, part_index, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error initializing decoder");
                    }
                    else
                    {
                        rv = exr_decoding_update (f, part_index, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error updating decoder");
                    }

                    if (cinfo.type != EXR_STORAGE_DEEP_TILED)
                    {
                        for (int c = 0; c < decoder.channel_count; c++)
                        {
                            exr_coding_channel_info_t& outc = decoder.channels[c];
                            outc.user_pixel_stride = outc.user_bytes_per_element;
                            outc.user_line_stride = outc.user_pixel_stride * curtw;

                            //
                            // Set the decoder data pointer to the appropriate offset
                            // into the pixel arrays
                            //
                
                            PyChannel& C = py::cast<PyChannel&>(channels[outc.channel_name]);

                            py::buffer_info buf = C.pixels.request();
                            switch (outc.data_type)
                            {
                              case EXR_PIXEL_UINT:
                                  {
                                      uint32_t* pixels = static_cast<uint32_t*>(buf.ptr);
                                      outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[cury*width]);
                                  }
                                  break;
                              case EXR_PIXEL_HALF:
                                  {
                                      half* pixels = static_cast<half*>(buf.ptr);
                                      outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[cury*width]);
                                  }
                                  break;
                              case EXR_PIXEL_FLOAT:
                                  {
                                      float* pixels = static_cast<float*>(buf.ptr);
                                      outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[cury*width]);
                                  }
                                  break;
                              case EXR_PIXEL_LAST_TYPE:
                              default:
                                  throw std::domain_error("invalid pixel type");
                                  break;
                            }
                        }
                    }

                    rv = exr_decoding_run (f, part_index, &decoder);
                    if (rv != EXR_ERR_SUCCESS)
                        throw std::runtime_error("error in decoder");
                }
            }

            exr_decoding_destroy (f, &decoder);
        }
    }
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

//
// Add an attribute to a file. Used during writing. This converts the
// python into the proper data format for the OpenEXRCore API.
//

void
PyPart::set_attribute(exr_context_t f, const std::string& name, py::object object)
{
    if (auto v = py_cast<exr_attr_box2i_t,Box2i>(object))
        exr_attr_set_box2i(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_box2f_t,Box2f>(object))
        exr_attr_set_box2f(f, part_index, name.c_str(), v);
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
            exr_attr_set_float_vector(f, part_index, name.c_str(), v.size(), &v[0]);
        }
        else if (py::isinstance<py::str>(list[0]))
        {
            // string vector
            std::vector<std::string> strings = list.cast<std::vector<std::string>>();
            std::vector<const char*> v;
            for (size_t i=0; i<strings.size(); i++)
                v.push_back(strings[i].c_str());
            exr_attr_set_string_vector(f, part_index, name.c_str(), v.size(), &v[0]);
        }
        else if (py::isinstance<PyChannel>(list[0]))
        {
            //
            // Channel list: don't create an explicit chlist attribute here,
            // since the channels get created elswhere.
        }
    }
    else if (auto o = py_cast<PyChromaticities>(object))
        exr_attr_set_chromaticities(f, part_index, name.c_str(), reinterpret_cast<const exr_attr_chromaticities_t*>(o));
    else if (auto o = py_cast<exr_compression_t>(object))
        exr_attr_set_compression(f, part_index, name.c_str(), *o);
    else if (auto o = py_cast<exr_envmap_t>(object))
        exr_attr_set_envmap(f, part_index, name.c_str(), *o);
    else if (py::isinstance<py::float_>(object))
        exr_attr_set_float(f, part_index, name.c_str(), py::cast<py::float_>(object));
    else if (py::isinstance<PyDouble>(object))
        exr_attr_set_double(f, part_index, name.c_str(), py::cast<PyDouble>(object).d);
    else if (py::isinstance<py::int_>(object))
        exr_attr_set_int(f, part_index, name.c_str(), py::cast<py::int_>(object));
    else if (auto o = py_cast<exr_attr_keycode_t,KeyCode>(object))
        exr_attr_set_keycode(f, part_index, name.c_str(), o);
    else if (auto o = py_cast<exr_lineorder_t>(object))
        exr_attr_set_lineorder(f, part_index, name.c_str(), *o);
    else if (auto v = py_cast<exr_attr_m33f_t,M33f>(object))
        exr_attr_set_m33f(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m33d_t,M33d>(object))
        exr_attr_set_m33d(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m44f_t,M44f>(object))
        exr_attr_set_m44f(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m44d_t,M44d>(object))
        exr_attr_set_m44d(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<PyPreviewImage>(object))
    {
        exr_attr_preview_t o;
        o.width = v->pixels.shape(1);
        o.height = v->pixels.shape(0);
        o.alloc_size = o.width * o.height * 4;
        py::buffer_info buf = v->pixels.request();
        o.rgba = static_cast<uint8_t*>(buf.ptr);
        exr_attr_set_preview(f, part_index, name.c_str(), &o);
    }
    else if (auto o = py_cast<exr_attr_rational_t,Rational>(object))
        exr_attr_set_rational(f, part_index, name.c_str(), o);
    else if (auto v = py_cast<TileDescription>(object))
    {
        exr_attr_tiledesc_t t;
        t.x_size = v->xSize;
        t.y_size = v->ySize;
        t.level_and_round = EXR_PACK_TILE_LEVEL_ROUND (v->mode, v->roundingMode);
        exr_attr_set_tiledesc(f, part_index, name.c_str(), &t);
    }
    else if (auto v = py_cast<TimeCode>(object))
    {
        exr_attr_timecode_t t;
        t.time_and_flags = v->timeAndFlags();
        t.user_data = v->userData();
        exr_attr_set_timecode(f, part_index, name.c_str(), &t);
    }
    else if (auto v = py_cast<exr_attr_v2i_t,V2i>(object))
        exr_attr_set_v2i(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v2f_t,V2f>(object))
        exr_attr_set_v2f(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v2d_t,V2d>(object))
        exr_attr_set_v2d(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3i_t,V3i>(object))
        exr_attr_set_v3i(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3f_t,V3f>(object))
        exr_attr_set_v3f(f, part_index, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3d_t,V3d>(object))
        exr_attr_set_v3d(f, part_index, name.c_str(), v);
    else if (py::isinstance<py::str>(object))
        exr_attr_set_string(f, part_index, name.c_str(), std::string(py::str(object)).c_str());
    else if (py_cast<exr_storage_t>(object))
    {
        // The OpenEXRCOre API does not treat storage as an attribute
    }
    else
    {
        std::stringstream s;
        s << "unknown attribute type: " << py::str(object);
        throw std::runtime_error(s.str());
    }
    
}

//
// Add all attributes (required and non-required) to a file. Used
// during writing.
//

void
PyPart::set_attributes(exr_context_t f)
{
    //
    // Initialize the part_index. If there's a "type" attribute in the
    // header, use it over the value provided in the constructor.
    //
    
#if XXX
    if (header.contains("type"))
        type = py::cast<exr_storage_t>(header["type"]);
#endif
    
    exr_result_t result = exr_add_part(f, name.c_str(), type, &part_index);
    if (result != EXR_ERR_SUCCESS) 
        throw std::runtime_error("error writing part");

    //
    // Extract the necessary information from the required header attributes
    //
    // If there's a "compression" attribute in the header, use it over the
    // value provided in the constructor.
    //
#if XXX        
    if (header.contains("compression"))
        compression = py::cast<exr_compression_t>(header["compression"]);
    
#endif
    exr_lineorder_t lineOrder = EXR_LINEORDER_INCREASING_Y;
    if (header.contains("lineOrder"))
        lineOrder = py::cast<exr_lineorder_t>(header["lineOrder"]);
    
    exr_attr_box2i_t dataw;
    dataw.min.x = 0;
    dataw.min.y = 0;
    dataw.max.x = int32_t(width - 1);
    dataw.max.y = int32_t(height - 1);
    if (header.contains("dataWindow"))
    {
        Box2i box = py::cast<Box2i>(header["dataWindow"]);
        dataw.min.x = box.min.x;
        dataw.min.y = box.min.y;
        dataw.max.x = box.max.x;
        dataw.max.y = box.max.y;
    }

    exr_attr_box2i_t dispw = dataw;
    if (header.contains("displayWindow"))
    {
        Box2i box = py::cast<Box2i>(header["displayWindow"]);
        dispw.min.x = box.min.x;
        dispw.min.y = box.min.y;
        dispw.max.x = box.max.x;
        dispw.max.y = box.max.y;
    }

    exr_attr_v2f_t swc;
    swc.x = 0.5f;
    swc.x = 0.5f;
    if (header.contains("screenWindowCenter"))
    {
        V2f v = py::cast<V2f>(header["screenWindowCenter"]);
        swc.x = v.x;
        swc.y = v.y;
    }

    float sww = 1.0f;
    if (header.contains("screenWindowWidth"))
        sww = py::cast<float>(header["screenWindowWidth"]);

    float pixelAspectRatio = 1.0f;
    if (header.contains("pixelAspectRatio"))
        sww = py::cast<float>(header["pixelAspectRatio"]);
        
    result = exr_initialize_required_attr (f, part_index, &dataw, &dispw, 
                                           pixelAspectRatio, &swc, sww,
                                           lineOrder, compression);
    if (result != EXR_ERR_SUCCESS)
        throw std::runtime_error("error writing header");

    //
    // Add the non-required attributes
    //
        
    for (auto a : header)
    {
        auto name = py::str(a.first);
        py::object second = py::cast<py::object>(a.second);
        set_attribute(f, name, second);
    }
}

//
// Add the channels to a file. Used during writing.
//

void
PyPart::add_channels(exr_context_t f)
{
    //
    // The channels must be written in alphabetic order, but the channels
    // py::dict may not be sorted.  Build a sorted vector of channel
    // names and write them in that order.
    //
        
    std::vector<std::string> sorted_channels;
    for (auto c : channels)
        sorted_channels.push_back(py::str(c.first));
    
    std::sort(sorted_channels.begin(), sorted_channels.end());

    //
    // Add the channels in sorted order, and assign the index field
    // based on the sorted order
    
    for (size_t i = 0; i < sorted_channels.size(); ++i)
    {
        auto channel_name = sorted_channels[i];
        PyChannel& C = channels[py::str(channel_name)].cast<PyChannel&>();
        C.channel_index = i;
        
        exr_result_t result = exr_add_channel(f, part_index, C.name.c_str(), C.pixelType(), 
                                              EXR_PERCEPTUALLY_LOGARITHMIC,
                                              C.xSampling, C.ySampling);
        if (result != EXR_ERR_SUCCESS) 
            throw std::runtime_error("error writing channels");
    }
}

void
PyPart::write(exr_context_t f)
{
    switch (type)
    {
      case EXR_STORAGE_SCANLINE:
          write_scanlines(f);
          break;
      case EXR_STORAGE_TILED:
          write_tiles(f);
          break;
      default:
          throw std::runtime_error("invalid storage type");
    }
}
          
void
PyPart::write_scanlines(exr_context_t f)
{
    int32_t scansperchunk = 0;
    exr_result_t result = exr_get_scanlines_per_chunk(f, part_index, &scansperchunk);
    if (result != EXR_ERR_SUCCESS)
        throw std::runtime_error("error writing scanlines per chunk");

    //
    // Get the data window
    //
        
    exr_attr_box2i_t dataw = {0, 0, int32_t(width - 1), int32_t(height - 1)};
    if (header.contains("dataWindow"))
    {
        Box2i box = py::cast<Box2i>(header["dataWindow"]);
        dataw.min.x = box.min.x;
        dataw.min.y = box.min.y;
        dataw.max.x = box.max.x;
        dataw.max.y = box.max.y;
    }

    exr_encode_pipeline_t encoder;
    exr_chunk_info_t cinfo;
    bool first = true;

    for (int16_t y = dataw.min.y; y <= dataw.max.y; y += scansperchunk)
    {
        result = exr_write_scanline_chunk_info(f, part_index, y, &cinfo);
        if (result != EXR_ERR_SUCCESS) 
            throw std::runtime_error("error writing scanline chunk info");

        if (first)
            result = exr_encoding_initialize(f, part_index, &cinfo, &encoder);
        else
            result = exr_encoding_update(f, part_index, &cinfo, &encoder);
        if (result != EXR_ERR_SUCCESS) 
            throw std::runtime_error("error updating encoder");
        
        //
        // Write the channel data.  Sort the channel list here, since the
        // channels are required to be sorted by name, although there's
        // no guarantee that the list of channels provided to the
        // constructor are in sorted order.
        //
            
        for (auto c : channels)
        {
            auto C = py::cast<PyChannel&>(c.second);
            C.set_encoder_channel(encoder, y-dataw.min.y, width, scansperchunk);
        }

        if (first)
        {
            result = exr_encoding_choose_default_routines(f, part_index, &encoder);
            if (result != EXR_ERR_SUCCESS) 
                throw std::runtime_error("error initializing encoder");
        }
            
        result = exr_encoding_run(f, part_index, &encoder);
        if (result != EXR_ERR_SUCCESS)
            throw std::runtime_error("encoder error");
            
        first = false;
    }

    result = exr_encoding_destroy(f, &encoder);
    if (result != EXR_ERR_SUCCESS)
        throw std::runtime_error("error with encoder");
}

void
PyPart::write_tiles(exr_context_t f)
{
    // TODO
    throw std::runtime_error("tiled writing not implemented.");
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
    int num_attributes_a = 0;
    for (auto a : A)
        if (!is_required_attribute(py::str(a.first)))
            num_attributes_a++;

    int num_attributes_b = 0;
    for (auto b : B)
        if (!is_required_attribute(py::str(b.first)))
            num_attributes_b++;

    if (num_attributes_a != num_attributes_b)
        return false;
    
    for (auto a : A)
    {
        if (is_required_attribute(py::str(a.first)))
            continue;
        
        py::object second = py::cast<py::object>(a.second);
        py::object b = B[a.first];
        if (!second.equal(b))
        {
            if (py::isinstance<py::float_>(second))
            {                
                float f = py::cast<py::float_>(second);
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
    if (name == other.name &&
        type == other.type &&  
        width == other.width &&
        height == other.height)
    {
        if (!equal_header(header, other.header))
        {
            diff(__PRETTY_FUNCTION__, __LINE__);
            return false;
        }
        
        //
        // The channel dicts might not be in alphabetical order
        // (they're sorted on write), so don't just compare the dicts
        // directly, compare each entry by key/name.
        //
        
        if (channels.size() != other.channels.size())
        {
            diff(__PRETTY_FUNCTION__, __LINE__);
            return false;
        }
        
        for (auto c : channels)
        {
            auto name = py::str(c.first);
            auto C = py::cast<const PyChannel&>(c.second);
            auto O = py::cast<const PyChannel&>(other.channels[py::str(name)]);
            if (C != O)
            {
                diff(__PRETTY_FUNCTION__, __LINE__);
                std::cout << "channel " << name << " differs." << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    diff(__PRETTY_FUNCTION__, __LINE__);
    std::cout << *this;
    std::cout << "other:" << std::endl;
    std::cout << other;

    return false;
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
                diff(__PRETTY_FUNCTION__, __LINE__);
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
        
        diff(__PRETTY_FUNCTION__, __LINE__);
        std::cout << "this=" << *this
                  << " other=" << other
                  << std::endl;
    }

    diff(__PRETTY_FUNCTION__, __LINE__);

    return false;
}
        

exr_pixel_type_t
PyChannel::pixelType() const
{
    auto buf = pybind11::array::ensure(pixels);
    if (buf)
    {
        if (py::isinstance<py::array_t<uint32_t>>(buf))
            return EXR_PIXEL_UINT;
        if (py::isinstance<py::array_t<half>>(buf))
            return EXR_PIXEL_HALF;      
        if (py::isinstance<py::array_t<float>>(buf))
            return EXR_PIXEL_FLOAT;
    }
    return EXR_PIXEL_LAST_TYPE;
}

void
PyChannel::set_encoder_channel(exr_encode_pipeline_t& encoder, size_t y, size_t width, size_t scansperchunk) const 
{
    py::buffer_info buf = pixels.request();

    auto offset = y * width;

    exr_coding_channel_info_t& channel = encoder.channels[channel_index];

    switch (pixelType())
    {
      case EXR_PIXEL_UINT:
          {
              const uint32_t* pixels = static_cast<const uint32_t*>(buf.ptr);
              channel.encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              channel.user_pixel_stride = sizeof(uint32_t);
          }
          break;
      case EXR_PIXEL_HALF:
          {
              const half* pixels = static_cast<const half*>(buf.ptr);
              channel.encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              channel.user_pixel_stride = sizeof(half);
          }
          break;
      case EXR_PIXEL_FLOAT:
          {
              const float* pixels = static_cast<const float*>(buf.ptr);
              channel.encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              channel.user_pixel_stride = sizeof(float);
          }
          break;
      case EXR_PIXEL_LAST_TYPE:
      default:
          throw std::runtime_error("invalid pixel type");
          break;
    }

    channel.user_line_stride= channel.user_pixel_stride * width / channel.x_samples;
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

    py::enum_<exr_lineorder_t>(m, "LineOrder")
        .value("INCREASING_Y", EXR_LINEORDER_INCREASING_Y)
        .value("DECREASING_Y", EXR_LINEORDER_DECREASING_Y)
        .value("RANDOM_Y", EXR_LINEORDER_RANDOM_Y)
        .value("NUM_LINE_ORDERS", EXR_LINEORDER_LAST_TYPE)
        .export_values();

    py::enum_<exr_pixel_type_t>(m, "PixelType")
        .value("UINT", EXR_PIXEL_UINT)
        .value("HALF", EXR_PIXEL_HALF)
        .value("FLOAT", EXR_PIXEL_FLOAT)
        .value("NUM_PIXEL_TYPES", EXR_PIXEL_LAST_TYPE)
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
        .value("NUM_ENVMAP_TYPES", EXR_ENVMAP_LAST_TYPE)
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
        .def(py::init<int,int>())
        .def(py::init<int,int,bool>())
        .def(py::init<py::array>())
        .def(py::init<py::array,int,int>())
        .def(py::init<py::array,int,int,bool>())
        .def(py::init<const char*,int,int>())
        .def(py::init<const char*,int,int,bool>())
        .def(py::init<const char*,py::array>())
        .def(py::init<const char*,py::array,int,int,bool>())
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
        .def(py::init<const char*,py::dict,py::dict,exr_storage_t,exr_compression_t>(),
             py::arg("name"),
             py::arg("header"),
             py::arg("channels"),
             py::arg("type")=EXR_STORAGE_SCANLINE,
             py::arg("compression")=EXR_COMPRESSION_ZIP)
        .def("__repr__", [](const PyPart& p) { return repr(p); })
        .def(py::self == py::self)
        .def_readwrite("name", &PyPart::name)
        .def_readwrite("type", &PyPart::type)
        .def_readonly("width", &PyPart::width)
        .def_readonly("height", &PyPart::height)
        .def_readwrite("compression", &PyPart::compression)
        .def_readwrite("header", &PyPart::header)
        .def_readwrite("channels", &PyPart::channels)
        .def_readonly("part_index", &PyPart::part_index)
        ;

    py::class_<PyFile>(m, "File")
        .def(py::init<>())
        .def(py::init<std::string>())
        .def(py::init<py::dict,py::dict,exr_storage_t,exr_compression_t>(),
             py::arg("header"),
             py::arg("channels"),
             py::arg("type")=EXR_STORAGE_SCANLINE,
             py::arg("compression")=EXR_COMPRESSION_ZIP)
        .def(py::init<py::list>())
        .def(py::self == py::self)
        .def_readwrite("filename", &PyFile::filename)
        .def_readwrite("parts", &PyFile::parts)
        .def("header", &PyFile::header, py::arg("part_index") = 0)
        .def("channels", &PyFile::channels, py::arg("part_index") = 0)
        .def("write", &PyFile::write)
        .def("RgbaInputFile", &PyFile::RgbaInputFile)
        .def("TiledRgbaInputFile", &PyFile::TiledRgbaInputFile)
        .def("multiPartInputFile", &PyFile::multiPartInputFile)
        .def("multiPartOutputFile", &PyFile::multiPartOutputFile)
        ;
}

