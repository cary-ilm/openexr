//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#include <pybind11/pybind11.h>

#include <ImfHeader.h>
#include <ImfMultiPartInputFile.h>
#include <ImfPixelType.h>
#include <ImfAttribute.h>
#include <ImfBoxAttribute.h>
#include <ImfChannelList.h>
#include <ImfStandardAttributes.h>
#include <ImfChannelListAttribute.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfKeyCodeAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfOutputFile.h>
#include <ImfPreviewImageAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfTiledOutputFile.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfVecAttribute.h>

#include "openexr.h"

using namespace pybind11::literals;

namespace {

class InputFile 
{
  public:
    InputFile(std::string filename);
    
    int numparts() const 
    {
        return _pyHeaders.size();
    }
    
    pybind11::object py_header() 
    {
        return _pyHeaders[0];
    }

    pybind11::object py_header(int part) 
    {
        return _pyHeaders[part];
    }

    pybind11::object channel(std::string c)
    {
        return pybind11::none();
    }
    
    pybind11::object channels()
    {
        return pybind11::none();
    }
    
    pybind11::object close()
    {
        return pybind11::none();
    }
    
    pybind11::object isComplete()
    {
        return pybind11::none();
    }
    
    std::vector<pybind11::dict> _pyHeaders;
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

static exr_result_t
realloc_deepdata (exr_decode_pipeline_t* decode)
{
    int32_t               w        = decode->chunk.width;
    int32_t               h        = decode->chunk.height;
    uint64_t              totsamps = 0, bytes = 0;
    const int32_t*        sampbuffer = decode->sample_count_table;
    std::vector<uint8_t>* ud =
        static_cast<std::vector<uint8_t>*> (decode->decoding_user_data);

    if (!ud)
    {
        for (int c = 0; c < decode->channel_count; c++)
        {
            exr_coding_channel_info_t& outc = decode->channels[c];
            outc.decode_to_ptr              = NULL;
            outc.user_pixel_stride          = outc.user_bytes_per_element;
            outc.user_line_stride           = 0;
        }
        return EXR_ERR_SUCCESS;
    }

    if ((decode->decode_flags & EXR_DECODE_SAMPLE_COUNTS_AS_INDIVIDUAL))
    {
        for (int32_t y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
                totsamps += sampbuffer[x];
            sampbuffer += w;
        }
    }
    else
    {
        for (int32_t y = 0; y < h; ++y)
            totsamps += sampbuffer[y * w + w - 1];
    }

    for (int c = 0; c < decode->channel_count; c++)
    {
        exr_coding_channel_info_t& outc = decode->channels[c];
        bytes += totsamps * outc.user_bytes_per_element;
    }

    if (bytes == 0)
    {
        for (int c = 0; c < decode->channel_count; c++)
        {
            exr_coding_channel_info_t& outc = decode->channels[c];
            outc.decode_to_ptr              = NULL;
            outc.user_pixel_stride          = outc.user_bytes_per_element;
            outc.user_line_stride           = 0;
        }
        return EXR_ERR_SUCCESS;
    }

    if (ud->size () < bytes)
    {
        ud->resize (bytes);
        if (ud->capacity () < bytes) return EXR_ERR_OUT_OF_MEMORY;
    }

    uint8_t* dptr = &((*ud)[0]);
    for (int c = 0; c < decode->channel_count; c++)
    {
        exr_coding_channel_info_t& outc = decode->channels[c];
        outc.decode_to_ptr              = dptr;
        outc.user_pixel_stride          = outc.user_bytes_per_element;
        outc.user_line_stride           = 0;

        dptr += totsamps * (uint64_t) outc.user_bytes_per_element;
    }
    return EXR_ERR_SUCCESS;
}

bool
readCoreScanlinePart (exr_context_t f, int part, bool reduceMemory = false, bool reduceTime = false) 
{
    exr_result_t     rv, frv;
    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS) return true;

    uint64_t width =
        (uint64_t) ((int64_t) datawin.max.x - (int64_t) datawin.min.x + 1);
    uint64_t height =
        (uint64_t) ((int64_t) datawin.max.y - (int64_t) datawin.min.y + 1);

    std::vector<uint8_t>  imgdata;
    bool                  doread  = false;
    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    int32_t lines_per_chunk;
    rv = exr_get_scanlines_per_chunk (f, part, &lines_per_chunk);
    if (rv != EXR_ERR_SUCCESS) return true;

    frv = rv;

    for (uint64_t chunk = 0; chunk < height; chunk += lines_per_chunk)
    {
        exr_chunk_info_t cinfo = {0};
        int              y     = ((int) chunk) + datawin.min.y;

        rv = exr_read_scanline_chunk_info (f, part, y, &cinfo);
        if (rv != EXR_ERR_SUCCESS)
        {
            frv = rv;
            if (reduceTime) break;
            continue;
        }

        if (decoder.channels == NULL)
        {
            rv = exr_decoding_initialize (f, part, &cinfo, &decoder);
            if (rv != EXR_ERR_SUCCESS) break;

            uint64_t bytes = 0;
            for (int c = 0; c < decoder.channel_count; c++)
            {
                exr_coding_channel_info_t& outc = decoder.channels[c];
                // fake addr for default routines
                outc.decode_to_ptr     = (uint8_t*) 0x1000;
                outc.user_pixel_stride = outc.user_bytes_per_element;
                outc.user_line_stride  = outc.user_pixel_stride * width;
                bytes += width * (uint64_t) outc.user_bytes_per_element *
                         (uint64_t) lines_per_chunk;
            }

            doread = true;

            if (cinfo.type == EXR_STORAGE_DEEP_SCANLINE)
            {
                decoder.decoding_user_data       = &imgdata;
                decoder.realloc_nonimage_data_fn = &realloc_deepdata;
            }
            else
            {
                if (doread) imgdata.resize (bytes);
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
                if (reduceTime) break;
                continue;
            }
        }

        if (doread)
        {
            if (cinfo.type != EXR_STORAGE_DEEP_SCANLINE)
            {
                uint8_t* dptr = &(imgdata[0]);
                for (int c = 0; c < decoder.channel_count; c++)
                {
                    exr_coding_channel_info_t& outc = decoder.channels[c];
                    outc.decode_to_ptr              = dptr;
                    outc.user_pixel_stride = outc.user_bytes_per_element;
                    outc.user_line_stride  = outc.user_pixel_stride * width;

                    dptr += width * (uint64_t) outc.user_bytes_per_element *
                            (uint64_t) lines_per_chunk;
                }
            }

            rv = exr_decoding_run (f, part, &decoder);
            if (rv != EXR_ERR_SUCCESS)
            {
                frv = rv;
                if (reduceTime) break;
            }
        }
    }

    exr_decoding_destroy (f, &decoder);

    return (frv != EXR_ERR_SUCCESS);
}

////////////////////////////////////////

bool
readCoreTiledPart (exr_context_t f, int part, bool reduceMemory = false, bool reduceTime = false)
    
{
    exr_result_t rv, frv;

    exr_attr_box2i_t datawin;
    rv = exr_get_data_window (f, part, &datawin);
    if (rv != EXR_ERR_SUCCESS) return true;

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
    bool keepgoing = true;
    for (int32_t ylevel = 0; keepgoing && ylevel < levelsy; ++ylevel)
    {
        for (int32_t xlevel = 0; keepgoing && xlevel < levelsx; ++xlevel)
        {
            int32_t levw, levh;
            rv = exr_get_level_sizes (f, part, xlevel, ylevel, &levw, &levh);
            if (rv != EXR_ERR_SUCCESS)
            {
                frv = rv;
                if (reduceTime)
                {
                    keepgoing = false;
                    break;
                }
                continue;
            }

            int32_t curtw, curth;
            rv = exr_get_tile_sizes (f, part, xlevel, ylevel, &curtw, &curth);
            if (rv != EXR_ERR_SUCCESS)
            {
                frv = rv;
                if (reduceTime)
                {
                    keepgoing = false;
                    break;
                }
                continue;
            }

            // we could make this over all levels but then would have to
            // re-check the allocation size, let's leave it here to check when
            // tile size is < full / top level tile size
            std::vector<uint8_t>  tiledata;
            bool                  doread = false;
            exr_chunk_info_t      cinfo;
            exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

            int tx, ty;
            ty = 0;
            for (int64_t cury = 0; keepgoing && cury < levh;
                 cury += curth, ++ty)
            {
                tx = 0;
                for (int64_t curx = 0; keepgoing && curx < levw;
                     curx += curtw, ++tx)
                {
                    rv = exr_read_tile_chunk_info (
                        f, part, tx, ty, xlevel, ylevel, &cinfo);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        frv = rv;
                        if (reduceTime)
                        {
                            keepgoing = false;
                            break;
                        }
                        continue;
                    }

                    if (decoder.channels == NULL)
                    {
                        rv =
                            exr_decoding_initialize (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv       = rv;
                            keepgoing = false;
                            break;
                        }

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
                        }

                        doread = true;

                        if (cinfo.type == EXR_STORAGE_DEEP_TILED)
                        {
                            decoder.decoding_user_data = &tiledata;
                            decoder.realloc_nonimage_data_fn =
                                &realloc_deepdata;
                        }
                        else
                        {
                            if (doread) tiledata.resize (bytes);
                        }
                        rv = exr_decoding_choose_default_routines (
                            f, part, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv       = rv;
                            keepgoing = false;
                            break;
                        }
                    }
                    else
                    {
                        rv = exr_decoding_update (f, part, &cinfo, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv = rv;
                            if (reduceTime)
                            {
                                keepgoing = false;
                                break;
                            }
                            continue;
                        }
                    }

                    if (doread)
                    {
                        if (cinfo.type != EXR_STORAGE_DEEP_TILED)
                        {
                            uint8_t* dptr = &(tiledata[0]);
                            for (int c = 0; c < decoder.channel_count; c++)
                            {
                                exr_coding_channel_info_t& outc =
                                    decoder.channels[c];
                                outc.decode_to_ptr = dptr;
                                outc.user_pixel_stride =
                                    outc.user_bytes_per_element;
                                outc.user_line_stride =
                                    outc.user_pixel_stride * curtw;
                                dptr += (uint64_t) curtw *
                                        (uint64_t) outc.user_bytes_per_element *
                                        (uint64_t) curth;
                            }
                        }

                        rv = exr_decoding_run (f, part, &decoder);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            frv = rv;
                            if (reduceTime)
                            {
                                keepgoing = false;
                                break;
                            }
                        }
                    }
                }
            }

