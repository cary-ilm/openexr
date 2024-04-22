//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/eval.h>

#include "openexr.h"

#include <ImfTimeCodeAttribute.h>
#include <ImfTileDescription.h>
#include <ImfRational.h>
#include <ImfKeyCode.h>
#include <ImfPreviewImage.h>

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>

#include <typeinfo>

//#define DEBUGGIT 1

namespace py = pybind11;
using namespace py::literals;

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

extern bool init_OpenEXR_old(PyObject* module);

namespace pybind11 {
namespace detail {

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

class PyDouble
{
public:
    PyDouble(double x) : d(x)  {}

    double d;
};
                         
class PyChannel 
{
public:

    PyChannel() : type(EXR_PIXEL_LAST_TYPE), xsamples(0), ysamples(0) {}
    PyChannel(const char* n, exr_pixel_type_t t, int x, int y)
        : name(n), type(t), xsamples(x), ysamples(y) {}
    PyChannel(const char* n, exr_pixel_type_t t, int x, int y, const py::array& p)
        : name(n), type(t), xsamples(x), ysamples(y), pixels(p) {}
    
    std::string           name;
    exr_pixel_type_t      type;
    int                   xsamples;
    int                   ysamples;
    py::array             pixels;
};
    
std::ostream&
operator<< (std::ostream& s, const PyChannel& C)
{
    return s << "Channel(\"" << C.name 
             << "\", type=" << py::cast(C.type) 
             << ", xsamples=" << C.xsamples 
             << ", ysamples=" << C.ysamples
             << ")";
}

class PyPart
{
  public:
    PyPart() : type(EXR_STORAGE_LAST_TYPE), compression (EXR_COMPRESSION_LAST_TYPE) {}
    PyPart(const py::dict& a, const py::list& channels,
         exr_storage_t type, exr_compression_t c, const char* name);

    const py::dict&    header() const { return _header; }
    py::list           channels() const { return py::cast(_channels); }

    std::string           name;
    exr_storage_t         type;
    uint64_t              width;
    uint64_t              height;
    exr_compression_t     compression;

    py::dict              _header;
    std::vector<PyChannel>  _channels;
};

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
    
    exr_result_t    write(const char* filename);
    
    std::vector<PyPart>  _parts;
};
    
PyPart::PyPart(const py::dict& a, const py::list& channels,
           exr_storage_t t, exr_compression_t c, const char* n)
    : name(n), type(t), width(0), height(0), compression(c), _header(a)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    
    for (auto c : channels)
    {
        auto o = *c;
        auto C = py::cast<PyChannel>(*o);
        _channels.push_back(C);

        if (C.pixels.ndim() == 2)
        {
            uint32_t w = C.pixels.shape(0);
            uint32_t h = C.pixels.shape(1);

            std::cout << "channel " << C.name << " " << w << " x " << h << std::endl;

            if (width == 0)
                width = w;
            if (height == 0)
                height = h;
            
            if (w != width)
                std::cout << "ERROR: bad width " << w << ", expected " << width << std::endl;
            if (h != height)
                std::cout << "ERROR: bad height " << h << ", expected " << height << std::endl;
        }
        else
        {
            std::cout << "ERROR: expected 2D array" << std::endl;
        }
    }

    if (!_header.contains("dataWindow"))
    {
        _header["dataWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));
    }

    if (!_header.contains("displayWindow"))
    {
        _header["displayWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));
    }
}
    
static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
#if XXX
    const char* fn;
    if (EXR_ERR_SUCCESS != exr_get_file_name (f, &fn))
        fn = "<error>";
    std::cout << "ERROR " << fn << " " << exr_get_error_code_as_string (code) << " " << msg << std::endl;
#endif
    std::cout << "ERROR " << exr_get_error_code_as_string (code) << " " << msg << std::endl;
}

bool
read_scanline_part (exr_context_t f, int part, PyPart& P)
{
    exr_result_t     rv, frv;
    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS)
        return false;

