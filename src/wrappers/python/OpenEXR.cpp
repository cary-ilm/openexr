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

//#define DEBUGGIT 1

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

bool
required_attribute(const std::string& name)
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
same_header(const py::dict& A, const py::dict& B)
{
    int num_attributes_a = 0;
    for (auto a = A.begin(); a != A.end(); ++a)
    {
        std::string name = py::cast<std::string>(py::str(a->first));
        if (!required_attribute(name))
            num_attributes_a++;
    }            

    int num_attributes_b = 0;
    for (auto a = B.begin(); a != B.end(); ++a)
    {
        std::string name = py::cast<std::string>(py::str(a->first));
        if (!required_attribute(name))
            num_attributes_b++;
    }

    if (num_attributes_a != num_attributes_b)
    {
        std::cout << "different number of attributes: " << num_attributes_a
                  << " " << num_attributes_b
                  << std::endl;
        return false;
    }
    
    for (auto a = A.begin(); a != A.end(); ++a)
    {
        std::string name = py::cast<std::string>(py::str(a->first));
        if (required_attribute(name))
            continue;
        
        py::object second = py::cast<py::object>(a->second);
        py::object b = B[a->first];
        if (!second.equal(b))
        {
            if (py::isinstance<py::float_>(second))
            {                
                float f = py::cast<py::float_>(second);
                float of = py::cast<py::float_>(b);
                if (equalWithRelError(f, of, 1e-5f))
                    return true;
            }
            
            std::cout << "attribute " << name << " differs: " << py::str(second)
                      << " " << py::str(b)
                      << std::endl;
            return false;
        }
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

    bool operator== (const PyDouble& other) const { return d == other.d; }
    
    double d;
};
                         
class PyPreviewImage
{
public:
    PyPreviewImage() 
    {
    }
    
    static constexpr uint32_t style = py::array::c_style | py::array::forcecast;
    
    PyPreviewImage(unsigned int width, unsigned int height, const PreviewRgba* data = nullptr)
#if XXX
        : pixels(py::array_t<PreviewRgba,style>(std::vector<size_t>(width, height),
                                                std::vector<size_t>(sizeof(PreviewRgba),
                                                                    sizeof(PreviewRgba)),
                                                data))
#endif
    {
        std::cout << "preview image: " << width << "x" << height << std::endl;
        std::vector<size_t> shape, strides;
        shape.assign({ width, height });
        strides.assign({ sizeof(PreviewRgba), sizeof(PreviewRgba) });
        pixels = py::array_t<PreviewRgba,style>(shape, strides, data);
    }
    
    PyPreviewImage(const py::array_t<PreviewRgba>& p)
        : pixels(p)
    {
    }
    
    py::array_t<PreviewRgba> pixels;
};
    
std::ostream&
operator<< (std::ostream& s, const PyPreviewImage& P)
{
    auto width = P.pixels.shape()[0];
    auto height = P.pixels.shape()[1];
    py::buffer_info buf = P.pixels.request();
    const PreviewRgba* rgba = static_cast<PreviewRgba*>(buf.ptr);
    s << "PreviewImage(" << width << ", " << height << "," << std::endl;
    for (ssize_t y=0; y<height; y++)
    {
        for (ssize_t x=0; x<width; x++)
        {
            auto p = rgba[y*width+x];
            s << " (" << int(p.r)
              << "," << int(p.g)
              << "," << int(p.b)
              << "," << int(p.a)
              << ")";
        }
        s << std::endl;
    }
    return s;
}
    
class py_exr_attr_chromaticities_t :  public exr_attr_chromaticities_t 
{
  public:

    bool operator==(const py_exr_attr_chromaticities_t& other) const
        {
            if (red_x == other.red_x &&
                red_y == other.red_y &&
                green_x == other.green_x &&
                green_y == other.green_y &&
                blue_x == other.blue_x &&
                blue_y == other.blue_y &&
                white_x == other.white_x &&
                white_y == other.white_y)
                return true;
            return false;
        }
};

std::ostream&
operator<< (std::ostream& s, const Box2i& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}

std::ostream&
operator<< (std::ostream& s, const Box2f& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}

std::ostream&
operator<< (std::ostream& s, const py_exr_attr_chromaticities_t& c)
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
    
//
// PyChannel holds information for a channel of a PyPart: name, type, x/y
// sampling, and the array of pixel data.
//
  
template <class T>
bool
compare_arrays(const py::buffer_info& a, const py::buffer_info& b)
{
    const T* apixels = static_cast<T*>(a.ptr);
    const T* bpixels = static_cast<T*>(b.ptr);
    for (ssize_t i = 0; i < a.size; i++)
        if (apixels[i] != bpixels[i])
        {
            std::cout << "pixel data differs at i=" << i
                      << ": '" << apixels[i]
                      << "' '" << bpixels[i]
                      << "'" << std::endl;
            return false;
        }
    
    return true;
}

class PyChannel 
{
public:

    PyChannel() : type(EXR_PIXEL_LAST_TYPE), xSampling(0), ySampling(0) {}
    PyChannel(const char* n, exr_pixel_type_t t, int x, int y)
        : name(n), type(t), xSampling(x), ySampling(y) {}
    PyChannel(const char* n, exr_pixel_type_t t, int x, int y, const py::array& p)
        : name(n), type(t), xSampling(x), ySampling(y), pixels(p) {}
    
    bool operator==(const PyChannel& other) const
    {
        if (name != other.name)
        {
            std::cout << "channel name differs: '" << name
                      << "' '" << other.name
                      << "'" << std::endl;
            return false;
        }
        if (type != other.type)
        {
            std::cout << "channel type differs: '" << type
                      << "' '" << other.type
                      << "'" << std::endl;
            return false;
        }
        
        if (xSampling != other.xSampling)
        {
            std::cout << "channel xSampling differs: '" << xSampling
                      << "' '" << other.xSampling
                      << "'" << std::endl;
            return false;
        }
        if (ySampling != other.ySampling)
        {
            std::cout << "channel ySampling differs: '" << ySampling
                      << "' '" << other.ySampling
                      << "'" << std::endl;
            return false;
        }

        if (pixels.ndim() != other.pixels.ndim())
        {
            std::cout << "channel ndim differs: '" << pixels.ndim()
                      << "' '" << other.pixels.ndim()
                      << "'" << std::endl;
            return false;
        }
        
        if (pixels.size() != other.pixels.size())
        {
            std::cout << "channel size differs: '" << pixels.size()
                      << "' '" << other.pixels.size()
                      << "'" << std::endl;
            return false;
        }
        
        py::buffer_info buf = pixels.request();
        py::buffer_info obuf = other.pixels.request();
        switch (type)
        {
        case EXR_PIXEL_UINT:
            return compare_arrays<uint8_t>(buf, obuf);
        case EXR_PIXEL_HALF:
            return compare_arrays<half>(buf, obuf);
        case EXR_PIXEL_FLOAT:
            return compare_arrays<float>(buf, obuf);
        case EXR_PIXEL_LAST_TYPE:
        default:
            throw std::domain_error("invalid pixel type");
            break;
        }
        return false;
    }

    bool operator<(const PyChannel& other) const
        {
            return name < other.name;
        }

    std::string           name;
    exr_pixel_type_t      type;
    int                   xSampling;
    int                   ySampling;
    py::array             pixels;
};
    
//
// PyPart holds the information for a part of an exr file: name, type,
// dimension, compression, the list of attributes (e.g. "header") and the
// list of channels.
//

class PyPart
{
  public:
    PyPart() : type(EXR_STORAGE_LAST_TYPE), compression (EXR_COMPRESSION_LAST_TYPE) {}
    PyPart(const py::dict& a, const py::list& channels,
         exr_storage_t type, exr_compression_t c, const char* name);

    void read_scanline_part (exr_context_t f, int part);
    void read_tiled_part (exr_context_t f, int part);

    const py::dict&    header() const { return attributes; }
    
    bool operator==(const PyPart& other) const
    {
        if (name != other.name)
        {
            std::cout << "part name differs: '" << name
                      << "' '" << other.name
                      << "'" << std::endl;
            return false;
        }
        if (type != other.type)
        {
            std::cout << "part type differs: '" << type
                      << "' '" << other.type
                      << "'" << std::endl;
            return false;
        }
        
        if (width != other.width)
        {
            std::cout << "part width differs: '" << width
                      << "' '" << other.width
                      << "'" << std::endl;
            return false;
        }

        if (height != other.height)
        {
            std::cout << "part height differs: '" << height
                      << "' '" << other.height
                      << "'" << std::endl;
            return false;
        }

        if (!same_header(attributes, other.attributes))
        {
            std::cout << "part attributes differ" << std::endl;
            return false;
        }
        
        if (!channels.equal(other.channels))
        {
            std::cout << "part channels differ: [";
            for (auto c : channels)
            {
                auto C = py::cast<PyChannel&>(*c);
                std::cout << " " << C.name;
            }
            std::cout << " ] [";
            for (auto c : other.channels)
            {
                auto C = py::cast<PyChannel&>(*c);
                std::cout << " " << C.name;
            }
            std::cout << "]" << std::endl;

            return false;
        }

        return true;
    }

    std::string           name;
    exr_storage_t         type;
    uint64_t              width;
    uint64_t              height;
    exr_compression_t     compression;

    py::dict              attributes;
    py::list              channels;
};

//
// PyFile is the object that corresponds to an exr file, either for reading
// or writing, consisting of a simple list of parts.
//

class PyFile 
{
public:
    PyFile(const std::string& filename);
    PyFile(const py::dict& attributes, const py::list& channels,
         exr_storage_t type, exr_compression_t compression);
    PyFile(const py::list& parts);


    py::list        parts() const { return py::cast(_parts); }
    const py::dict& header() const { return _parts[0].header(); }
    py::list        channels() const { return _parts[0].channels(); }
    
    void            write(const char* filename);
    
    bool operator==(const PyFile& other) const
    {
        if (_parts != other._parts)
        {
            std::cout << "file parts differ" << std::endl;
            return false;
        }
        return true;
    }
    
    std::string          _filename;
    std::vector<PyPart>  _parts;
};
    
PyPart::PyPart(const py::dict& attributes_arg, const py::list& channels_arg,
               exr_storage_t type_arg, exr_compression_t compression_arg, const char* name_arg)
    : name(name_arg), type(type_arg), width(0), height(0), compression(compression_arg),
      attributes(attributes_arg), channels(channels_arg)
{
    for (auto c : channels)
    {
        auto o = *c;
        auto C = py::cast<PyChannel>(*o);
        if (C.pixels.ndim() == 2)
        {
            uint32_t w = C.pixels.shape(0);
            uint32_t h = C.pixels.shape(1);

            if (width == 0)
                width = w;
            if (height == 0)
                height = h;
            
            if (w != width)
            {
                std::stringstream s;
                s << "error: bad width " << w << ", expected " << width;
                throw std::runtime_error(s.str());
            }
            if (h != height)
            {
                std::stringstream s;
                s << "error: bad height " << h << ", expected " << height;
                throw std::runtime_error(s.str());
            }                
        }
        else
        {
            throw std::runtime_error("error: channel must have a 2D array");
        }
    }
    
    if (!attributes.contains("dataWindow"))
        attributes["dataWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));

    if (!attributes.contains("displayWindow"))
        attributes["displayWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));
}
    
static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
    const char* fn = "";
#if XXX
    if (EXR_ERR_SUCCESS != exr_get_file_name (f, &fn))
        fn = "<error>";
#endif
    std::stringstream s;
    s << "error " << fn << " " << exr_get_error_code_as_string (code) << " " << msg;
    throw std::runtime_error(s.str());
}

//
// Read scanline data from a file
//

void
PyPart::read_scanline_part (exr_context_t f, int part)
{
    exr_result_t     rv;

    //
    // Get the width,height from the data window
    //

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad data window");

    width = (uint64_t) ((int64_t) datawin.max.x - (int64_t) datawin.min.x + 1);
    height = (uint64_t) ((int64_t) datawin.max.y - (int64_t) datawin.min.y + 1);

    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    int32_t lines_per_chunk;
    rv = exr_get_scanlines_per_chunk (f, part, &lines_per_chunk);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad scanlines per chunk");

#if DEBUGGIT
    std::cout << "Part " << name << " " << width << "x" << height << " lines_per_chunk=" << lines_per_chunk << std::endl;
#endif
    
    //
    // Read the chunks
    //

    for (uint64_t chunk = 0; chunk < height; chunk += lines_per_chunk)
    {
        exr_chunk_info_t cinfo = {0};
        int              y     = ((int) chunk) + datawin.min.y;

        rv = exr_read_scanline_chunk_info (f, part, y, &cinfo);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error("error reading scanline chunk");

        //
        // Initialze the decoder if not already done. This happens only on
        // the first chunk.
        //
        
        if (decoder.channels == NULL)
        {
            rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error ("error initializing decoder");
            
            channels = py::list();
            for (int c = 0; c < decoder.channel_count; c++)
                channels.append(PyChannel());
            
            //
            // Build the channel list for the decoder and allocate each
            // channel's pixel data arrays
            //
            
            auto c_iterator = channels.begin();
            for (int c = 0; c < decoder.channel_count; c++, c_iterator++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];

                outc.decode_to_ptr     = (uint8_t*) 0x1000;
                outc.user_pixel_stride = outc.user_bytes_per_element;
                outc.user_line_stride  = outc.user_pixel_stride * width;

                PyChannel& C = py::cast<PyChannel&>(*c_iterator);
                C.name = outc.channel_name;
                C.type = exr_pixel_type_t(outc.data_type);
                C.xSampling  = outc.x_samples;
                C.ySampling  = outc.y_samples;
                
                std::cout << "read_scanline_part: channel[" << c << "] " << C.name << std::endl;
                
                std::vector<size_t> shape, strides;
                shape.assign({ width, height });

                const auto style = py::array::c_style | py::array::forcecast;
                
                py::array pixels;
                switch (outc.data_type)
                {
                  case EXR_PIXEL_UINT:
                    strides.assign({ sizeof(uint8_t), sizeof(uint8_t) });
                    C.pixels = py::array_t<uint8_t,style>(shape, strides);
                    break;
                  case EXR_PIXEL_HALF:
                    strides.assign({ sizeof(half), sizeof(half) });
                    C.pixels = py::array_t<half,style>(shape, strides);
                    break;
                  case EXR_PIXEL_FLOAT:
                    strides.assign({ sizeof(float), sizeof(float) });
                    C.pixels = py::array_t<float,style>(shape, strides);
                    break;
                  case EXR_PIXEL_LAST_TYPE:
                  default:
                      throw std::domain_error("invalid pixel type");
                      break;
                }
            }

            rv = exr_decoding_choose_default_routines (f, part, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("error initialing decoder");
        }
        else
        {
            rv = exr_decoding_update (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("error updating decoder");
        }

        if (cinfo.type != EXR_STORAGE_DEEP_SCANLINE)
        {
            auto c_iterator = channels.begin();
            for (int16_t c = 0; c < decoder.channel_count; c++, c_iterator++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];

                //
                // Set the decoder data pointer to the appropriate offset
                // into the pixel arrays
                //
                
                PyChannel& C = py::cast<PyChannel&>(*c_iterator);
                py::buffer_info buf = C.pixels.request();
                switch (outc.data_type)
                {
                  case EXR_PIXEL_UINT:
                      {
                          uint8_t* pixels = static_cast<uint8_t*>(buf.ptr);
                          outc.decode_to_ptr = &pixels[y*width];
                      }
                      break;
                  case EXR_PIXEL_HALF:
                      {
                          half* pixels = static_cast<half*>(buf.ptr);
                          outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[y*width]);
                      }
                      break;
                  case EXR_PIXEL_FLOAT:
                      {
                          float* pixels = static_cast<float*>(buf.ptr);
                          outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[y*width]);
                      }
                      break;
                  case EXR_PIXEL_LAST_TYPE:
                  default:
                      throw std::domain_error("invalid pixel type");
                      break;
                }
                outc.user_pixel_stride = outc.user_bytes_per_element;
                outc.user_line_stride  = outc.user_pixel_stride * width;
            }
        }

        
        rv = exr_decoding_run (f, part, &decoder);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error("error in decoder");
    }

    exr_decoding_destroy (f, &decoder);
}

