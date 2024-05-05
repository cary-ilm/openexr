//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/pybind11.h>
#include <openexr_attr.h>
#include <string>
#include <vector>

#include "ImfForward.h"

namespace py = pybind11;

//
// PyFile is the object that corresponds to an exr file, either for reading
// or writing, consisting of a simple list of parts.
//

class PyPart;

class IMF_EXPORT_TYPE PyFile 
{
public:
    PyFile() {}
    PyFile(const std::string& filename);
    PyFile(const py::dict& header, const py::dict& channels,
           exr_storage_t type, exr_compression_t compression);
    PyFile(const py::list& parts);

    py::dict& header(int part_index = 0);
    py::dict& channels(int part_index = 0);

    void      write(const char* filename);
    
    bool operator==(const PyFile& other) const;
    
    std::string  filename;
    py::list     parts;
};