    uint64_t width =
        (uint64_t) ((int64_t) datawin.max.x - (int64_t) datawin.min.x + 1);
    uint64_t height =
        (uint64_t) ((int64_t) datawin.max.y - (int64_t) datawin.min.y + 1);

    P.width = width;
    P.height = height;
    
    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    int32_t lines_per_chunk;
    rv = exr_get_scanlines_per_chunk (f, part, &lines_per_chunk);
    if (rv != EXR_ERR_SUCCESS)
        return false;

    frv = rv;

#if DEBUGGIT
    std::cout << "Part " << P.name << " " << width << "x" << height << " lines_per_chunk=" << lines_per_chunk << std::endl;
#endif
    

    for (uint64_t chunk = 0; chunk < height; chunk += lines_per_chunk)
    {
        exr_chunk_info_t cinfo = {0};
        int              y     = ((int) chunk) + datawin.min.y;

        rv = exr_read_scanline_chunk_info (f, part, y, &cinfo);
        if (rv != EXR_ERR_SUCCESS)
        {
            std::cout << "error " << __LINE__ << std::endl;
            return false;
        }

        if (decoder.channels == NULL)
        {
            rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
            {
                std::cout << "error " << __LINE__ << std::endl;
                return false;
            }
            
            P._channels.resize(decoder.channel_count);

            for (int c = 0; c < decoder.channel_count; c++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];

                outc.decode_to_ptr     = (uint8_t*) 0x1000;
                outc.user_pixel_stride = outc.user_bytes_per_element;
                outc.user_line_stride  = outc.user_pixel_stride * width;

                P._channels[c].name = outc.channel_name;
                P._channels[c].type = exr_pixel_type_t(outc.data_type);
                P._channels[c].xsamples  = outc.x_samples;
                P._channels[c].ysamples  = outc.y_samples;

                std::vector<size_t> shape, strides;
                shape.assign({ width, height });

                const auto style = py::array::c_style | py::array::forcecast;
                
                switch (outc.data_type)
                {
                case EXR_PIXEL_UINT:
                    strides.assign({ sizeof(uint8_t), sizeof(uint8_t) });
                    P._channels[c].pixels = py::array_t<uint8_t,style>(shape, strides);
                    break;
                case EXR_PIXEL_HALF:
                    strides.assign({ sizeof(half), sizeof(half) });
                    P._channels[c].pixels = py::array_t<half,style>(shape, strides);
                    break;
                case EXR_PIXEL_FLOAT:
                    strides.assign({ sizeof(float), sizeof(float) });
                    P._channels[c].pixels = py::array_t<float,style>(shape, strides);
                    break;
                }
            }

            rv = exr_decoding_choose_default_routines (f, part, &decoder);
            if (rv != EXR_ERR_SUCCESS)
            {
                std::cout << "error " << __LINE__ << std::endl;
                frv = rv;
                break;
            }
        }
        else
        {
            rv = exr_decoding_update (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
            {
                std::cout << "error " << __LINE__ << std::endl;
                frv = rv;
                break;
            }
        }

        if (cinfo.type != EXR_STORAGE_DEEP_SCANLINE)
        {
            for (int16_t c = 0; c < decoder.channel_count; c++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];

                py::buffer_info buf = P._channels[c].pixels.request();
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
                    std::cout << "error " << __LINE__ << std::endl;
                    return false;
                }
                outc.user_pixel_stride = outc.user_bytes_per_element;
                outc.user_line_stride  = outc.user_pixel_stride * width;
            }
        }

        
        rv = exr_decoding_run (f, part, &decoder);
        if (rv != EXR_ERR_SUCCESS)
        {
            frv = rv;
            std::cout << "error " << __LINE__ << std::endl;
            break;
        }
    }

    exr_decoding_destroy (f, &decoder);

    return frv == EXR_ERR_SUCCESS;
}