void
PyPart::read_tiled_part (exr_context_t f, int part)
{
    exr_result_t rv;

    //
    // Get the width,height from the data window
    //

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad data window");

    width = datawin.max.x - datawin.min.x + 1;
    height = datawin.max.y - datawin.min.y + 1;

    uint32_t              txsz, tysz;
    exr_tile_level_mode_t levelmode;
    exr_tile_round_mode_t roundingmode;

    rv = exr_get_tile_descriptor (f, part, &txsz, &tysz, &levelmode, &roundingmode);
    if (rv != EXR_ERR_SUCCESS)
        throw std::runtime_error("bad tile descriptor");

    int32_t levelsx, levelsy;
    rv = exr_get_tile_levels (f, part, &levelsx, &levelsy);
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
            rv = exr_get_level_sizes (f, part, xlevel, ylevel, &levw, &levh);
            if (rv != EXR_ERR_SUCCESS)
                throw std::runtime_error("bad level sizes");

            int32_t curtw, curth;
            rv = exr_get_tile_sizes (f, part, xlevel, ylevel, &curtw, &curth);
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
            ty = 0;
            for (int64_t cury = 0; cury < levh; cury += curth, ++ty)
            {
                tx = 0;
                for (int64_t curx = 0; curx < levw; curx += curtw, ++tx)
                {
                    rv = exr_read_tile_chunk_info (f, part, tx, ty, xlevel, ylevel, &cinfo);
                    if (rv != EXR_ERR_SUCCESS)
                        throw std::runtime_error("error reading tile chunk");

                    if (decoder.channels == NULL)
                    {
                        rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error initializing decoder");

                        channels = py::list();
                        for (int c = 0; c < decoder.channel_count; c++)
                            channels.append(PyChannel());

                        //
                        // Build the channel list for the decoder and allocate each
                        // channel's pixel data arrays
                        //
            
                        auto c_iterator = channels.begin();
                        for (int c = 0; c < decoder.channel_count; c++, c_iterator++)
                        {
                            exr_coding_channel_info_t& outc = decoder.channels[c];
                            // fake addr for default routines
                            outc.decode_to_ptr = (uint8_t*) 0x1000;
                            outc.user_pixel_stride = outc.user_bytes_per_element;
                            outc.user_line_stride = outc.user_pixel_stride * curtw;

                            PyChannel& C = py::cast<PyChannel&>(*c_iterator);
                            C.name = outc.channel_name;
                            C.type = exr_pixel_type_t(outc.data_type);

                            std::vector<size_t> shape, strides;
                            shape.assign({ width, height });

                            const auto style = py::array::c_style | py::array::forcecast;
                
                            switch (outc.data_type)
                            {
                              case EXR_PIXEL_UINT:
                                  strides.assign({ sizeof(uint8_t), sizeof(uint8_t) });
                                  C.pixels = py::array_t<uint8_t,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_HALF:
                                  strides.assign({ sizeof(half), sizeof(half) });
                                  C.pixels = py::array_t<half,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_FLOAT:
                                  strides.assign({ sizeof(float), sizeof(float) });
                                  C.pixels = py::array_t<float,style>(shape, strides);
                                  break;
                              case EXR_PIXEL_LAST_TYPE:
                              default:      
                                  throw std::domain_error("invalid pixel type");
                                  break;
                            }
                        }

                        rv = exr_decoding_choose_default_routines (f, part, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error initializing decoder");
                    }
                    else
                    {
                        rv = exr_decoding_update (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                            throw std::runtime_error("error updating decoder");
                    }

                    if (cinfo.type != EXR_STORAGE_DEEP_TILED)
                    {
                        auto c_iterator = channels.begin();
                        for (int c = 0; c < decoder.channel_count; c++, c_iterator++)
                        {
                            exr_coding_channel_info_t& outc = decoder.channels[c];
                            outc.user_pixel_stride = outc.user_bytes_per_element;
                            outc.user_line_stride = outc.user_pixel_stride * curtw;

                            //
                            // Set the decoder data pointer to the appropriate offset
                            // into the pixel arrays
                            //
                
                            PyChannel& C = py::cast<PyChannel&>(*c_iterator);
                            py::buffer_info buf = C.pixels.request();
                            switch (outc.data_type)
                            {
                              case EXR_PIXEL_UINT:
                                  {
                                      uint8_t* pixels = static_cast<uint8_t*>(buf.ptr);
                                      outc.decode_to_ptr = &pixels[cury*width];
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

                    rv = exr_decoding_run (f, part, &decoder);
                    if (rv != EXR_ERR_SUCCESS)
                        throw std::runtime_error("error in decoder");
                }
            }

            exr_decoding_destroy (f, &decoder);
        }
    }
}

//
// Read an attribute from the file at the given index and create a
// corresponding python object.
//

py::object
get_attribute(exr_context_t f, int32_t p, int32_t a, std::string& name) 
{
    exr_result_t              rv;

    const exr_attribute_t* attr;
    rv = exr_get_attribute_by_index(f, p, EXR_ATTR_LIST_FILE_ORDER, a, &attr);
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
                  l.append(py::cast(PyChannel(e.name.str, e.pixel_type, e.x_sampling, e.y_sampling)));
              }
              return l;
          }
      case EXR_ATTR_CHROMATICITIES:
          {
              py_exr_attr_chromaticities_t c = {
                  attr->chromaticities->red_x,
                  attr->chromaticities->red_y,
                  attr->chromaticities->green_x,
                  attr->chromaticities->green_y,
                  attr->chromaticities->blue_x,
                  attr->chromaticities->blue_y,
                  attr->chromaticities->white_x,
                  attr->chromaticities->white_y
              };
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
          return py::str(attr->string->str);
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
          return py::cast( TimeCode(attr->timecode->time_and_flags,
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
// Create a PyFile out of a list of parts (i.e. a multi-part file)
//

PyFile::PyFile(const py::list& parts)
{
    for (auto p : parts)
        _parts.push_back(py::cast<PyPart>(*p));
}

//
// Create a PyFile out of a single part: attributes (i.e. header), channels,
// type, and compression (i.e. a single-part file)
//

PyFile::PyFile(const py::dict& attributes, const py::list& channels,
               exr_storage_t type, exr_compression_t compression)
{
    _parts.push_back(PyPart(attributes, channels, type, compression, "Part0"));
}

//
// Read a PyFile from the given filename
//

PyFile::PyFile(const std::string& filename)
    : _filename (filename)
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

    _parts.resize(numparts);
    
    for (int p = 0; p < numparts; ++p)
    {
        //
        // Read the attributes into the header
        //
        
        py::dict& h = _parts[p].attributes;

        int32_t attrcount;
        rv = exr_get_attribute_count(f, p, &attrcount);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error ("read error");

        for (int32_t a = 0; a < attrcount; ++a)
        {
            std::string name;
            py::object attr = get_attribute(f, p, a, name);
            h[name.c_str()] = attr;
            if (name == "name")
            {
                _parts[p].name = py::str(attr);
            }
                
            std::cout << "File::File header[" << name << "]" << std::endl;
        }

        //
        // Read the type (i.e. scanline, tiled, deep, etc)
        //
        
        exr_storage_t store;
        rv = exr_get_storage (f, p, &store);
        if (rv != EXR_ERR_SUCCESS)
            return;

        _parts[p].type = store;
            
        //
        // Read the compression type
        //
        
        exr_compression_t compression;
        rv = exr_get_compression(f, p, &compression);
        if (rv != EXR_ERR_SUCCESS)
            return;
        
        _parts[p].compression = compression;

        //
        // Read the part
        //
        
        if (store == EXR_STORAGE_SCANLINE || store == EXR_STORAGE_DEEP_SCANLINE)
            _parts[p].read_scanline_part (f, p);
        else if (store == EXR_STORAGE_TILED || store == EXR_STORAGE_DEEP_TILED)
             _parts[p].read_tiled_part (f, p);
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

void
write_attribute(exr_context_t f, int p, const std::string& name, py::object object)
{
#if DEBUGGIT
    std::cout << "write attribute " << name << std::endl;
#endif
    
    if (auto v = py_cast<exr_attr_box2i_t,Box2i>(object))
        exr_attr_set_box2i(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_box2f_t,Box2f>(object))
        exr_attr_set_box2f(f, p, name.c_str(), v);
    else if (py::isinstance<py::list>(object))
    {
        auto list = py::cast<py::list>(object);
        auto size = list.size();
        if (size == 0)
        {
            throw std::runtime_error("invalid empty list is header: can't deduce attribute type");
        }
        else if (py::isinstance<py::float_>(list[0]))
        {
            // float vector
            std::vector<float> v = list.cast<std::vector<float>>();
            exr_attr_set_float_vector(f, p, name.c_str(), v.size(), &v[0]);
        }
        else if (py::isinstance<py::str>(list[0]))
        {
            // string vector
            std::vector<std::string> strings = list.cast<std::vector<std::string>>();
            std::vector<const char*> v;
            for (size_t i=0; i<strings.size(); i++)
                v.push_back(strings[i].c_str());
            exr_attr_set_string_vector(f, p, name.c_str(), v.size(), &v[0]);
        }
        else if (py::isinstance<PyChannel>(list[0]))
        {
#if XXX
            // channel list
            std::vector<PyChannel> C = list.cast<std::vector<PyChannel>>();
            std::vector<exr_attr_chlist_entry_t> v(C.size());
            for (size_t i = 0; i<C.size(); i++)
            {
                v[i].name.length = C[i].name.size();
                v[i].name.alloc_size = C[i].name.size();
                v[i].name.str = C[i].name.c_str();
                v[i].pixel_type = C[i].type;
                v[i].p_linear = 0;
                v[i].reserved[0] = 0;
                v[i].reserved[1] = 0;
                v[i].reserved[2] = 0;
                v[i].x_sampling = C[i].xSampling;
                v[i].y_sampling = C[i].ySampling;
            }
            exr_attr_chlist_t channels;
            channels.num_channels = v.size();
            channels.num_alloced = v.size();
            channels.entries = &v[0];
            exr_attr_set_channels(f, p, name.c_str(), &channels);
#endif
        }
    }
    else if (auto o = py_cast<py_exr_attr_chromaticities_t>(object))
        exr_attr_set_chromaticities(f, p, name.c_str(), o);
    else if (auto o = py_cast<exr_compression_t>(object))
        exr_attr_set_compression(f, p, name.c_str(), *o);
    else if (auto o = py_cast<exr_envmap_t>(object))
        exr_attr_set_envmap(f, p, name.c_str(), *o);
    else if (py::isinstance<py::float_>(object))
    {
        const float o = py::cast<py::float_>(object);
        exr_attr_set_float(f, p, name.c_str(), o);
    }
    else if (py::isinstance<PyDouble>(object))
    {
        const PyDouble d = py::cast<PyDouble>(object);
        exr_attr_set_double(f, p, name.c_str(), d.d);
    }
    else if (py::isinstance<py::int_>(object))
    {
        const int o = py::cast<py::int_>(object);
        exr_attr_set_int(f, p, name.c_str(), o);
        return;
    }
    else if (auto o = py_cast<exr_attr_keycode_t,KeyCode>(object))
        exr_attr_set_keycode(f, p, name.c_str(), o);
    else if (auto o = py_cast<exr_lineorder_t>(object))
        exr_attr_set_lineorder(f, p, name.c_str(), *o);
    else if (auto v = py_cast<exr_attr_m33f_t,M33f>(object))
        exr_attr_set_m33f(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m33d_t,M33d>(object))
        exr_attr_set_m33d(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m44f_t,M44f>(object))
        exr_attr_set_m44f(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_m44d_t,M44d>(object))
        exr_attr_set_m44d(f, p, name.c_str(), v);
    else if (auto v = py_cast<PyPreviewImage>(object))
    {
        auto shape = v->pixels.shape();
        
        exr_attr_preview_t o;
        o.width = shape[0];
        o.height = shape[1];
        o.alloc_size = o.width * o.height * 4;
        py::buffer_info buf = v->pixels.request();
        o.rgba = static_cast<uint8_t*>(buf.ptr);
        exr_attr_set_preview(f, p, name.c_str(), &o);
    }
    else if (auto o = py_cast<exr_attr_rational_t,Rational>(object))
        exr_attr_set_rational(f, p, name.c_str(), o);
    else if (py::isinstance<py::str>(object))
    {
        const std::string& s = py::cast<py::str>(object);
        exr_attr_set_string(f, p, name.c_str(), s.c_str());
    }
    else if (auto v = py_cast<TileDescription>(object))
    {
        exr_attr_tiledesc_t t;
        t.x_size = v->xSize;
        t.y_size = v->ySize;
        t.level_and_round = EXR_PACK_TILE_LEVEL_ROUND (v->mode, v->roundingMode);
        exr_attr_set_tiledesc(f, p, name.c_str(), &t);
    }
    else if (auto v = py_cast<TimeCode>(object))
    {
        exr_attr_timecode_t t;
        t.time_and_flags = v->timeAndFlags();
        t.user_data = v->userData();
        exr_attr_set_timecode(f, p, name.c_str(), &t);
    }
    else if (auto v = py_cast<exr_attr_v2i_t,V2i>(object))
        exr_attr_set_v2i(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v2f_t,V2f>(object))
        exr_attr_set_v2f(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v2d_t,V2d>(object))
        exr_attr_set_v2d(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3i_t,V3i>(object))
        exr_attr_set_v3i(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3f_t,V3f>(object))
        exr_attr_set_v3f(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_v3d_t,V3d>(object))
        exr_attr_set_v3d(f, p, name.c_str(), v);
    else
        throw std::runtime_error("unknown attribute type");
}

//
// Write the PyFile to the given filename
//

void
PyFile::write(const char* filename)
{
    //
    // Open the file for writing
    //

    exr_context_t f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    exr_result_t result = exr_start_write(&f, filename, EXR_WRITE_FILE_DIRECTLY, &cinit);
    if (result != EXR_ERR_SUCCESS)
    {
        std::stringstream s;
        s << "can't open " << filename << " for write";
        throw std::runtime_error(s.str());
    }

    exr_set_longname_support(f, 1);

    //
    // Write the parts
    //
    
    for (size_t p=0; p<_parts.size(); p++)
    {
#if DEBUGGIT
        std::cout << "write part " << p << std::endl;
#endif
        const PyPart& P = _parts[p];
            
        int part_index;
        result = exr_add_part(f, P.name.c_str(), P.type, &part_index);
        if (result != EXR_ERR_SUCCESS) 
            throw std::runtime_error("error writing part");

        //
        // Extract the necessary information from the required header attributes
        //
        
        exr_lineorder_t lineOrder = EXR_LINEORDER_INCREASING_Y;
        if (P.attributes.contains("lineOrder"))
            lineOrder = py::cast<exr_lineorder_t>(P.attributes["lineOrder"]);

        exr_compression_t compression = EXR_COMPRESSION_NONE;
        if (P.attributes.contains("compression"))
            compression = py::cast<exr_compression_t>(P.attributes["compression"]);
        
        exr_attr_box2i_t dataw;
        dataw.min.x = 0;
        dataw.min.y = 0;
        dataw.max.x = int32_t(P.width - 1);
        dataw.max.x = int32_t(P.height - 1);
        if (P.attributes.contains("dataWindow"))
        {
            Box2i box = py::cast<Box2i>(P.attributes["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
        }

        exr_attr_box2i_t dispw = dataw;
        if (P.attributes.contains("displayWindow"))
        {
            Box2i box = py::cast<Box2i>(P.attributes["displayWindow"]);
            dispw.min.x = box.min.x;
            dispw.min.y = box.min.y;
            dispw.max.x = box.max.x;
            dispw.max.y = box.max.y;
        }

        exr_attr_v2f_t swc;
        swc.x = 0.5f;
        swc.x = 0.5f;
        if (P.attributes.contains("screenWindowCenter"))
        {
            V2f v = py::cast<V2f>(P.attributes["screenWindowCenter"]);
            swc.x = v.x;
            swc.y = v.y;
        }

        float sww = 1.0f;
        if (P.attributes.contains("screenWindowWidth"))
            sww = py::cast<float>(P.attributes["screenWindowWidth"]);

        float pixelAspectRatio = 1.0f;
        if (P.attributes.contains("pixelAspectRatio"))
            sww = py::cast<float>(P.attributes["pixelAspectRatio"]);
        
        result = exr_initialize_required_attr (f, p, &dataw, &dispw, 
                                               pixelAspectRatio, &swc, sww,
                                               lineOrder, compression);
        if (result != EXR_ERR_SUCCESS)
            throw std::runtime_error("error writing header");

        //
        // Add the attributes
        //
        
        for (auto a = P.attributes.begin(); a != P.attributes.end(); ++a)
        {
            auto v = *a;
            auto first = v.first;
            std::string name = py::cast<std::string>(py::str(first));
            py::object second = py::cast<py::object>(v.second);
            write_attribute(f, p, name, second);
        }

        std::vector<PyChannel> channels;
        for (auto c_iterator : P.channels)
            channels.push_back(py::cast<PyChannel&>(*c_iterator));
        std::sort(channels.begin(), channels.end());
        for (size_t c=0; c<channels.size(); c++)
        {
            const PyChannel& C = channels[c];
            result = exr_add_channel(f, p, C.name.c_str(), C.type, 
                                     EXR_PERCEPTUALLY_LOGARITHMIC,
                                     C.xSampling, C.ySampling);
            if (result != EXR_ERR_SUCCESS) 
                throw std::runtime_error("error writing channels");

            std::cout << "write: exr_add_channel " << C.name << std::endl;
        }

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

    exr_encode_pipeline_t encoder;

    //
    // Write the parts
    //
    
    for (size_t p=0; p<_parts.size(); p++)
    {
        const PyPart& P = _parts[p];
            
        exr_chunk_info_t cinfo;

        int32_t scansperchunk = 0;
        exr_get_scanlines_per_chunk(f, p, &scansperchunk);
        if (result != EXR_ERR_SUCCESS)
            throw std::runtime_error("error writing scanlines per chunk");

        bool first = true;

        //
        // Get the data window
        //
        
        exr_attr_box2i_t dataw = {0, 0, int32_t(P.width - 1), int32_t(P.height - 1)};
        if (P.attributes.contains("dataWindow"))
        {
            Box2i box = py::cast<Box2i>(P.attributes["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
        }

        for (int16_t y = dataw.min.y; y <= dataw.max.y; y += scansperchunk)
        {
            result = exr_write_scanline_chunk_info(f, p, y, &cinfo);
            if (result != EXR_ERR_SUCCESS) 
                throw std::runtime_error("error writing scanline chunk info");

            if (first)
                result = exr_encoding_initialize(f, p, &cinfo, &encoder);
            else
                result = exr_encoding_update(f, p, &cinfo, &encoder);
            if (result != EXR_ERR_SUCCESS) 
                throw std::runtime_error("error updating encoder");
        
            int channelCount = P.channels.size();
            
            //
            // Write the channel data
            //
            
            std::vector<PyChannel> channels;
            for (auto c_iterator : P.channels)
                channels.push_back(py::cast<PyChannel&>(*c_iterator));
            std::sort(channels.begin(), channels.end());

            auto c_iterator = P.channels.begin();
            for (int c=0; c<channelCount; c++, c_iterator++)
            {
#if XXX
                const PyChannel& C = py::cast<PyChannel&>(*c_iterator);
#endif
                const PyChannel& C = channels[c];
                
                encoder.channel_count = channelCount;
                py::buffer_info buf = C.pixels.request();
                switch (C.type)
                {
                  case EXR_PIXEL_UINT:
                      {
                          const uint8_t* pixels = static_cast<const uint8_t*>(buf.ptr);
                          encoder.channels[c].encode_from_ptr = &pixels[y*P.width];
                          encoder.channels[c].user_pixel_stride = sizeof(uint8_t);
                      }
                      break;
                  case EXR_PIXEL_HALF:
                      {
                          const half* pixels = static_cast<const half*>(buf.ptr);
                          encoder.channels[c].encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[y*P.width]);
                          encoder.channels[c].user_pixel_stride = sizeof(half);
                      }
                      break;
                  case EXR_PIXEL_FLOAT:
                      {
                          const float* pixels = static_cast<const float*>(buf.ptr);
                          encoder.channels[c].encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[y*P.width]);
                          encoder.channels[c].user_pixel_stride = sizeof(float);
                      }
                      break;
                  case EXR_PIXEL_LAST_TYPE:
                  default:
                      throw std::runtime_error("invalid pixel type");
                      break;
                }
                
                encoder.channels[c].user_line_stride  = encoder.channels[c].user_pixel_stride * P.width;
                encoder.channels[c].height            = scansperchunk; // chunk height
                encoder.channels[c].width             = dataw.max.x - dataw.min.y + 1;
            }

            if (first)
            {
                result = exr_encoding_choose_default_routines(f, p, &encoder);
                if (result != EXR_ERR_SUCCESS) 
                    throw std::runtime_error("error initializing encoder");
            }
            
            result = exr_encoding_run(f, p, &encoder);
            if (result != EXR_ERR_SUCCESS)
                throw std::runtime_error("encoder error");
            
            first = false;
        }
    }

    result = exr_encoding_destroy(f, &encoder);
    if (result != EXR_ERR_SUCCESS)
        throw std::runtime_error("error with encoder");

    result = exr_finish(&f);

    _filename = filename;
}

bool
write_exr_file_parts(const char* filename, const py::list& parts)
{
    return true;
}

bool
write_exr_file(const char* filename, const py::dict& attributes, const py::list& channels)
{
    return true;
}

std::ostream&
operator<< (std::ostream& s, const PyChannel& C)
{
    return s << "Channel(\"" << C.name 
             << "\", type=" << py::cast(C.type) 
             << ", xSampling=" << C.xSampling 
             << ", ySampling=" << C.ySampling
             << ")";
}

std::ostream&
operator<< (std::ostream& s, const PyPart& P)
{
    return s << "Part(\"" << P.name 
             << "\", type=" << py::cast(P.type) 
             << ", width=" << P.width
             << ", height=" << P.height
             << ", compression=" << P.compression
             << ")";
}

std::ostream&
operator<< (std::ostream& s, const TileDescription& v)
{
    s << "TileDescription(" << v.xSize
      << ", " << v.ySize
      << ", " << py::cast(v.mode)
      << ", " << py::cast(v.roundingMode)
      << ")";

    return s;
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

    py::class_<TileDescription>(m, "TileDescription")
        .def(py::init())
        .def("__repr__", [](TileDescription& v) { return repr(v); })
        .def_readwrite("xSize", &TileDescription::xSize)
        .def_readwrite("ySize", &TileDescription::ySize)
        .def_readwrite("mode", &TileDescription::mode)
        .def_readwrite("roundingMode", &TileDescription::roundingMode)
        ;       

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

    py::class_<Rational>(m, "Rational")
        .def(py::init())
        .def(py::init<int,unsigned int>())
        .def(py::self == py::self)
        .def_readwrite("n", &Rational::n)
        .def_readwrite("d", &Rational::d)
        ;
    
    py::class_<KeyCode>(m, "KeyCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int>())
        .def(py::self == py::self)
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

    py::class_<py_exr_attr_chromaticities_t>(m, "Chromaticities")
        .def(py::init<float,float,float,float,float,float,float,float>())
        .def(py::self == py::self)
        .def("__repr__", [](const py_exr_attr_chromaticities_t& v) { return repr(v); })
        .def_readwrite("red_x", &py_exr_attr_chromaticities_t::red_x)
        .def_readwrite("red_y", &py_exr_attr_chromaticities_t::red_y)
        .def_readwrite("green_x", &py_exr_attr_chromaticities_t::green_x)
        .def_readwrite("green_y", &py_exr_attr_chromaticities_t::green_y)
        .def_readwrite("blue_x", &py_exr_attr_chromaticities_t::blue_x)
        .def_readwrite("blue_y", &py_exr_attr_chromaticities_t::blue_y)
        .def_readwrite("white_x", &py_exr_attr_chromaticities_t::white_x)
        .def_readwrite("white_y", &py_exr_attr_chromaticities_t::white_y)
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
        .def_readwrite("pixels", &PyPreviewImage::pixels)
        ;
    
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
#if XXX
        .def("__repr__", [](const Box2i& b) {
            std::stringstream s;
            s << "(" << b.min << " " << b.max << ")";
            return s.str();
        })
#endif
        .def(py::self == py::self)
        .def_readwrite("min", &Box2i::min)
        .def_readwrite("max", &Box2i::max)
        ;
    
    py::class_<Box2f>(m, "Box2f")
        .def(py::init())
        .def(py::init<V2f,V2f>())
        .def("__repr__", [](const Box2f& v) { return repr(v); })
#if XXX
        .def("__repr__", [](const Box2f& b) {
            std::stringstream s;
            s << "(" << b.min << " " << b.max << ")";
            return s.str();
        })
#endif
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
    
    py::class_<PyDouble>(m, "Double")
        .def(py::init<double>())
        .def("__repr__", [](const PyDouble& d) { return repr(d.d); })
        .def(py::self == py::self)
        ;

    py::class_<PyChannel>(m, "Channel")
        .def(py::init())
        .def(py::init<const char*,exr_pixel_type_t,int,int>())
        .def(py::init<const char*,exr_pixel_type_t,int,int,py::array>())
        .def("__repr__", [](const PyChannel& c) { return repr(c); })
        .def(py::self == py::self)
        .def_readwrite("name", &PyChannel::name)
        .def_readwrite("type", &PyChannel::type)
        .def_readwrite("xSampling", &PyChannel::xSampling)
        .def_readwrite("ySampling", &PyChannel::ySampling)
        .def_readwrite("pixels", &PyChannel::pixels)
        ;
    
    py::class_<PyPart>(m, "Part")
        .def(py::init())
        .def(py::init<py::dict,py::list,exr_storage_t,exr_compression_t,const char*>())
        .def("__repr__", [](const PyPart& p) { return repr(p); })
        .def(py::self == py::self)
        .def_readwrite("name", &PyPart::name)
        .def_readwrite("type", &PyPart::type)
        .def_readwrite("width", &PyPart::width)
        .def_readwrite("height", &PyPart::height)
        .def_readwrite("compression", &PyPart::compression)
        .def("header", &PyPart::header)
        .def_readwrite("attributes", &PyPart::attributes)
        .def_readwrite("channels", &PyPart::channels)
        ;

    py::class_<PyFile>(m, "File")
        .def(py::init<std::string>())
        .def(py::init<py::dict,py::list,exr_storage_t,exr_compression_t>())
        .def(py::init<py::list>())
        .def(py::self == py::self)
        .def("parts", &PyFile::parts)
        .def("header", &PyFile::header)
        .def("channels", &PyFile::channels)
        .def("write", &PyFile::write)
        .def_readwrite("filename", &PyFile::_filename)
        ;

    m.def("write_exr_file", &write_exr_file);
    m.def("write_exr_file_parts", &write_exr_file_parts);
}