            exr_decoding_destroy (f, &decoder);
        }
    }

    return (frv != EXR_ERR_SUCCESS);
}


InputFile::InputFile(std::string filename)
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

    _pyHeaders.resize(numparts);

    bool reduceMemory = false;
    bool reduceTime = false;
    
    pybind11::module_ imath = pybind11::module_::import("Imath");
    pybind11::object V2i = imath.attr("V2i");
    pybind11::object V2f = imath.attr("V2f");
    pybind11::object Box2i = imath.attr("Box2i");
    pybind11::object Box2f = imath.attr("Box2f");
    
    for (int p = 0; p < numparts; ++p)
    {
        pybind11::dict& h = _pyHeaders[p];

        int32_t attrcount;
        rv = exr_get_attribute_count(f, p, &attrcount);
        if (rv != EXR_ERR_SUCCESS)
            return;

        for (int32_t a = 0; a < attrcount; ++a)
        {
            const exr_attribute_t* attr;
            rv = exr_get_attribute_by_index(f, p, EXR_ATTR_LIST_FILE_ORDER, a, &attr);

            printf ("attr->name: %s ", attr->name);
            
            switch (attr->type)
            {
            case EXR_ATTR_BOX2I:
                h[attr->name] = Box2i(V2i(attr->box2i->min.x, attr->box2i->min.y),
                                      V2i(attr->box2i->max.x, attr->box2i->max.y));
                break;
            case EXR_ATTR_BOX2F:
                h[attr->name] = Box2f(V2f(attr->box2i->min.x, attr->box2i->min.y),
                                      V2f(attr->box2i->max.x, attr->box2i->max.y));
                break;
            case EXR_ATTR_CHLIST:
                printf ("%d channels\n", attr->chlist->num_channels);
                for (int c = 0; c < attr->chlist->num_channels; ++c)
                {
                    if (c > 0) printf ("\n");
                    printf (
                        "   '%s': %s samp %d %d",
                        attr->chlist->entries[c].name.str,
                        (attr->chlist->entries[c].pixel_type == EXR_PIXEL_UINT
                         ? "uint"
                         : (attr->chlist->entries[c].pixel_type == EXR_PIXEL_HALF
                            ? "half"
                            : attr->chlist->entries[c].pixel_type ==
                            EXR_PIXEL_FLOAT
                            ? "float"
                            : "<UNKNOWN>")),
                        attr->chlist->entries[c].x_sampling,
                        attr->chlist->entries[c].y_sampling);
                }
                break;
            case EXR_ATTR_CHROMATICITIES:
                h[attr->name] = Chromaticiies(Chromaticity(attr->chromaticities->red_x,
                                                           attr->chromaticities->red_y),
                                              Chromaticity(attr->chromaticities->green_x,
                                                           attr->chromaticities->green_y),
                                              Chromaticity(attr->chromaticities->blue_x,
                                                           attr->chromaticities->blue_y),
                                              Chromaticity(attr->chromaticities->white_x,
                                                           attr->chromaticities->white_y));
                break;
            case EXR_ATTR_COMPRESSION: {
                static const char* compressionnames[] = {
                    "none",
                    "rle",
                    "zips",
                    "zip",
                    "piz",
                    "pxr24",
                    "b44",
                    "b44a",
                    "dwaa",
                    "dwab"};
                h[attr->name] = pybind11::str(compressionnames[attr->uc]);
                break;
            }
            case EXR_ATTR_DOUBLE:
                h[attr->name] = pybind11::float_(attr->d);
                break;
            case EXR_ATTR_ENVMAP:
                h[attr->name] = pybind11::str(attr->uc == 0 ? "latlong" : "cube");
                break;
            case EXR_ATTR_FLOAT:
                h[attr->name] = pybind11::float_(attr->f);
                break;
#if XXX
            case EXR_ATTR_FLOAT_VECTOR:
                printf ("[%d entries]:\n   ", attr->floatvector->length);
                for (int i = 0; i < attr->floatvector->length; ++i)
                    printf (" %g", (double) attr->floatvector->arr[i]);
                break;
#endif
            case EXR_ATTR_INT:
                h[attr->name] = pybind11::int_(attr->i);
                printf ("%d", attr->i);
                break;
            case EXR_ATTR_KEYCODE:
                h[attr->name] = pybind11::make_tuple(
                    attr->keycode->film_mfc_code,
                    attr->keycode->film_type,
                    attr->keycode->prefix,
                    attr->keycode->count,
                    attr->keycode->perf_offset,
                    attr->keycode->perfs_per_frame,
                    attr->keycode->perfs_per_count);
                
                printf (
                    "mfgc %d film %d prefix %d count %d perf_off %d ppf %d ppc %d",
                    attr->keycode->film_mfc_code,
                    attr->keycode->film_type,
                    attr->keycode->prefix,
                    attr->keycode->count,
                    attr->keycode->perf_offset,
                    attr->keycode->perfs_per_frame,
                    attr->keycode->perfs_per_count);
                break;
            case EXR_ATTR_LINEORDER:
                h[attr->name] = pybind11::int_(attr->uc);
                printf (
                    "%d (%s)",
                    (int) attr->uc,
                    attr->uc == EXR_LINEORDER_INCREASING_Y
                    ? "increasing"
                    : (attr->uc == EXR_LINEORDER_DECREASING_Y
                       ? "decreasing"
                       : (attr->uc == EXR_LINEORDER_RANDOM_Y ? "random"
                          : "<UNKNOWN>")));
                break;
            case EXR_ATTR_M33F:
                printf (
                    "[ [%g %g %g] [%g %g %g] [%g %g %g] ]",
                    (double) attr->m33f->m[0],
                    (double) attr->m33f->m[1],
                    (double) attr->m33f->m[2],
                    (double) attr->m33f->m[3],
                    (double) attr->m33f->m[4],
                    (double) attr->m33f->m[5],
                    (double) attr->m33f->m[6],
                    (double) attr->m33f->m[7],
                    (double) attr->m33f->m[8]);
                break;
            case EXR_ATTR_M33D:
                printf (
                    "[ [%g %g %g] [%g %g %g] [%g %g %g] ]",
                    attr->m33d->m[0],
                    attr->m33d->m[1],
                    attr->m33d->m[2],
                    attr->m33d->m[3],
                    attr->m33d->m[4],
                    attr->m33d->m[5],
                    attr->m33d->m[6],
                    attr->m33d->m[7],
                    attr->m33d->m[8]);
                break;
            case EXR_ATTR_M44F:
                printf (
                    "[ [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] ]",
                    (double) attr->m44f->m[0],
                    (double) attr->m44f->m[1],
                    (double) attr->m44f->m[2],
                    (double) attr->m44f->m[3],
                    (double) attr->m44f->m[4],
                    (double) attr->m44f->m[5],
                    (double) attr->m44f->m[6],
                    (double) attr->m44f->m[7],
                    (double) attr->m44f->m[8],
                    (double) attr->m44f->m[9],
                    (double) attr->m44f->m[10],
                    (double) attr->m44f->m[11],
                    (double) attr->m44f->m[12],
                    (double) attr->m44f->m[13],
                    (double) attr->m44f->m[14],
                    (double) attr->m44f->m[15]);
                break;
            case EXR_ATTR_M44D:
                printf (
                    "[ [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] ]",
                    attr->m44d->m[0],
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
                break;
            case EXR_ATTR_PREVIEW:
                printf ("%u x %u", attr->preview->width, attr->preview->height);
                break;
            case EXR_ATTR_RATIONAL:
                printf ("%d / %u", attr->rational->num, attr->rational->denom);
                if (attr->rational->denom != 0)
                    printf (
                        " (%g)",
                        (double) (attr->rational->num) /
                        (double) (attr->rational->denom));
                break;
            case EXR_ATTR_STRING:
                h[attr->name] = pybind11::str(attr->string->str);
                printf ("'%s'", attr->string->str ? attr->string->str : "<NULL>");
                break;
            case EXR_ATTR_STRING_VECTOR:
                printf ("[%d entries]:\n", attr->stringvector->n_strings);
                for (int i = 0; i < attr->stringvector->n_strings; ++i)
                {
                    if (i > 0) printf ("\n");
                    printf ("    '%s'", attr->stringvector->strings[i].str);
                }
                break;
            case EXR_ATTR_TILEDESC: {
                static const char* lvlModes[] = {
                    "single image", "mipmap", "ripmap"};
                uint8_t lvlMode =
                    (uint8_t) EXR_GET_TILE_LEVEL_MODE (*(attr->tiledesc));
                uint8_t rndMode =
                    (uint8_t) EXR_GET_TILE_ROUND_MODE (*(attr->tiledesc));
                printf (
                    "size %u x %u level %u (%s) round %u (%s)",
                    attr->tiledesc->x_size,
                    attr->tiledesc->y_size,
                    lvlMode,
                    lvlMode < 3 ? lvlModes[lvlMode] : "<UNKNOWN>",
                    rndMode,
                    rndMode == 0 ? "down" : "up");
                break;
            }
            case EXR_ATTR_TIMECODE:
                printf (
                    "time %u user %u",
                    attr->timecode->time_and_flags,
                    attr->timecode->user_data);
                break;
            case EXR_ATTR_V2I: printf ("[ %d, %d ]", attr->v2i->x, attr->v2i->y); break;
            case EXR_ATTR_V2F:
                printf ("[ %g, %g ]", (double) attr->v2f->x, (double) attr->v2f->y);
                break;
            case EXR_ATTR_V2D: printf ("[ %g, %g ]", attr->v2d->x, attr->v2d->y); break;
            case EXR_ATTR_V3I:
                printf ("[ %d, %d, %d ]", attr->v3i->x, attr->v3i->y, attr->v3i->z);
                break;
            case EXR_ATTR_V3F:
                printf (
                    "[ %g, %g, %g ]",
                    (double) attr->v3f->x,
                    (double) attr->v3f->y,
                    (double) attr->v3f->z);
                break;
            case EXR_ATTR_V3D:
                printf ("[ %g, %g, %g ]", attr->v3d->x, attr->v3d->y, attr->v3d->z);
                break;
            case EXR_ATTR_OPAQUE: {
                uintptr_t faddr_unpack = (uintptr_t) attr->opaque->unpack_func_ptr;
                uintptr_t faddr_pack   = (uintptr_t) attr->opaque->pack_func_ptr;
                uintptr_t faddr_destroy =
                    (uintptr_t) attr->opaque->destroy_unpacked_func_ptr;
                printf (
                    "(size %d unp size %d hdlrs %p %p %p)",
                    attr->opaque->size,
                    attr->opaque->unpacked_size,
                    (void*) faddr_unpack,
                    (void*) faddr_pack,
                    (void*) faddr_destroy);
                break;
            }
            case EXR_ATTR_UNKNOWN:
            case EXR_ATTR_LAST_KNOWN_TYPE:
            default: printf ("<ERROR Unknown type '%s'>", attr->type_name);
                break;
            }

            printf("\n");
        }
        
        exr_storage_t store;
        rv = exr_get_storage (f, p, &store);
        if (rv != EXR_ERR_SUCCESS)
            return;

        if (store == EXR_STORAGE_SCANLINE || store == EXR_STORAGE_DEEP_SCANLINE)
        {
            if (readCoreScanlinePart (f, p, reduceMemory, reduceTime))
                return;
        }
        else if (store == EXR_STORAGE_TILED || store == EXR_STORAGE_DEEP_TILED)
        {
            if (readCoreTiledPart (f, p, reduceMemory, reduceTime))
                return;
        }
    }
    
    exr_finish (&f);
}
    