bool
read_tiled_part (exr_context_t f, int part, PyPart& P)
{
    exr_result_t rv, frv;

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS) return true;

    P.width = datawin.max.x - datawin.min.x + 1;
    P.height = datawin.max.y - datawin.min.y + 1;

    uint32_t              txsz, tysz;
    exr_tile_level_mode_t levelmode;
    exr_tile_round_mode_t roundingmode;

    rv = exr_get_tile_descriptor (
        f, part, &txsz, &tysz, &levelmode, &roundingmode);
    if (rv != EXR_ERR_SUCCESS) return true;

    int32_t levelsx, levelsy;
    rv = exr_get_tile_levels (f, part, &levelsx, &levelsy);
    if (rv != EXR_ERR_SUCCESS) return true;

    frv            = rv;
    for (int32_t ylevel = 0; ylevel < levelsy; ++ylevel)
    {
        for (int32_t xlevel = 0; xlevel < levelsx; ++xlevel)
        {
            int32_t levw, levh;
            rv = exr_get_level_sizes (f, part, xlevel, ylevel, &levw, &levh);
            if (rv != EXR_ERR_SUCCESS)
            {
                frv = rv;
                break;
            }

            int32_t curtw, curth;
            rv = exr_get_tile_sizes (f, part, xlevel, ylevel, &curtw, &curth);
            if (rv != EXR_ERR_SUCCESS)
            {
                frv = rv;
                break;
            }

            // we could make this over all levels but then would have to
            // re-check the allocation size, let's leave it here to check when
            // tile size is < full / top level tile size
            exr_chunk_info_t      cinfo;
            exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

            int tx, ty;
            ty = 0;
            for (int64_t cury = 0; cury < levh; cury += curth, ++ty)
            {
                tx = 0;
                for (int64_t curx = 0; curx < levw; curx += curtw, ++tx)
                {
                    rv = exr_read_tile_chunk_info (
                        f, part, tx, ty, xlevel, ylevel, &cinfo);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        frv = rv;
                        break;
                    }

                    if (decoder.channels == NULL)
                    {
                        rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv       = rv;
                            break;
                        }

                        P._channels.resize(decoder.channel_count);

                        uint64_t bytes = 0;
                        for (int c = 0; c < decoder.channel_count; c++)
                        {
                            exr_coding_channel_info_t& outc =
                                decoder.channels[c];
                            // fake addr for default routines
                            outc.decode_to_ptr = (uint8_t*) 0x1000 + bytes;
                            outc.user_pixel_stride =
                                outc.user_bytes_per_element;
                            outc.user_line_stride =
                                outc.user_pixel_stride * curtw;
                            bytes += (uint64_t) curtw *
                                     (uint64_t) outc.user_bytes_per_element *
                                     (uint64_t) curth;


                            P._channels[c].name = outc.channel_name;
                            P._channels[c].type = exr_pixel_type_t(outc.data_type);

                            std::vector<size_t> shape, strides;
                            shape.assign({ P.width, P.height });

                            const auto style = py::array::c_style | py::array::forcecast;
                
                            switch (outc.data_type)
                            {
                            case EXR_PIXEL_UINT:
                                strides.assign({ sizeof(uint8_t), sizeof(uint8_t) });
                                P._channels[c].pixels = py::array_t<uint8_t,style>(shape, strides);
                                break;
                            case EXR_PIXEL_HALF:
                                strides.assign({ sizeof(half), sizeof(half) });
                                P._channels[c].pixels = py::array_t<half,style>(shape, strides);
                                break;
                            case EXR_PIXEL_FLOAT:
                                strides.assign({ sizeof(float), sizeof(float) });
                                P._channels[c].pixels = py::array_t<float,style>(shape, strides);
                                break;
                            }
                        }

                        rv = exr_decoding_choose_default_routines (
                            f, part, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv       = rv;
                            break;
                        }
                    }
                    else
                    {
                        rv = exr_decoding_update (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv = rv;
                            break;
                        }
                    }

                    if (cinfo.type != EXR_STORAGE_DEEP_TILED)
                    {
                        for (int c = 0; c < decoder.channel_count; c++)
                        {
                            exr_coding_channel_info_t& outc = decoder.channels[c];
                            outc.user_pixel_stride = outc.user_bytes_per_element;
                            outc.user_line_stride = outc.user_pixel_stride * curtw;

                            py::buffer_info buf = P._channels[c].pixels.request();
                            switch (outc.data_type)
                            {
                            case EXR_PIXEL_UINT:
                                {
                                    uint8_t* pixels = static_cast<uint8_t*>(buf.ptr);
                                    outc.decode_to_ptr = &pixels[cury*P.width];
                                }
                                break;
                            case EXR_PIXEL_HALF:
                                {
                                    half* pixels = static_cast<half*>(buf.ptr);
                                    outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[cury*P.width]);
                                }
                                break;
                            case EXR_PIXEL_FLOAT:
                                {
                                    float* pixels = static_cast<float*>(buf.ptr);
                                    outc.decode_to_ptr = reinterpret_cast<uint8_t*>(&pixels[cury*P.width]);
                                }
                                break;
                            }
                        }
                    }

                    rv = exr_decoding_run (f, part, &decoder);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        frv = rv;
                        break;
                    }
                }
            }

            exr_decoding_destroy (f, &decoder);
        }
    }

    return (frv != EXR_ERR_SUCCESS);
}

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
    
    std::cout << "get_attribute " << a << ": " << name << " type=" << attr->type << std::endl;
    
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
              exr_attr_chromaticities_t c = {
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
          return py::cast(PreviewImage(attr->preview->width, attr->preview->height));
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
      default: printf ("<ERROR Unknown type '%s'>", attr->type_name);
          break;
    }
    return py::none();
}

