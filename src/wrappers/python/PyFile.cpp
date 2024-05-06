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

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathBox.h>
#include <ImathMath.h>

//using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
    const char* filename = "";
    exr_get_file_name (f, &filename);
    std::stringstream s;
    s << "error \"" << filename << "\": " << msg;
    throw std::runtime_error(s.str());
}

//
// Create a PyFile out of a list of parts (i.e. a multi-part file)
//

PyFile::PyFile(const py::list& p)
    : parts(p)
{
    for (auto v : parts)
        if (!py::isinstance<PyPart>(*v))
            throw std::invalid_argument("must be a list of OpenEXR.Part() objects");
}

//
// Create a PyFile out of a single part: header, channels,
// type, and compression (i.e. a single-part file)
//

PyFile::PyFile(const py::dict& header, const py::dict& channels,
               exr_storage_t type, exr_compression_t compression)
{
    parts.append(py::cast<PyPart>(PyPart("Part0", header, channels, type, compression)));
}

//
// Read a PyFile from the given filename
//


PyFile::PyFile(const std::string& filename)
    : filename (filename)
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

    parts = py::list();
    
    for (int part_index = 0; part_index < numparts; ++part_index)
        parts.append(PyPart(f, part_index));
}

bool
PyFile::operator==(const PyFile& other) const
{
    if (parts.size() != other.parts.size())
        return false;
    
    for (size_t i = 0; i<parts.size(); i++)
        if (!(py::cast<const PyPart&>(parts[i]) == py::cast<const PyPart&>(other.parts[i])))
            return false;
    return true;
}       

void
validate_part_index(int part_index, size_t num_parts)
{
    if (0 < part_index)
    {
        std::stringstream s;
        s << "Invalid part index '" << part_index << "'";
        throw std::invalid_argument(s.str());
    }
    
    if (static_cast<size_t>(part_index) >= num_parts)
    {
        std::stringstream s;
        s << "Invalid part index '" << part_index
          << "': file has " << num_parts
          << " part";
        if (num_parts != 1)
            s << "s";
        s << ".";
        throw std::invalid_argument(s.str());
    }
}
    
py::dict&
PyFile::header(int part_index)
{
    validate_part_index(part_index, parts.size());
    return parts[part_index].cast<PyPart&>().header;
}

py::dict&
PyFile::channels(int part_index)
{
    validate_part_index(part_index, parts.size());
    return parts[part_index].cast<PyPart&>().channels;
}

//
// Write the PyFile to the given filename
//

void
PyFile::write(const char* outfilename)
{
    //
    // Open the file for writing
    //

    exr_context_t f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    exr_result_t result = exr_start_write(&f, outfilename, EXR_WRITE_FILE_DIRECTLY, &cinit);
    if (result != EXR_ERR_SUCCESS)
    {
        std::stringstream s;
        s << "can't open " << outfilename << " for write";
        throw std::runtime_error(s.str());
    }

    exr_set_longname_support(f, 1);

    //
    // Set up the parts
    //
    
    for (size_t p=0; p<parts.size(); p++)
    {
        PyPart& P = py::cast<PyPart&>(parts[p]);
        
        P.add_attributes(f);
        P.add_channels(f);
        
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

    //
    // Write the parts
    //
    
    for (size_t p=0; p<parts.size(); p++)
    {
        auto P = py::cast<const PyPart&>(parts[p]);
        P.write(f);
    }

    result = exr_finish(&f);

    filename = outfilename;
}