class OutputFile
{
  public:
    OutputFile(std::string, pybind11::dict header) 
    {
    }
    
    pybind11::object writePixels(pybind11::dict channels)
    {
        return pybind11::none();
    }
    
    pybind11::object currentScanline()
    {
        return pybind11::none();
    }
    
    pybind11::object close()
    {
        return pybind11::none();
    }
};
    
pybind11::object
makeHeader(int width, int height)
{
    pybind11::dict h;
    h["width"] = width;
    h["height"] = height;
    return h;
}
    
}// namespace

PYBIND11_MODULE(OpenEXRp11, m)
{
    using namespace pybind11::literals;

    m.doc() = "openexrp11 doc";
    m.attr("__version__") = "dev";

    m.def("Header", [](int width, int height) -> pybind11::object
    {
        return makeHeader(width, height);
    });
          
    auto ifile = pybind11::class_<InputFile>(m, "InputFile");
    ifile.def(pybind11::init<std::string>());

    ifile.def("xxx", [](InputFile& self) -> pybind11::object
    {
        pybind11::module_ imath = pybind11::module_::import("Imath");
        pybind11::object min = imath.attr("V2i")(1,2);
        pybind11::object max = imath.attr("V2i")(10,20);
        pybind11::object box = imath.attr("Box2i")(min,max);
        return box;
    });
    
    ifile.def("parts", &InputFile::numparts);

    ifile.def("header", [](InputFile& self) -> pybind11::object
    {
        return self.py_header();
    });
    
    ifile.def("header", [](InputFile& self, int part) -> pybind11::object
    {
        return self.py_header(part);
    });
    
    ifile.def("channel", [](InputFile& self,std::string c) -> pybind11::object
    {
        return self.channel(c);
    });
    
    ifile.def("channels", [](InputFile& self) -> pybind11::object
    {
        return self.channels();
    });
    ifile.def("close", [](InputFile& self) -> pybind11::object
    {
        return self.close();
    });
    
    ifile.def("isComplete", [](InputFile& self) -> pybind11::object
    {
        return self.isComplete();
    });
    
    auto ofile = pybind11::class_<OutputFile>(m, "OutputFile");
    ofile.def(pybind11::init<std::string,pybind11::dict>());
    
    ofile.def("writePixels", [](OutputFile& self, pybind11::dict channels) -> pybind11::object
    {
        return self.writePixels(channels);
    });
    ofile.def("currentScanline", [](OutputFile& self) -> pybind11::object
    {
        return self.currentScanline();
    });
    ofile.def("close", [](OutputFile& self) -> pybind11::object
    {
        return self.close();
    });
    

    m.attr("FLOAT") = static_cast<int> (OPENEXR_IMF_NAMESPACE::FLOAT);
    m.attr("HALF") = static_cast<int> (OPENEXR_IMF_NAMESPACE::HALF);
    m.attr("UINT") = static_cast<int> (OPENEXR_IMF_NAMESPACE::UINT);

    m.attr("__version__") = OPENEXR_VERSION_STRING;
    m.attr("__xersion__") = pybind11::str(std::string(OPENEXR_VERSION_STRING));
}

#if XXX
        
#define PY_SSIZE_T_CLEAN // required for Py_BuildValue("s#") for Python 3.10
#include <Python.h>

#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#    define PY_SSIZE_T_MAX INT_MAX
#    define PY_SSIZE_T_MIN INT_MIN
#endif

#if PY_MAJOR_VERSION >= 3
#    define MOD_ERROR_VAL NULL
#    define MOD_SUCCESS_VAL(val) val
#    define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name (void)
#    define MOD_DEF(ob, name, doc, methods)                                    \
        static struct PyModuleDef moduledef = {                                \
            PyModuleDef_HEAD_INIT,                                             \
            name,                                                              \
            doc,                                                               \
            -1,                                                                \
            methods,                                                           \
        };                                                                     \
        ob = PyModule_Create (&moduledef);
#    define PyInt_FromLong(x) PyLong_FromLong (x)
#    define PyInt_AsLong(x) PyLong_AsLong (x)
#    define PyInt_Check(x) PyLong_Check (x)
#    define PyString_Check(x) PyBytes_Check (x)
#    define PyString_AsString(x) PyBytes_AsString (x)
#    define PyString_Size(x) PyBytes_Size (x)
#    define PyString_FromString(x) PyBytes_FromString (x)
#    define PyString_FromStringAndSize(x, y) PyBytes_FromStringAndSize (x, y)
#    define PyUTF8_AsSstring(x) PyString_AsString (PyUnicode_AsUTF8String (x))
#    define PyUTF8_FromSstring(x) Something...
#else
#    define MOD_ERROR_VAL
#    define MOD_SUCCESS_VAL(val)
#    define MOD_INIT(name) extern "C" void init##name (void)
#    define MOD_DEF(ob, name, doc, methods)                                    \
        ob = Py_InitModule3 (name, methods, doc);
#    define PyUTF8_AsSstring(x) PyString_AsString (x)
#endif