PyFile::PyFile(const py::list& parts)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    for (auto p : parts)
    {
        auto P = py::cast<PyPart>(*p);
        _parts.push_back(P);
    }
}

PyFile::PyFile(const py::dict& attributes, const py::list& channels,
           exr_storage_t type, exr_compression_t compression)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    _parts.push_back(PyPart(attributes, channels, type, compression, "Part0"));
}

PyFile::PyFile(const std::string& filename)
{
    exr_result_t              rv;
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    rv = exr_start_read (&f, filename.c_str(), &cinit);
    if (rv != EXR_ERR_SUCCESS)
        return;
    
    int numparts;
        
    rv = exr_get_count (f, &numparts);
    if (rv != EXR_ERR_SUCCESS)
        return;

    _parts.resize(numparts);
    
    for (int p = 0; p < numparts; ++p)
    {
        py::dict& h = _parts[p]._header;

        int32_t attrcount;
        rv = exr_get_attribute_count(f, p, &attrcount);
        if (rv != EXR_ERR_SUCCESS)
            return;

        for (int32_t a = 0; a < attrcount; ++a)
        {
            std::string name;
            py::object attr = get_attribute(f, p, a, name);
            h[name.c_str()] = attr;
        }

        exr_storage_t store;
        rv = exr_get_storage (f, p, &store);
        if (rv != EXR_ERR_SUCCESS)
            return;

        _parts[p].type = store;
            
        exr_compression_t compression;
        rv = exr_get_compression(f, p, &compression);
        if (rv != EXR_ERR_SUCCESS)
            return;
        
        _parts[p].compression = compression;

        if (store == EXR_STORAGE_SCANLINE || store == EXR_STORAGE_DEEP_SCANLINE)
        {
            if (!read_scanline_part (f, p, _parts[p]))
            {
                std::cerr << "error reading " << filename << std::endl;
                return;
            }
        }
        else if (store == EXR_STORAGE_TILED || store == EXR_STORAGE_DEEP_TILED)
        {
            if (!read_tiled_part (f, p, _parts[p]))
            {
                std::cerr << "error reading " << filename << std::endl;
                return;
            }
        }
    }
}

