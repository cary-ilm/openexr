//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "openexr.h"

#include <ImfTimeCodeAttribute.h>
#include <ImfTileDescription.h>
#include <ImfRational.h>
#include <ImfKeyCode.h>
#include <ImfPreviewImage.h>

#include <ImathVec.h>
#include <ImathBox.h>

//#define DEBUGGIT 1

namespace py = pybind11;
using namespace py::literals;

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

class Channel 
{
public:

    Channel() : type(EXR_PIXEL_LAST_TYPE), xsamples(0), ysamples(0) {}
    Channel(const char* n, exr_pixel_type_t t, int x, int y)
        : name(n), type(t), xsamples(x), ysamples(y) {}
    
    std::string           name;
    exr_pixel_type_t      type;
    int                   xsamples;
    int                   ysamples;
    py::array             pixels;
};
    
class Part
{
  public:
    Part() : type(EXR_STORAGE_LAST_TYPE), compression (EXR_COMPRESSION_LAST_TYPE) {}

    const py::dict&    header() const { return _header; }
    py::list           channels() const { return py::cast(_channels); }

    std::string           name;
    exr_storage_t         type;
    uint64_t              width;
    uint64_t              height;
    exr_compression_t     compression;

    py::dict              _header;
    std::vector<Channel>  _channels;
};

class File 
{
public:
    File(const std::string& filename);
    
    py::list        parts() const { return py::cast(_parts); }
    const py::dict& header() const { return _parts[0].header(); }
    py::list        channels() const { return _parts[0].channels(); }
    
    exr_result_t    write(const char* filename);
    
    std::vector<Part>  _parts;
};
    
static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
    const char* fn;
    if (EXR_ERR_SUCCESS != exr_get_file_name (f, &fn)) fn = "<error>";
    fprintf (
        stderr,
        "ERROR '%s' (%s): %s\n",
        fn,
        exr_get_error_code_as_string (code),
        msg);
}

bool
readScanlinePart (exr_context_t f, int part, Part& P)
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
    
#if DEBUGGIT
    std::cout << "Part " << P.name << " " << width << "x" << height << std::endl;
#endif
    
    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    int32_t lines_per_chunk;
    rv = exr_get_scanlines_per_chunk (f, part, &lines_per_chunk);
    if (rv != EXR_ERR_SUCCESS)
        return false;

    frv = rv;

    for (uint64_t chunk = 0; chunk < height; chunk += lines_per_chunk)
    {
        exr_chunk_info_t cinfo = {0};
        int              y     = ((int) chunk) + datawin.min.y;

        rv = exr_read_scanline_chunk_info (f, part, y, &cinfo);
        if (rv != EXR_ERR_SUCCESS)
            return false;

        if (decoder.channels == NULL)
        {
            rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS)
                return false;

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
                frv = rv;
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
                case EXR_PIXEL_LAST_TYPE:
                default:
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
            break;
        }
    }

    exr_decoding_destroy (f, &decoder);

    return frv;
}

exr_result_t
readTiledPart (exr_context_t f, int part, Part& P)
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
getAttribute(exr_context_t f, int32_t p, int32_t a, std::string& name) 
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
    
    py::module_ imath = py::module_::import("Imath");
//    py::object V2i = imath.attr("V2i");
//    py::object V2f = imath.attr("V2f");
    py::object V2d = imath.attr("V2d");
    py::object V3i = imath.attr("V3i");
    py::object V3f = imath.attr("V3f");
    py::object V3d = imath.attr("V3d");