#include <ImfIO.h>
#include <Iex.h>
#include <ImfArray.h>
#include <ImfAttribute.h>
#include <ImfBoxAttribute.h>
#include <ImfChannelList.h>
#include <ImfStandardAttributes.h>
#include <ImfChannelListAttribute.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfIntAttribute.h>
#include <ImfKeyCodeAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfOutputFile.h>
#include <ImfPreviewImageAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfTiledOutputFile.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfVersion.h>

#include <ImfRationalAttribute.h>
#include <ImfRational.h>
#include <ImfKeyCodeAttribute.h>
#include <ImfKeyCode.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfTimeCode.h>

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iostream>
#include <vector>

#define IMATH_VERSION                                                          \
    IMATH_VERSION_MAJOR * 10000 + IMATH_VERSION_MINOR * 100 +                  \
        IMATH_VERSION_PATCH

#if IMATH_VERSION >= 30001
#    define Int64 uint64_t
#    define SInt64 int64_t
#endif

using namespace std;
using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static PyObject* OpenEXR_error = NULL;
static PyObject* pModuleImath;

static PyObject*
PyObject_StealAttrString (PyObject* o, const char* name)
{
    PyObject* r = PyObject_GetAttrString (o, name);
    Py_DECREF (r);
    return r;
}

static PyObject*
PyObject_Call1 (PyObject* f, PyObject* a)
{
    PyObject* r = PyObject_CallObject (f, a);
    Py_DECREF (a);
    return r;
}

/** @brief split a string in a list of them
 * @param str The string to split
 * @param sep The separator
 * @return The list of strings
 */
static std::vector<std::string>
split (const std::string& str, const char sep)
{
    std::stringstream        spliter (str);
    std::string              token;
    std::vector<std::string> words;
    while (std::getline (spliter, token, sep))
    {
        if (token.size ()) words.push_back (token);
    }
    return words;
}

////////////////////////////////////////////////////////////////////////
//    Istream and Ostream derivatives
////////////////////////////////////////////////////////////////////////

class C_IStream : public IStream
{
public:
    C_IStream (PyObject* fo) : IStream (""), _fo (fo) {}
    virtual bool        read (char c[], int n);
    virtual Int64       tellg ();
    virtual void        seekg (Int64 pos);
    virtual void        clear ();
    virtual const char* fileName () const;

private:
    PyObject* _fo;
};

bool
C_IStream::read (char c[], int n)
{
    PyObject* data =
        PyObject_CallMethod (_fo, (char*) "read", (char*) "(i)", n);
    if (data != NULL && PyString_AsString (data) &&
        PyString_Size (data) == (Py_ssize_t) n)
    {
        memcpy (c, PyString_AsString (data), PyString_Size (data));
        Py_DECREF (data);
    }
    else { throw IEX_NAMESPACE::InputExc ("file read failed"); }
    return 0;
}

const char*
C_IStream::fileName () const
{
    return "xxx";
}

Int64
C_IStream::tellg ()
{
    PyObject* rv = PyObject_CallMethod (_fo, (char*) "tell", NULL);
    if (rv != NULL && PyNumber_Check (rv))
    {
        PyObject* lrv = PyNumber_Long (rv);
        long long t   = PyLong_AsLong (lrv);
        Py_DECREF (lrv);
        Py_DECREF (rv);
        return (Int64) t;
    }
    else { throw IEX_NAMESPACE::InputExc ("tell failed"); }
}

void
C_IStream::seekg (Int64 pos)
{
    PyObject* data =
        PyObject_CallMethod (_fo, (char*) "seek", (char*) "(L)", pos);
    if (data != NULL) { Py_DECREF (data); }
    else { throw IEX_NAMESPACE::InputExc ("seek failed"); }
}

void
C_IStream::clear ()
{}

////////////////////////////////////////////////////////////////////////

class C_OStream : public OStream
{
public:
    C_OStream (PyObject* fo) : OStream (""), _fo (fo) {}
    virtual void        write (const char* c, int n);
    virtual Int64       tellp ();
    virtual void        seekp (Int64 pos);
    virtual void        clear ();
    virtual const char* fileName () const;

private:
    PyObject* _fo;
};

void
C_OStream::write (const char* c, int n)
{
    PyObject* data =
        PyObject_CallMethod (_fo, (char*) "write", (char*) "(s#)", c, n);
    if (data != NULL) { Py_DECREF (data); }
    else { throw IEX_NAMESPACE::InputExc ("file write failed"); }
}

const char*
C_OStream::fileName () const
{
    return "xxx";
}

Int64
C_OStream::tellp ()
{
    PyObject* rv = PyObject_CallMethod (_fo, (char*) "tell", NULL);
    if (rv != NULL && PyNumber_Check (rv))
    {
        PyObject* lrv = PyNumber_Long (rv);
        long long t   = PyLong_AsLong (lrv);
        Py_DECREF (lrv);
        Py_DECREF (rv);
        return (Int64) t;
    }
    else { throw IEX_NAMESPACE::InputExc ("tell failed"); }
}

void
C_OStream::seekp (Int64 pos)
{
    PyObject* data =
        PyObject_CallMethod (_fo, (char*) "seek", (char*) "(L)", pos);
    if (data != NULL) { Py_DECREF (data); }
    else { throw IEX_NAMESPACE::InputExc ("seek failed"); }
}

void
C_OStream::clear ()
{}

////////////////////////////////////////////////////////////////////////
//    InputFile
////////////////////////////////////////////////////////////////////////

typedef struct
{
    PyObject_HEAD InputFile i;
    PyObject*               fo;
    C_IStream*              istream;
    int                     is_opened;
} InputFileC;

static PyObject*
channel (PyObject* self, PyObject* args, PyObject* kw)
{
    InputFile* file = &((InputFileC*) self)->i;

    Box2i dw = file->header ().dataWindow ();
    int   miny, maxy;

    miny = dw.min.y;
    maxy = dw.max.y;

    char*     cname;
    PyObject* pixel_type = NULL;
    char*     keywords[] = {
        (char*) "cname",
        (char*) "pixel_type",
        (char*) "scanLine1",
        (char*) "scanLine2",
        NULL};
    if (!PyArg_ParseTupleAndKeywords (
            args, kw, "s|Oii", keywords, &cname, &pixel_type, &miny, &maxy))
        return NULL;

    if (maxy < miny)
    {
        PyErr_SetString (PyExc_TypeError, "scanLine1 must be <= scanLine2");
        return NULL;
    }
    if (miny < dw.min.y)
    {
        PyErr_SetString (
            PyExc_TypeError, "scanLine1 cannot be outside dataWindow");
        return NULL;
    }
    if (maxy > dw.max.y)
    {
        PyErr_SetString (
            PyExc_TypeError, "scanLine2 cannot be outside dataWindow");
        return NULL;
    }

    ChannelList channels   = file->header ().channels ();
    Channel*    channelPtr = channels.findChannel (cname);
    if (channelPtr == NULL)
    {
        return PyErr_Format (
            PyExc_TypeError, "There is no channel '%s' in the image", cname);
    }

    Imf::PixelType pt;
    if (pixel_type != NULL)
    {
        if (PyObject_GetAttrString (pixel_type, "v") == NULL)
        {
            return PyErr_Format (PyExc_TypeError, "Invalid PixelType object");
        }
        pt = PixelType (
            PyLong_AsLong (PyObject_StealAttrString (pixel_type, "v")));
    }
    else { pt = channelPtr->type; }

    int xSampling = channelPtr->xSampling;
    int ySampling = channelPtr->ySampling;
    int width     = (dw.max.x - dw.min.x + 1) / xSampling;
    int height    = (maxy - miny + 1) / ySampling;

    size_t typeSize;
    switch (pt)
    {
        case HALF: typeSize = 2; break;

        case FLOAT:
        case UINT: typeSize = 4; break;

        default: PyErr_SetString (PyExc_TypeError, "Unknown type"); return NULL;
    }
    PyObject* r = PyString_FromStringAndSize (NULL, typeSize * width * height);

    char* pixels = PyString_AsString (r);

    try
    {
        FrameBuffer frameBuffer;
        size_t      xstride = typeSize;
        size_t      ystride = typeSize * width;
        frameBuffer.insert (
            cname,
            Slice (
                pt,
                pixels - dw.min.x * xstride / xSampling -
                    miny * ystride / ySampling,
                xstride,
                ystride,
                xSampling,
                ySampling,
                0.0));
        file->setFrameBuffer (frameBuffer);
        file->readPixels (miny, maxy);
    }
    catch (const std::exception& e)
    {
        PyErr_SetString (PyExc_IOError, e.what ());
        return NULL;
    }

    return r;
}