template <class T>
const T*
py_cast(const py::object& object)
{
    if (py::isinstance<T>(object))
        return py::cast<T*>(object);

    return nullptr;
}

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
    std::cout << "write_attribute " << name << std::endl;
    
    if (auto v = py_cast<exr_attr_box2i_t,Box2i>(object))
        exr_attr_set_box2i(f, p, name.c_str(), v);
    else if (auto v = py_cast<exr_attr_box2f_t,Box2f>(object))
        exr_attr_set_box2f(f, p, name.c_str(), v);
    else if (py::isinstance<py::list>(object))
    {
        auto list = py::cast<py::list>(object);
        std::cout << "list: ";
        py::print(object);
        std::cout << "list: " << list.attr("__repr__")() << std::endl;
        auto size = list.size();
        if (size == 0)
        {
            std::cout << "ERROR: list is empty" << std::endl;
        }
        else if (py::isinstance<py::float_>(list[0]))
        {
            std::vector<float> v = list.cast<std::vector<float>>();
            exr_attr_set_float_vector(f, p, name.c_str(), v.size(), &v[0]);
        }
        else if (py::isinstance<py::str>(list[0]))
        {
            std::vector<std::string> strings = list.cast<std::vector<std::string>>();
            std::vector<const char*> v;
            for (size_t i=0; i<strings.size(); i++)
                v.push_back(strings[i].c_str());
            exr_attr_set_string_vector(f, p, name.c_str(), v.size(), &v[0]);
            std::cout << ">> write_attribute string vector " << name << " [";
            for (auto e : v)
                std::cout << " " << e;
            std::cout << " ]" << std::endl;
        }
        else if (py::isinstance<PyChannel>(list[0]))
        {
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
                v[i].x_sampling = C[i].xsamples;
                v[i].y_sampling = C[i].ysamples;
            }
            exr_attr_chlist_t channels;
            channels.num_channels = v.size();
            channels.num_alloced = v.size();
            channels.entries = &v[0];
            exr_attr_set_channels(f, p, name.c_str(), &channels);
        }
    }
    else if (auto o = py_cast<exr_attr_chromaticities_t>(object))
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
    else if (auto v = py_cast<PreviewImage>(object))
    {
        exr_attr_preview_t o;
        o.width = v->width();
        o.height = v->height();
        o.alloc_size = 0;
        o.rgba = nullptr;
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
    {
        std::cout << "ERROR: write_attribute " << name
                  << " object=" << typeid(object).name()
                  << " " << object.get_type()
                  << std::endl;
    }
}