//    py::object Box2i = imath.attr("Box2i");
//    py::object Box2f = imath.attr("Box2f");
    py::object Box2d = imath.attr("Box2d");
    py::object Box3f = imath.attr("Box3f");
    py::object Box3d = imath.attr("Box3d");
    py::object M33 = imath.attr("M33");
    py::object M44 = imath.attr("M44");
    
    switch (attr->type)
    {
      case EXR_ATTR_BOX2I:
          {
              IMATH_NAMESPACE::V2i min(attr->box2i->min);
              IMATH_NAMESPACE::V2i max(attr->box2i->max);
              IMATH_NAMESPACE::Box2i box(min, max);
              return py::cast(box);
          }
      case EXR_ATTR_BOX2F:
          {
              IMATH_NAMESPACE::V2f min(attr->box2f->min);
              IMATH_NAMESPACE::V2f max(attr->box2f->max);
              IMATH_NAMESPACE::Box2f box(min, max);
              return py::cast(box);
          }
      case EXR_ATTR_CHLIST:
          {
              auto l = py::list();
              for (int c = 0; c < attr->chlist->num_channels; ++c)
              {
                  auto e = attr->chlist->entries[c];
                  l.append(py::cast(Channel(e.name.str, e.pixel_type, e.x_sampling, e.y_sampling)));
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
          return py::float_(attr->d);
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
          return py::cast(OPENEXR_IMF_NAMESPACE::KeyCode(attr->keycode->film_mfc_code,
                                                         attr->keycode->film_type,
                                                         attr->keycode->prefix,   
                                                         attr->keycode->count,            
                                                         attr->keycode->perf_offset,      
                                                         attr->keycode->perfs_per_frame,
                                                         attr->keycode->perfs_per_count));
      case EXR_ATTR_LINEORDER:
          return py::cast(exr_lineorder_t(attr->uc));
      case EXR_ATTR_M33F:
          return M33(attr->m33f->m[0],
                     attr->m33f->m[1],
                     attr->m33f->m[2],
                     attr->m33f->m[3],
                     attr->m33f->m[4],
                     attr->m33f->m[5],
                     attr->m33f->m[6],
                     attr->m33f->m[7],
                     attr->m33f->m[8]);
      case EXR_ATTR_M33D:
          return M33(attr->m33d->m[0],
                     attr->m33d->m[1],
                     attr->m33d->m[2],
                     attr->m33d->m[3],
                     attr->m33d->m[4],
                     attr->m33d->m[5],
                     attr->m33d->m[6],
                     attr->m33d->m[7],
                     attr->m33d->m[8]);
      case EXR_ATTR_M44F:
          return M44(attr->m44f->m[0],
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
                     attr->m44f->m[15]);
      case EXR_ATTR_M44D:
          return M44(attr->m44d->m[0],
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
                     attr->m44d->m[15]);
      case EXR_ATTR_PREVIEW:
          return py::cast(OPENEXR_IMF_NAMESPACE::PreviewImage(attr->preview->width, attr->preview->height));
      case EXR_ATTR_RATIONAL:
          return py::cast(OPENEXR_IMF_NAMESPACE::Rational(attr->rational->num, attr->rational->denom));
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
          return py::cast(OPENEXR_IMF_NAMESPACE::TileDescription(attr->tiledesc->x_size,
                                                                 attr->tiledesc->y_size,
                                                                 OPENEXR_IMF_NAMESPACE::LevelMode (EXR_GET_TILE_LEVEL_MODE (*(attr->tiledesc))),
                                                                 OPENEXR_IMF_NAMESPACE::LevelRoundingMode(EXR_GET_TILE_ROUND_MODE (*(attr->tiledesc)))));
      case EXR_ATTR_TIMECODE:
          return py::cast( OPENEXR_IMF_NAMESPACE::TimeCode(attr->timecode->time_and_flags,
                                                           attr->timecode->user_data));
      case EXR_ATTR_V2I:
          return py::cast(IMATH_NAMESPACE::V2i(attr->v2i->x, attr->v2i->y));
      case EXR_ATTR_V2F:
          return py::cast(IMATH_NAMESPACE::V2f(attr->v2f->x, attr->v2f->y));
      case EXR_ATTR_V2D:
          return V2d(attr->v2d->x, attr->v2d->y);
      case EXR_ATTR_V3I:
          return V3i(attr->v3i->x, attr->v3i->y, attr->v3i->z);
      case EXR_ATTR_V3F:
          return V3f(attr->v3f->x, attr->v3f->y, attr->v3f->z);
      case EXR_ATTR_V3D:
          return V3d(attr->v3d->x, attr->v3d->y, attr->v3d->z);
      case EXR_ATTR_OPAQUE: 
          return py::none();
      case EXR_ATTR_UNKNOWN:
      case EXR_ATTR_LAST_KNOWN_TYPE:
      default: printf ("<ERROR Unknown type '%s'>", attr->type_name);
          break;
    }
    return py::none();
}

#if XXX
template <class T>
bool
Part::get_attr_value(const char* name, T& value)
{
    if (!_header.has(name))
        return false;

    auto v = _header[name];
}
#endif
    
File::File(const std::string& filename)
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
            py::object attr = getAttribute(f, p, a, name);
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
            if (readScanlinePart (f, p, _parts[p]))
                return;
        }
        else if (store == EXR_STORAGE_TILED || store == EXR_STORAGE_DEEP_TILED)
        {
            if (readTiledPart (f, p, _parts[p]))
                return;
        }
    }
}