static PyObject*
channels (PyObject* self, PyObject* args, PyObject* kw)
{
    InputFile* file = &((InputFileC*) self)->i;

    Box2i dw = file->header ().dataWindow ();
    int   miny, maxy;

    miny = dw.min.y;
    maxy = dw.max.y;

    PyObject* clist;
    PyObject* pixel_type = NULL;
    char*     keywords[] = {
        (char*) "cnames",
        (char*) "pixel_type",
        (char*) "scanLine1",
        (char*) "scanLine2",
        NULL};
    if (!PyArg_ParseTupleAndKeywords (
            args, kw, "O|Oii", keywords, &clist, &pixel_type, &miny, &maxy))
        return NULL;

    if (maxy < miny)
    {
        PyErr_SetString (PyExc_TypeError, "scanLine1 must be <= scanLine2");
        return NULL;
    }
    if (miny < dw.min.y)
    {
        PyErr_SetString (
            PyExc_TypeError, "scanLine1 cannot be outside dataWindow");
        return NULL;
    }
    if (maxy > dw.max.y)
    {
        PyErr_SetString (
            PyExc_TypeError, "scanLine2 cannot be outside dataWindow");
        return NULL;
    }

    ChannelList channels = file->header ().channels ();
    FrameBuffer frameBuffer;

    int width  = dw.max.x - dw.min.x + 1;
    int height = maxy - miny + 1;

    PyObject* retval   = PyList_New (0);
    PyObject* iterator = PyObject_GetIter (clist);
    if (iterator == NULL)
    {
        PyErr_SetString (PyExc_TypeError, "Channel list must be iterable");
        return NULL;
    }
    PyObject* item;

    while ((item = PyIter_Next (iterator)) != NULL)
    {
        char*    cname      = PyUTF8_AsSstring (item);
        Channel* channelPtr = channels.findChannel (cname);
        if (channelPtr == NULL)
        {
            return PyErr_Format (
                PyExc_TypeError,
                "There is no channel '%s' in the image",
                cname);
        }

        Imf::PixelType pt;
        if (pixel_type != NULL)
        {
            pt = PixelType (
                PyLong_AsLong (PyObject_StealAttrString (pixel_type, "v")));
        }
        else { pt = channelPtr->type; }

        // Use pt to compute typeSize
        size_t typeSize;
        switch (pt)
        {
            case HALF: typeSize = 2; break;

            case FLOAT:
            case UINT: typeSize = 4; break;

            default:
                PyErr_SetString (PyExc_TypeError, "Unknown type");
                return NULL;
        }

        size_t xstride = typeSize;
        size_t ystride = typeSize * width;

        PyObject* r =
            PyString_FromStringAndSize (NULL, typeSize * width * height);
        PyList_Append (retval, r);
        Py_DECREF (r);

        char* pixels = PyString_AsString (r);

        try
        {
            frameBuffer.insert (
                cname,
                Slice (
                    pt,
                    pixels - dw.min.x * xstride - miny * ystride,
                    xstride,
                    ystride,
                    1,
                    1,
                    0.0));
        }
        catch (const std::exception& e)
        {
            PyErr_SetString (PyExc_IOError, e.what ());
            return NULL;
        }
        Py_DECREF (item);
    }
    Py_DECREF (iterator);
    file->setFrameBuffer (frameBuffer);
    file->readPixels (miny, maxy);

    return retval;
}
static PyObject*
inclose (PyObject* self, PyObject* args)
{
    InputFileC* pc = ((InputFileC*) self);
    if (pc->is_opened)
    {
        pc->is_opened   = 0;
        InputFile* file = &((InputFileC*) self)->i;
        file->~InputFile ();
    }
    Py_RETURN_NONE;
}

static PyObject*
dict_from_header (Header h)
{
    PyObject* object;

    object = PyDict_New ();

    PyObject* pV2FFunc   = PyObject_GetAttrString (pModuleImath, "V2f");
    PyObject* pChanFunc  = PyObject_GetAttrString (pModuleImath, "Channel");
    PyObject* pPTFunc    = PyObject_GetAttrString (pModuleImath, "PixelType");
    PyObject* pBoxFunc   = PyObject_GetAttrString (pModuleImath, "Box2i");
    PyObject* pPointFunc = PyObject_GetAttrString (pModuleImath, "point");
    PyObject* pPIFunc = PyObject_GetAttrString (pModuleImath, "PreviewImage");
    PyObject* pLOFunc = PyObject_GetAttrString (pModuleImath, "LineOrder");
    PyObject* pCFunc  = PyObject_GetAttrString (pModuleImath, "Compression");
    PyObject* pCHFunc = PyObject_GetAttrString (pModuleImath, "chromaticity");
    PyObject* pCHSFunc =
        PyObject_GetAttrString (pModuleImath, "Chromaticities");
    PyObject* pLevelMode = PyObject_GetAttrString (pModuleImath, "LevelMode");
    PyObject* pLevelRoundingMode =
        PyObject_GetAttrString (pModuleImath, "LevelRoundingMode");
    PyObject* pTileDescription =
        PyObject_GetAttrString (pModuleImath, "TileDescription");
    PyObject* pRationalFunc = PyObject_GetAttrString (pModuleImath, "Rational");
    PyObject* pKeyCodeFunc  = PyObject_GetAttrString (pModuleImath, "KeyCode");
    PyObject* pTimeCodeFunc = PyObject_GetAttrString (pModuleImath, "TimeCode");

    for (Header::ConstIterator i = h.begin (); i != h.end (); ++i)
    {
        const Attribute* a  = &i.attribute ();
        PyObject*        ob = NULL;

        // cout << i.name() << " (type " << a->typeName() << ")\n";
        if (const Box2iAttribute* ta = dynamic_cast<const Box2iAttribute*> (a))
        {

            PyObject* ptargs[2];
            ptargs[0] =
                Py_BuildValue ("ii", ta->value ().min.x, ta->value ().min.y);
            ptargs[1] =
                Py_BuildValue ("ii", ta->value ().max.x, ta->value ().max.y);
            PyObject* pt[2];
            pt[0]             = PyObject_CallObject (pPointFunc, ptargs[0]);
            pt[1]             = PyObject_CallObject (pPointFunc, ptargs[1]);
            PyObject* boxArgs = Py_BuildValue ("NN", pt[0], pt[1]);

            ob = PyObject_CallObject (pBoxFunc, boxArgs);
            Py_DECREF (boxArgs);
            Py_DECREF (ptargs[0]);
            Py_DECREF (ptargs[1]);
        }
        else if (
            const KeyCodeAttribute* ka =
                dynamic_cast<const KeyCodeAttribute*> (a))
        {
            PyObject* args = Py_BuildValue (
                "iiiiiii",
                ka->value ().filmMfcCode (),
                ka->value ().filmType (),
                ka->value ().prefix (),
                ka->value ().count (),
                ka->value ().perfOffset (),
                ka->value ().perfsPerFrame (),
                ka->value ().perfsPerCount ());
            ob = PyObject_CallObject (pKeyCodeFunc, args);
            Py_DECREF (args);
        }
        else if (
            const TimeCodeAttribute* ta =
                dynamic_cast<const TimeCodeAttribute*> (a))
        {
            PyObject* args = Py_BuildValue (
                "iiiiiiiiiiiiiiiiii",
                ta->value ().hours (),
                ta->value ().minutes (),
                ta->value ().seconds (),
                ta->value ().frame (),
                ta->value ().dropFrame (),
                ta->value ().colorFrame (),
                ta->value ().fieldPhase (),
                ta->value ().bgf0 (),
                ta->value ().bgf1 (),
                ta->value ().bgf2 (),
                ta->value ().binaryGroup (1),
                ta->value ().binaryGroup (2),
                ta->value ().binaryGroup (3),
                ta->value ().binaryGroup (4),
                ta->value ().binaryGroup (5),
                ta->value ().binaryGroup (6),
                ta->value ().binaryGroup (7),
                ta->value ().binaryGroup (8));
            ob = PyObject_CallObject (pTimeCodeFunc, args);
            Py_DECREF (args);
        }
        else if (
            const RationalAttribute* ra =
                dynamic_cast<const RationalAttribute*> (a))
        {
            PyObject* args =
                Py_BuildValue ("ii", ra->value ().n, ra->value ().d);
            ob = PyObject_CallObject (pRationalFunc, args);
            Py_DECREF (args);
        }
        else if (
            const PreviewImageAttribute* pia =
                dynamic_cast<const PreviewImageAttribute*> (a))
        {
            int size = pia->value ().width () * pia->value ().height () * 4;
#if PY_MAJOR_VERSION >= 3
            const char fmt[] = "iiy#";
#else
            const char fmt[] = "iis#";
#endif
            PyObject* args = Py_BuildValue (
                fmt,
                pia->value ().width (),
                pia->value ().height (),
                (char*) pia->value ().pixels (),
                size);
            ob = PyObject_CallObject (pPIFunc, args);

            Py_DECREF (args);
        }
        else if (
            const LineOrderAttribute* ta =
                dynamic_cast<const LineOrderAttribute*> (a))
        {
            PyObject* args = PyTuple_Pack (1, PyInt_FromLong (ta->value ()));
            ob             = PyObject_CallObject (pLOFunc, args);
            Py_DECREF (args);
        }
        else if (
            const CompressionAttribute* ta =
                dynamic_cast<const CompressionAttribute*> (a))
        {
            PyObject* args = PyTuple_Pack (1, PyInt_FromLong (ta->value ()));
            ob             = PyObject_CallObject (pCFunc, args);
            Py_DECREF (args);
        }
        else if (
            const ChannelListAttribute* ta =
                dynamic_cast<const ChannelListAttribute*> (a))
        {
            const ChannelList cl = ta->value ();
            PyObject*         CS = PyDict_New ();
            for (ChannelList::ConstIterator j = cl.begin (); j != cl.end ();
                 ++j)
            {
                PyObject* ptarg   = Py_BuildValue ("(i)", j.channel ().type);
                PyObject* pt      = PyObject_CallObject (pPTFunc, ptarg);
                PyObject* chanarg = Py_BuildValue (
                    "Nii", pt, j.channel ().xSampling, j.channel ().ySampling);
                PyObject* C = PyObject_CallObject (pChanFunc, chanarg);
                PyDict_SetItemString (CS, j.name (), C);
                Py_DECREF (C);
                Py_DECREF (ptarg);
                Py_DECREF (chanarg);
            }
            ob = CS;
        }
        else if (
            const FloatAttribute* ta = dynamic_cast<const FloatAttribute*> (a))
        {
            ob = PyFloat_FromDouble (ta->value ());
        }
        else if (const IntAttribute* ta = dynamic_cast<const IntAttribute*> (a))
        {
            ob = PyInt_FromLong (ta->value ());
        }
        else if (const V2fAttribute* ta = dynamic_cast<const V2fAttribute*> (a))
        {
            PyObject* args =
                Py_BuildValue ("ff", ta->value ().x, ta->value ().y);
            ob = PyObject_CallObject (pV2FFunc, args);
            Py_DECREF (args);
        }
        else if (
            const StringAttribute* ta =
                dynamic_cast<const StringAttribute*> (a))
        {
            ob = PyString_FromString (ta->value ().c_str ());
        }
        else if (
            const TileDescriptionAttribute* ta =
                dynamic_cast<const TileDescriptionAttribute*> (a))
        {
            const TileDescription td = ta->value ();
            PyObject*             m =
                PyObject_Call1 (pLevelMode, Py_BuildValue ("(i)", td.mode));
            PyObject* r = PyObject_Call1 (
                pLevelRoundingMode, Py_BuildValue ("(i)", td.roundingMode));
            ob = PyObject_Call1 (
                pTileDescription,
                Py_BuildValue ("(iiNN)", td.xSize, td.ySize, m, r));
        }
        else if (
            const ChromaticitiesAttribute* ta =
                dynamic_cast<const ChromaticitiesAttribute*> (a))
        {
            const Chromaticities& ch (ta->value ());
            PyObject*             rgbwargs[4];
            rgbwargs[0] = Py_BuildValue ("ff", ch.red[0], ch.red[1]);
            rgbwargs[1] = Py_BuildValue ("ff", ch.green[0], ch.green[1]);
            rgbwargs[2] = Py_BuildValue ("ff", ch.blue[0], ch.blue[1]);
            rgbwargs[3] = Py_BuildValue ("ff", ch.white[0], ch.white[1]);
            PyObject* chromas[4];
            chromas[0]      = PyObject_CallObject (pCHFunc, rgbwargs[0]);
            chromas[1]      = PyObject_CallObject (pCHFunc, rgbwargs[1]);
            chromas[2]      = PyObject_CallObject (pCHFunc, rgbwargs[2]);
            chromas[3]      = PyObject_CallObject (pCHFunc, rgbwargs[3]);
            PyObject* cargs = Py_BuildValue (
                "NNNN", chromas[0], chromas[1], chromas[2], chromas[3]);
            ob = PyObject_CallObject (pCHSFunc, cargs);
            Py_DECREF (cargs);
            Py_DECREF (rgbwargs[0]);
            Py_DECREF (rgbwargs[1]);
            Py_DECREF (rgbwargs[2]);
            Py_DECREF (rgbwargs[3]);
#ifdef INCLUDED_IMF_STRINGVECTOR_ATTRIBUTE_H
        }
        else if (
            const StringVectorAttribute* ta =
                dynamic_cast<const StringVectorAttribute*> (a))
        {
            StringVector sv = ta->value ();
            ob              = PyList_New (sv.size ());
            for (size_t i = 0; i < sv.size (); i++)
                PyList_SetItem (ob, i, PyString_FromString (sv[i].c_str ()));
#endif
        }
        else
        {
            // Unknown type for this object, so set its value to None.
            // printf("Baffled by type %s\n", a->typeName());
            ob = Py_None;
            Py_INCREF (ob);
        }
        PyDict_SetItemString (object, i.name (), ob);
        Py_DECREF (ob);
    }

    Py_DECREF (pV2FFunc);
    Py_DECREF (pChanFunc);
    Py_DECREF (pPTFunc);
    Py_DECREF (pBoxFunc);
    Py_DECREF (pPointFunc);
    Py_DECREF (pPIFunc);
    Py_DECREF (pLOFunc);
    Py_DECREF (pCFunc);
    Py_DECREF (pLevelMode);
    Py_DECREF (pLevelRoundingMode);
    Py_DECREF (pTileDescription);
    Py_DECREF (pRationalFunc);
    Py_DECREF (pKeyCodeFunc);
    Py_DECREF (pTimeCodeFunc);

    return object;
}

