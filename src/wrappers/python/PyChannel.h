//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/numpy.h>
#include <openexr_attr.h>
#include <string>

//
// PyChannel holds information for a channel of a PyPart: name, type, x/y
// sampling, and the array of pixel data.
//
  
#include "ImfForward.h"

class IMF_EXPORT_TYPE PyChannel 
{
public:

    PyChannel()
        : xSampling(0), ySampling(0) {}
    PyChannel(const char* n, exr_pixel_type_t t, int x, int y)
        : name(n), xSampling(x), ySampling(y) {}
    PyChannel(const char* n, const py::array& p)
        : name(n), xSampling(1), ySampling(1), pixels(p) {}
    PyChannel(const char* n, const py::array& p, int x, int y)
        : name(n), xSampling(x), ySampling(y), pixels(p) {}
    
    bool operator==(const PyChannel& other) const;
    bool operator<(const PyChannel& other) const { return name < other.name; }

    exr_pixel_type_t      pixelType() const;

    std::string           name;
    int                   xSampling;
    int                   ySampling;
    py::array             pixels;

    size_t                channel_index;
    
    void set_encoder_channel(exr_encode_pipeline_t& encoder, size_t y, size_t width, size_t scansperchunk) const;
};
    

inline std::ostream&
operator<< (std::ostream& s, const PyChannel& C)
{
    return s << "Channel(\"" << C.name 
             << "\", xSampling=" << C.xSampling 
             << ", ySampling=" << C.ySampling
             << ")";
}