exr_result_t
File::write(const char* filename)
{
    exr_context_t f;
    exr_context_initializer_t init = EXR_DEFAULT_CONTEXT_INITIALIZER;

    exr_result_t result = exr_start_write(&f, filename, EXR_WRITE_FILE_DIRECTLY, &init);
    if (result != EXR_ERR_SUCCESS)
        return result;

    exr_set_longname_support(f, 1);

    for (size_t p=0; p<_parts.size(); p++)
    {
        const Part& P = _parts[p];
            
        int part_index;
        result = exr_add_part(f, P.name.c_str(), P.type, &part_index);
        if (result != EXR_ERR_SUCCESS) 
            return result;

        exr_lineorder_t lineOrder = EXR_LINEORDER_INCREASING_Y;
        if (P._header.contains("lineOrder"))
            lineOrder = py::cast<exr_lineorder_t>(P._header["lineOrder"]);
        std::cout << "lineOrder=" << lineOrder << std::endl;

        exr_compression_t compression = EXR_COMPRESSION_NONE;
        if (P._header.contains("compression"))
            compression = py::cast<exr_compression_t>(P._header["compression"]);
        std::cout << "compression=" << compression << std::endl;
        
        exr_attr_box2i_t dataw = {0, 0, int32_t(P.width - 1), int32_t(P.height - 1)};
        if (P._header.contains("dataWindow"))
        {
            IMATH_NAMESPACE::Box2i box = py::cast<IMATH_NAMESPACE::Box2i>(P._header["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
        }

        exr_attr_box2i_t dispw = dataw;
        if (P._header.contains("displayWindow"))
        {
            IMATH_NAMESPACE::Box2i box = py::cast<IMATH_NAMESPACE::Box2i>(P._header["displayWindow"]);
            dispw.min.x = box.min.x;
            dispw.min.y = box.min.y;
            dispw.max.x = box.max.x;
            dispw.max.y = box.max.y;
        }

        exr_attr_v2f_t   swc   = {0.5f, 0.5f}; // center of the screen window
        if (P._header.contains("screenWindowCenter"))
        {
            IMATH_NAMESPACE::V2f v = py::cast<IMATH_NAMESPACE::V2f>(P._header["screenWindowCenter"]);
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

        for (size_t c=0; c<P._channels.size(); c++)
        {
            const Channel& C = P._channels[c];
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

        // set chromaticities to Rec. ITU-R BT.709-3
        exr_attr_chromaticities_t chroma = {
            0.6400f, 0.3300f,  // red
            0.3000f, 0.6000f,  // green
            0.1500f, 0.0600f,  // blue
            0.3127f, 0.3290f}; // white
        result = exr_attr_set_chromaticities(f, p, "chromaticities", &chroma);
        if (result != EXR_ERR_SUCCESS) 
            return result;
    }

    result = exr_write_header(f);
    if (result != EXR_ERR_SUCCESS)
        return result;

    exr_encode_pipeline_t encoder;

    for (size_t p=0; p<_parts.size(); p++)
    {
        const Part& P = _parts[p];
            
        exr_chunk_info_t cinfo;

        int32_t scansperchunk = 0;
        exr_get_scanlines_per_chunk(f, p, &scansperchunk);
        if (result != EXR_ERR_SUCCESS)
            return result;

        bool first = true;

        exr_attr_box2i_t dataw = {0, 0, int32_t(P.width - 1), int32_t(P.height - 1)};
        if (P._header.contains("dataWindow"))
        {
            IMATH_NAMESPACE::Box2i box = py::cast<IMATH_NAMESPACE::Box2i>(P._header["dataWindow"]);
            dataw.min.x = box.min.x;
            dataw.min.y = box.min.y;
            dataw.max.x = box.max.x;
            dataw.max.y = box.max.y;
        }

        for (int16_t y = dataw.min.y; y <= dataw.max.y; y += scansperchunk)
        {
            std::cout << "Part " << p << " y=" << y << std::endl;

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


} // namespace

PYBIND11_MODULE(OpenEXR_new, m)
{
    using namespace py::literals;

    m.doc() = "openexr doc";
    m.attr("__version__") = OPENEXR_VERSION_STRING;

    py::enum_<OPENEXR_IMF_NAMESPACE::LevelRoundingMode>(m, "LevelRoundingMode")
        .value("ROUND_UP", OPENEXR_IMF_NAMESPACE::ROUND_UP)
        .value("ROUND_DOWN", OPENEXR_IMF_NAMESPACE::ROUND_DOWN)
        .value("NUM_ROUNDINGMODES", OPENEXR_IMF_NAMESPACE::NUM_ROUNDINGMODES)
        .export_values();

    py::enum_<OPENEXR_IMF_NAMESPACE::LevelMode>(m, "LevelMode")
        .value("ONE_LEVEL", OPENEXR_IMF_NAMESPACE::ONE_LEVEL)
        .value("MIPMAP_LEVELS", OPENEXR_IMF_NAMESPACE::MIPMAP_LEVELS)
        .value("RIPMAP_LEVELS", OPENEXR_IMF_NAMESPACE::RIPMAP_LEVELS)
        .value("NUM_LEVELMODES", OPENEXR_IMF_NAMESPACE::NUM_LEVELMODES)
        .export_values();

    py::class_<OPENEXR_IMF_NAMESPACE::TileDescription>(m, "TileDescription")
        .def(py::init())
        .def("__repr__", [](OPENEXR_IMF_NAMESPACE::TileDescription& v) {
            std::stringstream stream;
            stream << "TileDescription(" << v.xSize
                   << ", " << v.ySize
                   << ", " << py::cast(v.mode)
                   << ", " << py::cast(v.roundingMode)
                   << ")";
            return stream.str();
        })
        .def_readwrite("xSize", &OPENEXR_IMF_NAMESPACE::TileDescription::xSize)
        .def_readwrite("ySize", &OPENEXR_IMF_NAMESPACE::TileDescription::ySize)
        .def_readwrite("mode", &OPENEXR_IMF_NAMESPACE::TileDescription::mode)
        .def_readwrite("roundingMode", &OPENEXR_IMF_NAMESPACE::TileDescription::roundingMode)
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

    py::class_<OPENEXR_IMF_NAMESPACE::Rational>(m, "Rational")
        .def(py::init())
        .def(py::init<int,unsigned int>())
        .def_readwrite("n", &OPENEXR_IMF_NAMESPACE::Rational::n)
        .def_readwrite("d", &OPENEXR_IMF_NAMESPACE::Rational::d)
        ;
    
    py::class_<OPENEXR_IMF_NAMESPACE::KeyCode>(m, "KeyCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int>())
        .def_property("filmMfcCode", &OPENEXR_IMF_NAMESPACE::KeyCode::filmMfcCode, &OPENEXR_IMF_NAMESPACE::KeyCode::setFilmMfcCode)
        .def_property("filmType", &OPENEXR_IMF_NAMESPACE::KeyCode::filmType, &OPENEXR_IMF_NAMESPACE::KeyCode::setFilmType)
        .def_property("prefix", &OPENEXR_IMF_NAMESPACE::KeyCode::prefix, &OPENEXR_IMF_NAMESPACE::KeyCode::setPrefix)
        .def_property("count", &OPENEXR_IMF_NAMESPACE::KeyCode::count, &OPENEXR_IMF_NAMESPACE::KeyCode::setCount)
        .def_property("perfOffset", &OPENEXR_IMF_NAMESPACE::KeyCode::perfOffset, &OPENEXR_IMF_NAMESPACE::KeyCode::setPerfOffset)
        .def_property("perfsPerFrame", &OPENEXR_IMF_NAMESPACE::KeyCode::perfsPerFrame, &OPENEXR_IMF_NAMESPACE::KeyCode::setPerfsPerFrame) 
        .def_property("perfsPerCount", &OPENEXR_IMF_NAMESPACE::KeyCode::perfsPerCount, &OPENEXR_IMF_NAMESPACE::KeyCode::setPerfsPerCount)
        ; 

    py::class_<OPENEXR_IMF_NAMESPACE::TimeCode>(m, "TimeCode")
        .def(py::init())
        .def(py::init<int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int>())
        .def_property("hours", &OPENEXR_IMF_NAMESPACE::TimeCode::hours, &OPENEXR_IMF_NAMESPACE::TimeCode::setHours)
        .def_property("minutes", &OPENEXR_IMF_NAMESPACE::TimeCode::minutes, &OPENEXR_IMF_NAMESPACE::TimeCode::setMinutes)
        .def_property("seconds", &OPENEXR_IMF_NAMESPACE::TimeCode::seconds, &OPENEXR_IMF_NAMESPACE::TimeCode::setSeconds)
        .def_property("frame", &OPENEXR_IMF_NAMESPACE::TimeCode::frame, &OPENEXR_IMF_NAMESPACE::TimeCode::setFrame)
        .def_property("dropFrame", &OPENEXR_IMF_NAMESPACE::TimeCode::dropFrame, &OPENEXR_IMF_NAMESPACE::TimeCode::setDropFrame)
        .def_property("colorFrame", &OPENEXR_IMF_NAMESPACE::TimeCode::colorFrame, &OPENEXR_IMF_NAMESPACE::TimeCode::setColorFrame)
        .def_property("fieldPhase", &OPENEXR_IMF_NAMESPACE::TimeCode::fieldPhase, &OPENEXR_IMF_NAMESPACE::TimeCode::setFieldPhase)
        .def_property("bgf0", &OPENEXR_IMF_NAMESPACE::TimeCode::bgf0, &OPENEXR_IMF_NAMESPACE::TimeCode::setBgf0)
        .def_property("bgf1", &OPENEXR_IMF_NAMESPACE::TimeCode::bgf1, &OPENEXR_IMF_NAMESPACE::TimeCode::setBgf1)
        .def_property("bgf2", &OPENEXR_IMF_NAMESPACE::TimeCode::bgf2, &OPENEXR_IMF_NAMESPACE::TimeCode::setBgf2)
        .def_property("binaryGroup", &OPENEXR_IMF_NAMESPACE::TimeCode::binaryGroup, &OPENEXR_IMF_NAMESPACE::TimeCode::setBinaryGroup)
        .def_property("userData", &OPENEXR_IMF_NAMESPACE::TimeCode::userData, &OPENEXR_IMF_NAMESPACE::TimeCode::setUserData)
        .def("timeAndFlags", &OPENEXR_IMF_NAMESPACE::TimeCode::timeAndFlags)
        .def("setTimeAndFlags", &OPENEXR_IMF_NAMESPACE::TimeCode::setTimeAndFlags)
        ;

    py::class_<exr_attr_chromaticities_t>(m, "Chromaticities")
        .def_readwrite("red_x", &exr_attr_chromaticities_t::red_x)
        .def_readwrite("red_y", &exr_attr_chromaticities_t::red_y)
        .def_readwrite("green_x", &exr_attr_chromaticities_t::green_x)
        .def_readwrite("green_y", &exr_attr_chromaticities_t::green_y)
        .def_readwrite("blue_x", &exr_attr_chromaticities_t::blue_x)
        .def_readwrite("blue_y", &exr_attr_chromaticities_t::blue_y)
        .def_readwrite("white_x", &exr_attr_chromaticities_t::white_x)
        .def_readwrite("white_y", &exr_attr_chromaticities_t::white_y)
        ;

    py::class_<OPENEXR_IMF_NAMESPACE::PreviewRgba>(m, "PreviewRgba")
        .def(py::init<unsigned char,unsigned char,unsigned char,unsigned char>())
        .def_readwrite("r", &OPENEXR_IMF_NAMESPACE::PreviewRgba::r)
        .def_readwrite("g", &OPENEXR_IMF_NAMESPACE::PreviewRgba::g)
        .def_readwrite("b", &OPENEXR_IMF_NAMESPACE::PreviewRgba::b)
        .def_readwrite("a", &OPENEXR_IMF_NAMESPACE::PreviewRgba::a)
        ;
    
    py::class_<OPENEXR_IMF_NAMESPACE::PreviewImage>(m, "PreviewImage")
        .def(py::init<int,int>())
        .def("width", &OPENEXR_IMF_NAMESPACE::PreviewImage::width)
        .def("height", &OPENEXR_IMF_NAMESPACE::PreviewImage::height)
        ;
    
    py::class_<IMATH_NAMESPACE::V2i>(m, "V2i")
        .def_readwrite("x", &Imath::V2i::x)
        .def_readwrite("y", &Imath::V2i::y)
        ;

    py::class_<IMATH_NAMESPACE::Box2i>(m, "Box2i")
        .def_readwrite("min", &IMATH_NAMESPACE::Box2i::min)
        .def_readwrite("max", &IMATH_NAMESPACE::Box2i::max)
        ;
    
    py::class_<IMATH_NAMESPACE::V2f>(m, "V2f")
        .def_readwrite("x", &Imath::V2f::x)
        .def_readwrite("y", &Imath::V2f::y)
        ;

    py::class_<IMATH_NAMESPACE::Box2f>(m, "Box2f")
        .def_readwrite("min", &IMATH_NAMESPACE::Box2f::min)
        .def_readwrite("max", &IMATH_NAMESPACE::Box2f::max)
        ;
    
    py::class_<Channel>(m, "Channel")
        .def(py::init())
        .def(py::init<const char*,exr_pixel_type_t,int,int>())
        .def("__repr__", [](const Channel& c) {
            std::stringstream stream;
            stream << "Channel(\"" << c.name 
                << "\", type=" << py::cast(c.type) 
                << ", xsamples=" << c.xsamples 
                << ", ysamples=" << c.ysamples
                   << ")";
            return stream.str();
        })
        .def_readwrite("name", &Channel::name)
        .def_readwrite("type", &Channel::type)
        .def_readwrite("xsamples", &Channel::xsamples)
        .def_readwrite("ysamples", &Channel::ysamples)
        .def_readwrite("pixels", &Channel::pixels)
        ;
    
    py::class_<Part>(m, "Part")
        .def(py::init())
        .def_readwrite("name", &Part::name)
        .def_readwrite("type", &Part::type)
        .def_readwrite("width", &Part::width)
        .def_readwrite("height", &Part::height)
        .def_readwrite("compression", &Part::compression)
        .def("header", &Part::header)
        .def("channels", &Part::channels)
        ;

    py::class_<File>(m, "File")
        .def(py::init<std::string>())
        .def("parts", &File::parts)
        .def("header", &File::header)
        .def("channels", &File::channels)
        .def("write", &File::write)
        ;
}