static PyObject*
inheader (PyObject* self, PyObject* args)
{
    InputFile* file = &((InputFileC*) self)->i;
    return dict_from_header (file->header ());
}

static PyObject*
isComplete (PyObject* self, PyObject* args)
{
    InputFile* file = &((InputFileC*) self)->i;
    return PyBool_FromLong (file->isComplete ());
}

/* Method table */
static PyMethodDef InputFile_methods[] = {
    {"header", inheader, METH_VARARGS},
    {"channel", (PyCFunction) channel, METH_VARARGS | METH_KEYWORDS},
    {"channels", (PyCFunction) channels, METH_VARARGS | METH_KEYWORDS},
    {"close", inclose, METH_VARARGS},
    {"isComplete", isComplete, METH_VARARGS},
    {NULL, NULL},
};

static void
InputFile_dealloc (PyObject* self)
{
    InputFileC* object = ((InputFileC*) self);
    if (object->fo) Py_DECREF (object->fo);
    Py_DECREF (inclose (self, NULL));
    PyObject_Del (self);
}

static PyObject*
InputFile_Repr (PyObject* self)
{
    //PyObject *result = NULL;
    char buf[50];

    sprintf (buf, "InputFile represented");
    return PyUnicode_FromString (buf);
}

static PyTypeObject InputFile_Type = {
    PyVarObject_HEAD_INIT (&PyType_Type, 0) "OpenEXR.InputFile",
    sizeof (InputFileC),
    0,
    (destructor) InputFile_dealloc,
    0,
    0,
    0,
    0,
    (reprfunc) InputFile_Repr,
    0, //&InputFile_as_number,
    0, //&InputFile_as_sequence,
    0,

    0,
    0,
    0,
    0,
    0,

    0,

    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,

    "OpenEXR Input file object",

    0,
    0,
    0,
    0,
    0,
    0,

    InputFile_methods

    /* the rest are NULLs */
};

int
makeInputFile (PyObject* self, PyObject* args, PyObject* kwds)
{
    InputFileC* object = ((InputFileC*) self);
    PyObject*   fo;
    char*       filename = NULL;

    if (PyArg_ParseTuple (args, "O:InputFile", &fo))
    {
        if (PyString_Check (fo))
        {
            filename        = PyString_AsString (fo);
            object->fo      = NULL;
            object->istream = NULL;
        }
        else if (PyUnicode_Check (fo))
        {
            filename        = PyUTF8_AsSstring (fo);
            object->fo      = NULL;
            object->istream = NULL;
        }
        else
        {
            object->fo = fo;
            Py_INCREF (fo);
            object->istream = new C_IStream (fo);
        }
    }
    else { return -1; }

    try
    {
        if (filename != NULL)
            new (&object->i) InputFile (filename);
        else
            new (&object->i) InputFile (*object->istream);
    }
    catch (const std::exception& e)
    {
        // Py_DECREF(object);
        PyErr_SetString (PyExc_IOError, e.what ());
        return -1;
    }
    object->is_opened = 1;

    return 0;
}

////////////////////////////////////////////////////////////////////////
//    OutputFile
////////////////////////////////////////////////////////////////////////

typedef struct
{
    PyObject_HEAD OutputFile o;
    C_OStream*               ostream;
    PyObject*                fo;
    int                      is_opened;
} OutputFileC;

static void
releaseviews (std::vector<Py_buffer>& views)
{
    for (size_t i = 0; i < views.size (); i++)
        PyBuffer_Release (&views[i]);
}

