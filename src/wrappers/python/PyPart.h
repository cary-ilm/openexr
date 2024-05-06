//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

//
// PyPart holds the information for a part of an exr file: name, type,
// dimension, compression, the list of attributes (e.g. "header") and the
// list of channels.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/eval.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>

#include <string>
#include <ostream>
#include "openexr.h"

#include "ImfForward.h"

class IMF_EXPORT_TYPE PyPart
{
  public:
    PyPart()
        : type(EXR_STORAGE_LAST_TYPE), width(0), height(0),
        compression (EXR_COMPRESSION_LAST_TYPE), part_index(0) {}
    PyPart(const char* name, const py::dict& a, const py::dict& channels,
           exr_storage_t type, exr_compression_t c);
    PyPart(exr_context_t f, int part_index);
    
    bool operator==(const PyPart& other) const;

    std::string           name;
    exr_storage_t         type;
    uint64_t              width;
    uint64_t              height;
    exr_compression_t     compression;

    py::dict              header;
    py::dict              channels;

    int                   part_index;
    
    void read_scanline_part (exr_context_t f);
    void read_tiled_part (exr_context_t f);

    py::object get_attribute_object(exr_context_t f, int32_t attr_index, std::string& name);

    void add_attributes(exr_context_t f); 
    void add_attribute(exr_context_t f, const std::string& name, py::object object);
    void add_channels(exr_context_t f);

    void write(exr_context_t f);
    void write_scanlines(exr_context_t f);
    void write_tiles(exr_context_t f);
};

inline std::ostream&
operator<< (std::ostream& s, const PyPart& P)
{
    return s << "Part(\"" << P.name 
             << "\", type=" << py::cast(P.type) 
             << ", width=" << P.width
             << ", height=" << P.height
             << ", compression=" << P.compression
             << ")";
}

