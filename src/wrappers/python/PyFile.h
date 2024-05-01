//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include <pybind11/pybind11.h>
#include <openexr_attr.h>
#include <string>
#include <vector>

namespace py = pybind11;

//
// PyFile is the object that corresponds to an exr file, either for reading
// or writing, consisting of a simple list of parts.
//

class PyPart;

class PyFile 
{
public:
    PyFile(const std::string& filename);
    PyFile(const py::dict& header, const py::list& channels,
           exr_storage_t type, exr_compression_t compression);
    PyFile(const py::list& parts);


    py::list        parts() const { return py::cast(_parts); }
    const py::dict& header() const;
    const py::list& channels() const;

    void            write(const char* filename);
    
    bool operator==(const PyFile& other) const { return _parts == other._parts; }
    
    std::string          _filename;
    std::vector<PyPart>  _parts;
};

