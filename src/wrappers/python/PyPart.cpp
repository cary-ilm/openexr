//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#include "PyFile.h"
#include "PyPart.h"
#include "PyChannel.h"
#include "PyAttributes.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/eval.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

PyPart::PyPart(const char* name_arg, const py::dict& header_arg, const py::list& channels_arg,
               exr_storage_t type_arg, exr_compression_t compression_arg)
    : name(name_arg), type(type_arg), width(0), height(0),
      compression(compression_arg),
      header(header_arg), channels(channels_arg)
{
    //
    // Confirm all the channels have 2 dimensions and the same size
    //
    
    
    for (auto c : channels)
    {
        auto o = *c;
        auto C = py::cast<PyChannel>(*o);

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
    
    if (!header.contains("dataWindow"))
        header["dataWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));

    if (!header.contains("displayWindow"))
        header["displayWindow"] = Box2i(V2i(0,0), V2i(width-1,height-1));
}
    
//
// Read scanline data from a file
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
        int              y     = ((int) chunk) + datawin.min.y;

        rv = exr_read_scanline_chunk_info (f, part_index, y, &cinfo);
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
            // Initialize the channel list with empty data, to be filled in below.
            //
            
            channels = py::list();
            for (int c = 0; c < decoder.channel_count; c++)
                channels.append(PyChannel());
            
            //
            // Build the channel list for the decoder and allocate each
            // channel's pixel data arrays. The decoder's array of
            // channels parallels the channels py::list, so
            // iterate through both in lock step.
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
                C.xSampling  = outc.x_samples;
                C.ySampling  = outc.y_samples;
                
                std::vector<size_t> shape, strides;
                shape.assign({ height, width });

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
            auto c_iterator = channels.begin();
            for (int16_t c = 0; c < decoder.channel_count; c++, c_iterator++)
            {
                PyChannel& C = py::cast<PyChannel&>(*c_iterator);
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

        
        rv = exr_decoding_run (f, part_index, &decoder);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error("error in decoder");
    }

    exr_decoding_destroy (f, &decoder);
}

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
                        // Initialize the channel list with empty data, to be filled in below.
                        //
            
                        channels = py::list();
                        for (int c = 0; c < decoder.channel_count; c++)
                            channels.append(PyChannel());

                        //
                        // Build the channel list for the decoder and allocate each
                        // channel's pixel data arrays. The decoder's array of
                        // channels parallels the channels py::list, so
                        // iterate through both in lock step.
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
                            
                            std::vector<size_t> shape, strides;
                            shape.assign({ height, width });

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

void
PyPart::add_attribute(exr_context_t f, const std::string& name, py::object object)
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
        {
            throw std::runtime_error("invalid empty list is header: can't deduce attribute type");
        }
        else if (py::isinstance<py::float_>(list[0]))
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
    {
        const float o = py::cast<py::float_>(object);
        exr_attr_set_float(f, part_index, name.c_str(), o);
    }
    else if (py::isinstance<PyDouble>(object))
    {
        const PyDouble d = py::cast<PyDouble>(object);
        exr_attr_set_double(f, part_index, name.c_str(), d.d);
    }
    else if (py::isinstance<py::int_>(object))
    {
        const int o = py::cast<py::int_>(object);
        exr_attr_set_int(f, part_index, name.c_str(), o);
        return;
    }
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
        auto shape = v->pixels.shape();
        
        exr_attr_preview_t o;
        o.width = shape[1];
        o.height = shape[0];
        o.alloc_size = o.width * o.height * 4;
        py::buffer_info buf = v->pixels.request();
        o.rgba = static_cast<uint8_t*>(buf.ptr);
        exr_attr_set_preview(f, part_index, name.c_str(), &o);
    }
    else if (auto o = py_cast<exr_attr_rational_t,Rational>(object))
        exr_attr_set_rational(f, part_index, name.c_str(), o);
    else if (py::isinstance<py::str>(object))
    {
        const std::string& s = py::cast<py::str>(object);
        exr_attr_set_string(f, part_index, name.c_str(), s.c_str());
    }
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
    else
        throw std::runtime_error("unknown attribute type");
}

void
PyPart::add_attributes(exr_context_t f)
{
    //
    // Initialize the part_index. If there's a "type" attribute in the
    // header, use it over the value provided in the constructor.
    //
    
    if (header.contains("type"))
        type = py::cast<exr_storage_t>(header["type"]);
        
    exr_result_t result = exr_add_part(f, name.c_str(), type, &part_index);
    if (result != EXR_ERR_SUCCESS) 
        throw std::runtime_error("error writing part");

    //
    // Extract the necessary information from the required header attributes
    //
    // If there's a "compression" attribute in the header, use it over the
    // value provided in the constructor.
    //
        
    if (header.contains("compression"))
        compression = py::cast<exr_compression_t>(header["compression"]);
    
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
    // Add the attributes
    //
        
    for (auto a = header.begin(); a != header.end(); ++a)
    {
        auto v = *a;
        auto first = v.first;
        std::string name = py::cast<std::string>(py::str(first));
        py::object second = py::cast<py::object>(v.second);
        add_attribute(f, name, second);
    }
}

void
PyPart::add_channels(exr_context_t f)
{
    //
    // The channels must be written in alphabetic order, but the channels
    // py::list may not be sorted.  Assign each channel's "index" field
    // to the index in sorted order.
    //
        
    std::vector<int> sorted_indices(channels.size());
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);

    // Sort the indices based on the names of the PyChannel objects
    
    py::list& L = channels;
    std::sort(sorted_indices.begin(), sorted_indices.end(),
              [&L](int i, int j) {
                  PyChannel& channel_i = L[i].cast<PyChannel&>();
                  PyChannel& channel_j = L[j].cast<PyChannel&>();
                  return channel_i.name < channel_j.name;
              });

    // Assign the index field based on the sorted order
    for (size_t i = 0; i < channels.size(); ++i)
    {
        PyChannel& channel = channels[sorted_indices[i]].cast<PyChannel&>();
        channel.channel_index = i;
    }
        
    for (size_t i=0; i<sorted_indices.size(); i++)
    {
        auto c = sorted_indices[i];
        PyChannel& C = channels[c].cast<PyChannel&>();
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
      default:
          throw std::runtime_error("not implemented.");
          break;
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
            
        for (size_t c=0; c<channels.size(); c++)
        {            
            PyChannel& channel = channels[c].cast<PyChannel&>();
            channel.set_encoder_channel(encoder, y, width, scansperchunk);
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
}

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
header_equals(const py::dict& A, const py::dict& B)
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
        return false;
    
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
        if (!header_equals(header, other.header))
            return false;
        
        auto channels_v = py::cast<std::vector<PyChannel>>(channels);
        auto other_channels_v = py::cast<std::vector<PyChannel>>(other.channels);
        if (channels_v.size() != other_channels_v.size())
            return false;

        std::sort(channels_v.begin(), channels_v.end());
        std::sort(other_channels_v.begin(), other_channels_v.end());
        
        for (size_t c = 0; c<channels_v.size(); c++)
            if (!(channels_v[c] == other_channels_v[c]))
                return false;
        
        return true;
    }
    
    return false;
}

