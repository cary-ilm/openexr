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

#include <half.h>

#include <ostream>

template <class T>
bool
array_equals(const py::buffer_info& a, const py::buffer_info& b, const std::string& name)
{
    const T* apixels = static_cast<const T*>(a.ptr);
    const T* bpixels = static_cast<const T*>(b.ptr);
    for (ssize_t i = 0; i < a.size; i++)
        if (!(apixels[i] == bpixels[i]))
            return false;
    return true;
}

bool
PyChannel::operator==(const PyChannel& other) const
{
    if (name == other.name && 
        xSampling == other.xSampling && 
        ySampling == other.ySampling &&
        pixels.ndim() == other.pixels.ndim() && 
        pixels.size() == other.pixels.size())
    {
        py::buffer_info buf = pixels.request();
        py::buffer_info obuf = other.pixels.request();

        if (py::isinstance<py::array_t<uint32_t>>(pixels) && py::isinstance<py::array_t<uint32_t>>(other.pixels))
            return array_equals<uint32_t>(buf, obuf, name);
        if (py::isinstance<py::array_t<half>>(pixels) && py::isinstance<py::array_t<half>>(other.pixels))
            return array_equals<half>(buf, obuf, name);
        if (py::isinstance<py::array_t<float>>(pixels) && py::isinstance<py::array_t<float>>(other.pixels))
            return array_equals<float>(buf, obuf, name);
    }

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

    switch (pixelType())
    {
      case EXR_PIXEL_UINT:
          {
              const uint32_t* pixels = static_cast<const uint32_t*>(buf.ptr);
              encoder.channels[channel_index].encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              encoder.channels[channel_index].user_pixel_stride = sizeof(uint32_t);
          }
          break;
      case EXR_PIXEL_HALF:
          {
              const half* pixels = static_cast<const half*>(buf.ptr);
              encoder.channels[channel_index].encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              encoder.channels[channel_index].user_pixel_stride = sizeof(half);
          }
          break;
      case EXR_PIXEL_FLOAT:
          {
              const float* pixels = static_cast<const float*>(buf.ptr);
              encoder.channels[channel_index].encode_from_ptr = reinterpret_cast<const uint8_t*>(&pixels[offset]);
              encoder.channels[channel_index].user_pixel_stride = sizeof(float);
          }
          break;
      case EXR_PIXEL_LAST_TYPE:
      default:
          throw std::runtime_error("invalid pixel type");
          break;
    }

    encoder.channels[channel_index].user_line_stride  = encoder.channels[channel_index].user_pixel_stride * width;
    encoder.channels[channel_index].height            = scansperchunk; // chunk height
    encoder.channels[channel_index].width             = width;
}