static PyObject*
outwrite (PyObject* self, PyObject* args)
{
    OutputFile* file = &((OutputFileC*) self)->o;

    // long height = PyLong_AsLong(PyTuple_GetItem(args, 1));
    Box2i     dw     = file->header ().dataWindow ();
    int       width  = dw.max.x - dw.min.x + 1;
    int       height = dw.max.y - dw.min.y + 1;
    PyObject* pixeldata;

    if (!PyArg_ParseTuple (
            args, "O!|i:writePixels", &PyDict_Type, &pixeldata, &height))
        return NULL;

    int currentScanLine = file->currentScanLine ();
    if (file->header ().lineOrder () == DECREASING_Y)
    {
        // With DECREASING_Y, currentScanLine() returns the maximum Y value of
        // the window on the first call, and decrements at each scan line.
        // We have to adjust to point to the correct address in the client buffer.
        currentScanLine = dw.max.y - currentScanLine + dw.min.y;
    }

    FrameBuffer            frameBuffer;
    std::vector<Py_buffer> views;

    const ChannelList& channels = file->header ().channels ();
    for (ChannelList::ConstIterator i = channels.begin (); i != channels.end ();
         ++i)
    {
        PyObject* channel_spec =
            PyDict_GetItem (pixeldata, PyUnicode_FromString (i.name ()));
        if (channel_spec != NULL)
        {
            Imf::PixelType pt       = i.channel ().type;
            int            typeSize = 4;
            switch (pt)
            {
                case HALF: typeSize = 2; break;

                case FLOAT:
                case UINT: typeSize = 4; break;

                default: break;
            }
            int   xSampling = i.channel ().xSampling;
            int   ySampling = i.channel ().ySampling;
            int   yStride   = typeSize * width;
            char* srcPixels;
            int   expectedSize = (height * yStride) / (xSampling * ySampling);
            Py_ssize_t bufferSize;

            if (PyString_Check (channel_spec))
            {
                bufferSize = PyString_Size (channel_spec);
                srcPixels  = PyString_AsString (channel_spec);
            }
            else if (PyObject_CheckBuffer (channel_spec))
            {
                Py_buffer view;
                if (PyObject_GetBuffer (channel_spec, &view, PyBUF_CONTIG_RO) !=
                    0)
                {
                    releaseviews (views);
                    PyErr_Format (
                        PyExc_TypeError,
                        "Unsupported buffer structure for channel '%s'",
                        i.name ());
                    return NULL;
                }
                views.push_back (view);
                bufferSize = view.len;
                srcPixels  = (char*) view.buf;
            }
            else
            {
                releaseviews (views);
                PyErr_Format (
                    PyExc_TypeError,
                    "Data for channel '%s' must be a string or support buffer protocol",
                    i.name ());
                return NULL;
            }

            if (bufferSize != expectedSize)
            {
                releaseviews (views);
                PyErr_Format (
                    PyExc_TypeError,
                    "Data for channel '%s' should have size %zu but got %zu",
                    i.name (),
                    expectedSize,
                    bufferSize);
                return NULL;
            }

            frameBuffer.insert (
                i.name (), // name
                Slice (
                    pt, // type
                    srcPixels - dw.min.x * typeSize / xSampling -
                        currentScanLine * yStride / ySampling, // base
                    typeSize,                                  // xStride
                    yStride,                                   // yStride
                    xSampling,
                    ySampling)); // subsampling
        }
    }

    try
    {
        file->setFrameBuffer (frameBuffer);
        file->writePixels (height);
    }
    catch (const std::exception& e)
    {
        releaseviews (views);
        PyErr_SetString (PyExc_IOError, e.what ());
        return NULL;
    }
    releaseviews (views);
    Py_RETURN_NONE;
}

static PyObject*
outcurrentscanline (PyObject* self, PyObject* args)
{
    OutputFile* file = &((OutputFileC*) self)->o;
    return PyLong_FromLong (file->currentScanLine ());
}

static PyObject*
outclose (PyObject* self, PyObject* args)
{
    OutputFileC* oc = (OutputFileC*) self;
    if (oc->is_opened)
    {
        oc->is_opened    = 0;
        OutputFile* file = &oc->o;
        file->~OutputFile ();
    }
    Py_RETURN_NONE;
}

/* Method table */
static PyMethodDef OutputFile_methods[] = {
    {"writePixels", outwrite, METH_VARARGS},
    {"currentScanLine", outcurrentscanline, METH_VARARGS},
    {"close", outclose, METH_VARARGS},
    {NULL, NULL},
};

static void
OutputFile_dealloc (PyObject* self)
{
    OutputFileC* object = ((OutputFileC*) self);
    if (object->fo) Py_DECREF (object->fo);
    Py_DECREF (outclose (self, NULL));
    PyObject_Del (self);
}

static PyObject*
OutputFile_Repr (PyObject* self)
{
    //PyObject *result = NULL;
    char buf[50];

    sprintf (buf, "OutputFile represented");
    return PyUnicode_FromString (buf);
}

static PyTypeObject OutputFile_Type = {
    PyVarObject_HEAD_INIT (&PyType_Type, 0) "OpenEXR.OutputFile",
    sizeof (OutputFileC),
    0,
    (destructor) OutputFile_dealloc,
    0,
    0,
    0,
    0,
    (reprfunc) OutputFile_Repr,
    0, //&InputFile_as_number,
    0, //&InputFile_as_sequence,
    0,

    0,
    0,
    0,
    0,
    0,

    0,

    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,

    "OpenEXR Output file object",

    0,
    0,
    0,
    0,
    0,
    0,

    OutputFile_methods

    /* the rest are NULLs */
};