exr_result_t
PyFile::write(const char* filename)
{
    std::cout << __PRETTY_FUNCTION__ << " " << filename
              << " parts=" << _parts.size()
              << std::endl;

    exr_context_t f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    exr_result_t result = exr_start_write(&f, filename,
                                          EXR_WRITE_FILE_DIRECTLY, &cinit);
    if (result != EXR_ERR_SUCCESS)
        return result;

    exr_set_longname_support(f, 1);

    for (size_t p=0; p<_parts.size(); p++)
    {
        const PyPart& P = _parts[p];
            
        int part_index;
        result = exr_add_part(f, P.name.c_str(), P.type, &part_index);
        if (result != EXR_ERR_SUCCESS) 
            return result;

        exr_lineorder_t lineOrder = EXR_LINEORDER_INCREASING_Y;
        if (P._header.contains("lineOrder"))
            lineOrder = py::cast<exr_lineorder_t>(P._header["lineOrder"]);

        exr_compression_t compression = EXR_COMPRESSION_NONE;
        if (P._header.contains("compression"))
            compression = py::cast<exr_compression_t>(P._header["compression"]);
        
        exr_attr_box2i_t dataw;
        dataw.min.x = 0;
        dataw.min.y = 0;
        dataw.max.x = int32_t(P.width - 1);
        dataw.max.x = int32_t(P.height - 1);
        if (P._header.contains("dataWindow"))
        {
            Box2i box = py::cast<Box2i>(P._header["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
            std::cout << "dataWindow from header: " << box.min << " " << box.max << std::endl;
        }

        exr_attr_box2i_t dispw = dataw;
        if (P._header.contains("displayWindow"))
        {
            Box2i box = py::cast<Box2i>(P._header["displayWindow"]);
            dispw.min.x = box.min.x;
            dispw.min.y = box.min.y;
            dispw.max.x = box.max.x;
            dispw.max.y = box.max.y;
            std::cout << "displayWindow from header: " << box.min << " " << box.max << std::endl;
        }

        exr_attr_v2f_t swc;
        swc.x = 0.5f;
        swc.x = 0.5f;
        if (P._header.contains("screenWindowCenter"))
        {
            V2f v = py::cast<V2f>(P._header["screenWindowCenter"]);
            swc.x = v.x;
            swc.y = v.y;
        }

        float sww = 1.0f;
        if (P._header.contains("screenWindowWidth"))
            sww = py::cast<float>(P._header["screenWindowWidth"]);

        float pixelAspectRatio = 1.0f;
        if (P._header.contains("pixelAspectRatio"))
            sww = py::cast<float>(P._header["pixelAspectRatio"]);
        
        result = exr_initialize_required_attr (f, p, &dataw, &dispw, 
                                               pixelAspectRatio, &swc, sww,
                                               lineOrder, compression);
        
        if (result != EXR_ERR_SUCCESS)
            return result;

        for (auto a = P._header.begin(); a != P._header.end(); ++a)
        {
            auto v = *a;
            auto first = v.first;
            std::string name = py::cast<std::string>(py::str(first));
            py::object second = py::cast<py::object>(v.second);
            write_attribute(f, p, name, second);
        }

        for (size_t c=0; c<P._channels.size(); c++)
        {
            const PyChannel& C = P._channels[c];

            std::cout << "exr_add_channel " << c
                      << " " << C.name.c_str()
                      << " type=" << C.type
                      << " xs=" << C.xsamples
                      << " ys=" << C.ysamples
                      << std::endl;
            
            result = exr_add_channel(f, p, C.name.c_str(), C.type, 
                                     EXR_PERCEPTUALLY_LOGARITHMIC,
                                     C.xsamples, C.ysamples);
            if (result != EXR_ERR_SUCCESS) 
                return result;
        }

        result = exr_set_version(f, p, 1); // 1 is the latest version
        if (result != EXR_ERR_SUCCESS) 
            return result;

#if XXX
        // set chromaticities to Rec. ITU-R BT.709-3
        exr_attr_chromaticities_t chroma = {
            0.6400f, 0.3300f,  // red
            0.3000f, 0.6000f,  // green
            0.1500f, 0.0600f,  // blue
            0.3127f, 0.3290f}; // white
        result = exr_attr_set_chromaticities(f, p, "chromaticities", &chroma);
        if (result != EXR_ERR_SUCCESS) 
            return result;
#endif
    }

    result = exr_write_header(f);
    if (result != EXR_ERR_SUCCESS)
        return result;

    exr_encode_pipeline_t encoder;

    for (size_t p=0; p<_parts.size(); p++)
    {
        const PyPart& P = _parts[p];
            
        std::cout << "writing part " << p << " " << P.name << std::endl;
        
        exr_chunk_info_t cinfo;

        int32_t scansperchunk = 0;
        exr_get_scanlines_per_chunk(f, p, &scansperchunk);
        if (result != EXR_ERR_SUCCESS)
            return result;

        bool first = true;

        exr_attr_box2i_t dataw = {0, 0, int32_t(P.width - 1), int32_t(P.height - 1)};
        if (P._header.contains("dataWindow"))
        {
            Box2i box = py::cast<Box2i>(P._header["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
        }

        for (int16_t y = dataw.min.y; y <= dataw.max.y; y += scansperchunk)
        {
            std::cout << "Part " << p << " width=" << P.width << " y=" << y << std::endl;

            result = exr_write_scanline_chunk_info(f, p, y, &cinfo);
            if (result != EXR_ERR_SUCCESS) 
                return result;

            if (first)
                result = exr_encoding_initialize(f, p, &cinfo, &encoder);
            else
                result = exr_encoding_update(f, p, &cinfo, &encoder);
            if (result != EXR_ERR_SUCCESS) 
                return result;
        
            int channelCount = P._channels.size();
            
            for (size_t c=0; c<P._channels.size(); c++)
            {
                const auto& C = P._channels[c];
                
                std::cout << "channel " << c << " " << C.name << " array: " << C.pixels.shape(0) << std::endl;
                
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
                    return false;
                }
                
                encoder.channels[c].user_line_stride  = encoder.channels[c].user_pixel_stride * P.width;
                encoder.channels[c].height            = scansperchunk; // chunk height
                encoder.channels[c].width             = dataw.max.x - dataw.min.y + 1;

                std::cout << " channel " << C.name
                          << " " << encoder.channels[c].width
                          << " x " << encoder.channels[c].height
                          << std::endl;
            }

            if (first)
            {
                result = exr_encoding_choose_default_routines(f, p, &encoder);
                if (result != EXR_ERR_SUCCESS) 
                    return result;
            }
            
            result = exr_encoding_run(f, p, &encoder);
            if (result != EXR_ERR_SUCCESS)
                return result;

            first = false;
        }
    }

    result = exr_encoding_destroy(f, &encoder);
    if (result != EXR_ERR_SUCCESS)
        return result;

    result = exr_finish(&f);

    return EXR_ERR_SUCCESS;
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
        .def("__repr__", [](TileDescription& v) {
            std::stringstream stream;
            stream << "TileDescription(" << v.xSize
                   << ", " << v.ySize
                   << ", " << py::cast(v.mode)
                   << ", " << py::cast(v.roundingMode)
                   << ")";
            return stream.str();
        })
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
        .def_readwrite("n", &Rational::n)
        .def_readwrite("d", &Rational::d)
        ;
    
    py::class_<KeyCode>(m, "KeyCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int>())
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

    py::class_<exr_attr_chromaticities_t>(m, "Chromaticities")
        .def(py::init<float,float,float,float,float,float,float,float>())
        .def_readwrite("red_x", &exr_attr_chromaticities_t::red_x)
        .def_readwrite("red_y", &exr_attr_chromaticities_t::red_y)
        .def_readwrite("green_x", &exr_attr_chromaticities_t::green_x)
        .def_readwrite("green_y", &exr_attr_chromaticities_t::green_y)
        .def_readwrite("blue_x", &exr_attr_chromaticities_t::blue_x)
        .def_readwrite("blue_y", &exr_attr_chromaticities_t::blue_y)
        .def_readwrite("white_x", &exr_attr_chromaticities_t::white_x)
        .def_readwrite("white_y", &exr_attr_chromaticities_t::white_y)
        ;

    py::class_<PreviewRgba>(m, "PreviewRgba")
        .def(py::init())
        .def(py::init<unsigned char,unsigned char,unsigned char,unsigned char>())
        .def_readwrite("r", &PreviewRgba::r)
        .def_readwrite("g", &PreviewRgba::g)
        .def_readwrite("b", &PreviewRgba::b)
        .def_readwrite("a", &PreviewRgba::a)
        ;
    
    py::class_<PreviewImage>(m, "PreviewImage")
        .def(py::init())
        .def(py::init<int,int>())
        .def("width", &PreviewImage::width)
        .def("height", &PreviewImage::height)
        ;
    
    py::class_<V2i>(m, "V2i")
        .def(py::init())
        .def(py::init<int,int>())
        .def("__repr__", [](const V2i& v) { return repr(v); })
        .def_readwrite("x", &Imath::V2i::x)
        .def_readwrite("y", &Imath::V2i::y)
        ;

    py::class_<V2f>(m, "V2f")
        .def(py::init())
        .def(py::init<float,float>())
        .def("__repr__", [](const V2f& v) { return repr(v); })
        .def_readwrite("x", &Imath::V2f::x)
        .def_readwrite("y", &Imath::V2f::y)
        ;

    py::class_<V2d>(m, "V2d")
        .def(py::init())
        .def(py::init<double,double>())
        .def("__repr__", [](const V2d& v) { return repr(v); })
        .def_readwrite("x", &Imath::V2d::x)
        .def_readwrite("y", &Imath::V2d::y)
        ;

    py::class_<V3i>(m, "V3i")
        .def(py::init())
        .def(py::init<int,int,int>())
        .def("__repr__", [](const V3i& v) { return repr(v); })
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
        .def_readwrite("x", &Imath::V3d::x)
        .def_readwrite("y", &Imath::V3d::y)
        .def_readwrite("z", &Imath::V3d::z)
        ;

    py::class_<Box2i>(m, "Box2i")
        .def(py::init())
        .def(py::init<V2i,V2i>())
        .def("__repr__", [](const Box2i& b) {
            std::stringstream s;
            s << "(" << b.min << " " << b.max << ")";
            return s.str();
        })
        .def_readwrite("min", &Box2i::min)
        .def_readwrite("max", &Box2i::max)
        ;
    
    py::class_<Box2f>(m, "Box2f")
        .def(py::init())
        .def(py::init<V2f,V2f>())
        .def("__repr__", [](const Box2f& b) {
            std::stringstream s;
            s << "(" << b.min << " " << b.max << ")";
            return s.str();
        })
        .def_readwrite("min", &Box2f::min)
        .def_readwrite("max", &Box2f::max)
        ;
    
    py::class_<M33f>(m, "M33f")
        .def(py::init())
        .def(py::init<float,float,float,float,float,float,float,float,float>())
        .def("__repr__", [](const M33f& m) { return repr(m); })
        ;
    
    py::class_<M33d>(m, "M33d")
        .def(py::init())
        .def(py::init<double,double,double,double,double,double,double,double,double>())
        .def("__repr__", [](const M33d& m) { return repr(m); })
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
        ;
    
    py::class_<PyDouble>(m, "Double")
        .def(py::init<double>())
        .def("__repr__", [](const PyDouble& d) { return repr(d.d); })
        ;

    py::class_<PyChannel>(m, "Channel")
        .def(py::init())
        .def(py::init<const char*,exr_pixel_type_t,int,int>())
        .def(py::init<const char*,exr_pixel_type_t,int,int,py::array>())
        .def("__repr__", [](const PyChannel& c) { return repr(c); })
        .def_readwrite("name", &PyChannel::name)
        .def_readwrite("type", &PyChannel::type)
        .def_readwrite("xsamples", &PyChannel::xsamples)
        .def_readwrite("ysamples", &PyChannel::ysamples)
        .def_readwrite("pixels", &PyChannel::pixels)
        ;
    
    py::class_<PyPart>(m, "Part")
        .def(py::init())
        .def(py::init<py::dict,py::list,exr_storage_t,exr_compression_t,const char*>())
        .def("__repr__", [](const PyPart& p) { return repr(p); })
        .def_readwrite("name", &PyPart::name)
        .def_readwrite("type", &PyPart::type)
        .def_readwrite("width", &PyPart::width)
        .def_readwrite("height", &PyPart::height)
        .def_readwrite("compression", &PyPart::compression)
        .def("header", &PyPart::header)
        .def("channels", &PyPart::channels)
        ;

    py::class_<PyFile>(m, "File")
        .def(py::init<std::string>())
        .def(py::init<py::dict,py::list,exr_storage_t,exr_compression_t>())
        .def(py::init<py::list>())
        .def("parts", &PyFile::parts)
        .def("header", &PyFile::header)
        .def("channels", &PyFile::channels)
        .def("write", &PyFile::write)
        ;

    m.def("write_exr_file", &write_exr_file);
    m.def("write_exr_file_parts", &write_exr_file_parts);
}