int
makeOutputFile (PyObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* fo;
    PyObject* header_dict;

    char* filename = NULL;

    OutputFileC* object = (OutputFileC*) self;

    if (PyArg_ParseTuple (
            args, "OO!:OutputFile", &fo, &PyDict_Type, &header_dict))
    {
        if (PyString_Check (fo))
        {
            filename        = PyString_AsString (fo);
            object->fo      = NULL;
            object->ostream = NULL;
        }
        else if (PyUnicode_Check (fo))
        {
            filename        = PyUTF8_AsSstring (fo);
            object->fo      = NULL;
            object->ostream = NULL;
        }
        else
        {
            object->fo = fo;
            Py_INCREF (fo);
            object->ostream = new C_OStream (fo);
        }
    }
    else { return -1; }

    Header header (64, 64);

    PyObject* pB2i  = PyObject_GetAttrString (pModuleImath, "Box2i");
    PyObject* pB2f  = PyObject_GetAttrString (pModuleImath, "Box2f");
    PyObject* pV2f  = PyObject_GetAttrString (pModuleImath, "V2f");
    PyObject* pLO   = PyObject_GetAttrString (pModuleImath, "LineOrder");
    PyObject* pCOMP = PyObject_GetAttrString (pModuleImath, "Compression");
    PyObject* pPI   = PyObject_GetAttrString (pModuleImath, "PreviewImage");
    PyObject* pCH   = PyObject_GetAttrString (pModuleImath, "Chromaticities");
    PyObject* pTD   = PyObject_GetAttrString (pModuleImath, "TileDescription");
    PyObject* pRA   = PyObject_GetAttrString (pModuleImath, "Rational");
    PyObject* pKA   = PyObject_GetAttrString (pModuleImath, "KeyCode");
    PyObject* pTC   = PyObject_GetAttrString (pModuleImath, "TimeCode");

    Py_ssize_t pos = 0;
    PyObject * key, *value;

    while (PyDict_Next (header_dict, &pos, &key, &value))
    {
        const char* ks = PyUTF8_AsSstring (key);
        if (PyFloat_Check (value))
        {
            header.insert (ks, FloatAttribute (PyFloat_AsDouble (value)));
        }
        else if (PyInt_Check (value))
        {
            header.insert (ks, IntAttribute (PyInt_AsLong (value)));
        }
        else if (PyBytes_Check (value))
        {
            header.insert (ks, StringAttribute (PyString_AsString (value)));
        }
        else if (PyObject_IsInstance (value, pB2i))
        {
            Box2i box (
                V2i (
                    PyLong_AsLong (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "min"), "x")),
                    PyLong_AsLong (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "min"), "y"))),
                V2i (
                    PyLong_AsLong (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "max"), "x")),
                    PyLong_AsLong (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "max"), "y"))));
            header.insert (ks, Box2iAttribute (box));
        }
        else if (PyObject_IsInstance (value, pB2f))
        {
            Box2f box (
                V2f (
                    PyFloat_AsDouble (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "min"), "x")),
                    PyFloat_AsDouble (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "min"), "y"))),
                V2f (
                    PyFloat_AsDouble (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "max"), "x")),
                    PyFloat_AsDouble (PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "max"), "y"))));
            header.insert (ks, Box2fAttribute (box));
        }
        else if (PyObject_IsInstance (value, pPI))
        {
            PreviewImage pi (
                PyLong_AsLong (PyObject_StealAttrString (value, "width")),
                PyLong_AsLong (PyObject_StealAttrString (value, "height")),
                (Imf::PreviewRgba*) PyString_AsString (
                    PyObject_StealAttrString (value, "pixels")));
            header.insert (ks, PreviewImageAttribute (pi));
        }
        else if (PyObject_IsInstance (value, pV2f))
        {
            V2f v (
                PyFloat_AsDouble (PyObject_StealAttrString (value, "x")),
                PyFloat_AsDouble (PyObject_StealAttrString (value, "y")));

            header.insert (ks, V2fAttribute (v));
        }
        else if (PyObject_IsInstance (value, pLO))
        {
            LineOrder i = (LineOrder) PyInt_AsLong (
                PyObject_StealAttrString (value, "v"));

            header.insert (ks, LineOrderAttribute (i));
        }
        else if (PyObject_IsInstance (value, pTC))
        {
            TimeCode v (
                PyLong_AsLong (PyObject_StealAttrString (value, "hours")),
                PyLong_AsLong (PyObject_StealAttrString (value, "minutes")),
                PyLong_AsLong (PyObject_StealAttrString (value, "seconds")),
                PyLong_AsLong (PyObject_StealAttrString (value, "frame")),
                PyLong_AsLong (PyObject_StealAttrString (value, "dropFrame")),
                PyLong_AsLong (PyObject_StealAttrString (value, "colorFrame")),
                PyLong_AsLong (PyObject_StealAttrString (value, "fieldPhase")),
                PyLong_AsLong (PyObject_StealAttrString (value, "bgf0")),
                PyLong_AsLong (PyObject_StealAttrString (value, "bgf1")),
                PyLong_AsLong (PyObject_StealAttrString (value, "bgf2")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup1")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup2")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup3")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup4")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup5")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup6")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup7")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "binaryGroup8")));

            header.insert (ks, TimeCodeAttribute (v));
        }
        else if (PyObject_IsInstance (value, pKA))
        {
            KeyCode v (
                PyLong_AsLong (PyObject_StealAttrString (value, "filmMfcCode")),
                PyLong_AsLong (PyObject_StealAttrString (value, "filmType")),
                PyLong_AsLong (PyObject_StealAttrString (value, "prefix")),
                PyLong_AsLong (PyObject_StealAttrString (value, "count")),
                PyLong_AsLong (PyObject_StealAttrString (value, "perfOffset")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "perfsPerFrame")),
                PyLong_AsLong (
                    PyObject_StealAttrString (value, "perfsPerCount")));
            header.insert (ks, KeyCodeAttribute (v));
        }
        else if (PyObject_IsInstance (value, pRA))
        {
            Rational v (
                PyLong_AsLong (PyObject_StealAttrString (value, "n")),
                PyLong_AsLong (PyObject_StealAttrString (value, "d")));
            header.insert (ks, RationalAttribute (v));
        }
        else if (PyObject_IsInstance (value, pCOMP))
        {
            Compression i = (Compression) PyInt_AsLong (
                PyObject_StealAttrString (value, "v"));

            header.insert (ks, CompressionAttribute (i));
        }
        else if (PyObject_IsInstance (value, pCH))
        {
            V2f red (
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "red"), "x")),
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "red"), "y")));
            V2f green (
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "green"), "x")),
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "green"), "y")));
            V2f blue (
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "blue"), "x")),
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "blue"), "y")));
            V2f white (
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "white"), "x")),
                PyFloat_AsDouble (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "white"), "y")));
            Chromaticities c (red, green, blue, white);
            header.insert (ks, ChromaticitiesAttribute (c));
        }
        else if (PyObject_IsInstance (value, pTD))
        {
            TileDescription td (
                PyInt_AsLong (PyObject_StealAttrString (value, "xSize")),
                PyInt_AsLong (PyObject_StealAttrString (value, "ySize")),
                (Imf::LevelMode) PyInt_AsLong (PyObject_StealAttrString (
                    PyObject_StealAttrString (value, "mode"), "v")),
                (Imf::LevelRoundingMode) PyInt_AsLong (
                    PyObject_StealAttrString (
                        PyObject_StealAttrString (value, "roundingMode"),
                        "v")));
            header.insert (ks, TileDescriptionAttribute (td));
        }
        else if (PyDict_Check (value))
        {
            PyObject * key2, *value2;
            Py_ssize_t pos2 = 0;

            while (PyDict_Next (value, &pos2, &key2, &value2))
            {
                if (0)
                    printf (
                        "%s -> %s\n",
                        PyString_AsString (key2),
                        PyString_AsString (
                            PyObject_Str (PyObject_Type (value2))));
                header.channels ().insert (
                    PyUTF8_AsSstring (key2),
                    Channel (
                        PixelType (PyLong_AsLong (PyObject_StealAttrString (
                            PyObject_StealAttrString (value2, "type"), "v"))),
                        PyLong_AsLong (
                            PyObject_StealAttrString (value2, "xSampling")),
                        PyLong_AsLong (
                            PyObject_StealAttrString (value2, "ySampling"))));
            }
#ifdef INCLUDED_IMF_STRINGVECTOR_ATTRIBUTE_H
        }
        else if (PyList_Check (value))
        {
            StringVector sv (PyList_Size (value));
            for (size_t i = 0; i < sv.size (); i++)
                sv[i] = PyUTF8_AsSstring (PyList_GetItem (value, i));
            header.insert (ks, StringVectorAttribute (sv));
#endif
        }
        else
        {
            printf (
                "XXX - unknown attribute: %s\n",
                PyUTF8_AsSstring (PyObject_Str (key)));
        }
    }

    Py_DECREF (pB2i);
    Py_DECREF (pB2f);
    Py_DECREF (pV2f);
    Py_DECREF (pLO);
    Py_DECREF (pCOMP);
    Py_DECREF (pPI);
    Py_DECREF (pCH);
    Py_DECREF (pTD);
    Py_DECREF (pRA);
    Py_DECREF (pKA);
    Py_DECREF (pTC);

    try
    {
        if (filename != NULL)
            new (&object->o) OutputFile (filename, header);
        else
            new (&object->o) OutputFile (*object->ostream, header);
    }
    catch (const std::exception& e)
    {
        PyErr_SetString (PyExc_IOError, e.what ());
        return -1;
    }
    object->is_opened = 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////

PyObject*
makeHeader (PyObject* self, PyObject* args)
{
    int         w, h;
    const char* channels = "R,G,B";
    if (!PyArg_ParseTuple (args, "ii|s:Header", &w, &h, &channels)) return NULL;
    Header header (w, h);
    for (auto channel: split (channels, ','))
    {
        header.channels ().insert (channel.c_str (), Channel (FLOAT));
    }
    return dict_from_header (header);
}

////////////////////////////////////////////////////////////////////////

static bool
isOpenExrFile (const char fileName[])
{
    std::ifstream f (fileName, std::ios_base::binary);
    char          bytes[4];
    f.read (bytes, sizeof (bytes));
    return !!f && Imf::isImfMagic (bytes);
}

PyObject*
_isOpenExrFile (PyObject* self, PyObject* args)
{
    char* filename;
    if (!PyArg_ParseTuple (args, "s:isOpenExrFile", &filename)) return NULL;
    return PyBool_FromLong (isOpenExrFile (filename));
}

#ifdef VERSION_HAS_ISTILED
PyObject*
_isTiledOpenExrFile (PyObject* self, PyObject* args)
{
    char* filename;
    if (!PyArg_ParseTuple (args, "s:isTiledOpenExrFile", &filename))
        return NULL;
    return PyBool_FromLong (isTiledOpenExrFile (filename));
}
#endif

////////////////////////////////////////////////////////////////////////

static PyMethodDef methods[] = {
    {"Header", makeHeader, METH_VARARGS},
    {"isOpenExrFile", _isOpenExrFile, METH_VARARGS},
#ifdef VERSION_HAS_ISTILED
    {"isTiledOpenExrFile", _isTiledOpenExrFile, METH_VARARGS},
#endif
    {NULL, NULL},
};

MOD_INIT (OpenEXR)
{
    PyObject *m, *d, *item;

    Imf::staticInitialize ();

    MOD_DEF (m, "OpenEXR", "", methods)
    d = PyModule_GetDict (m);

    pModuleImath = PyImport_ImportModule ("Imath");

    /* initialize module variables/constants */
    InputFile_Type.tp_new   = PyType_GenericNew;
    InputFile_Type.tp_init  = makeInputFile;
    OutputFile_Type.tp_new  = PyType_GenericNew;
    OutputFile_Type.tp_init = makeOutputFile;
    if (PyType_Ready (&InputFile_Type) != 0) return MOD_ERROR_VAL;
    if (PyType_Ready (&OutputFile_Type) != 0) return MOD_ERROR_VAL;
    PyModule_AddObject (m, "InputFile", (PyObject*) &InputFile_Type);
    PyModule_AddObject (m, "OutputFile", (PyObject*) &OutputFile_Type);

#if PYTHON_API_VERSION >= 1007
    OpenEXR_error = PyErr_NewException ((char*) "OpenEXR.error", NULL, NULL);
#else
    OpenEXR_error = PyString_FromString ("OpenEXR.error");
#endif
    PyDict_SetItemString (d, "error", OpenEXR_error);
    Py_DECREF (OpenEXR_error);

    PyDict_SetItemString (d, "UINT", item = PyLong_FromLong (UINT));
    Py_DECREF (item);
    PyDict_SetItemString (d, "HALF", item = PyLong_FromLong (HALF));
    Py_DECREF (item);
    PyDict_SetItemString (d, "FLOAT", item = PyLong_FromLong (FLOAT));
    Py_DECREF (item);

    return MOD_SUCCESS_VAL (m);
}

#endif
