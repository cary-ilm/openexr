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

py::object get_attribute(exr_context_t f, int32_t p, int32_t a, std::string& name);

static void
core_error_handler_cb (exr_const_context_t f, int code, const char* msg)
{
    const char* fn = "";
#if XXX
    if (EXR_ERR_SUCCESS != exr_get_file_name (f, &fn))
        fn = "<error>";
#endif
    std::stringstream s;
    s << "error " << fn << " " << exr_get_error_code_as_string (code) << " " << msg;
    throw std::runtime_error(s.str());
}

//
// Create a PyFile out of a list of parts (i.e. a multi-part file)
//

PyFile::PyFile(const py::list& p)
    : parts(p)
{
}

//
// Create a PyFile out of a single part: header, channels,
// type, and compression (i.e. a single-part file)
//

PyFile::PyFile(const py::dict& header, const py::list& channels,
               exr_storage_t type, exr_compression_t compression)
{
    parts.append(py::cast<PyPart>(PyPart("Part0", header, channels, type, compression)));
}

//
// Read a PyFile from the given filename
//


PyFile::PyFile(const std::string& filename)
    : _filename (filename)
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
    {
        PyPart P;
        P.part_index = part_index;
        
        //
        // Read the attributes into the header
        //
        
        py::dict& h = P.header;

        int32_t attrcount;
        rv = exr_get_attribute_count(f, part_index, &attrcount);
        if (rv != EXR_ERR_SUCCESS)
            throw std::runtime_error ("read error");

        for (int32_t a = 0; a < attrcount; ++a)
        {
            std::string name;
            py::object attr = get_attribute(f, part_index, a, name);
            h[name.c_str()] = attr;
            if (name == "name")
                P.name = py::str(attr);
        }

        //
        // Read the type (i.e. scanline, tiled, deep, etc)
        //
        
        exr_storage_t store;
        rv = exr_get_storage (f, part_index, &store);
        if (rv != EXR_ERR_SUCCESS)
            return;

        P.type = store;
            
        //
        // Read the compression type
        //
        
        exr_compression_t compression;
        rv = exr_get_compression(f, part_index, &compression);
        if (rv != EXR_ERR_SUCCESS)
            return;
        
        P.compression = compression;

        //
        // Read the part
        //
        
        if (store == EXR_STORAGE_SCANLINE || store == EXR_STORAGE_DEEP_SCANLINE)
            P.read_scanline_part (f);
        else if (store == EXR_STORAGE_TILED || store == EXR_STORAGE_DEEP_TILED)
            P.read_tiled_part (f);

        parts.append(P);
    }
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

const py::dict&
PyFile::header() const
{
    if (parts.size() == 0)
        throw std::runtime_error("File has no parts");
    const PyPart& P = py::cast<const PyPart&>(parts[0]);
    return P.header;
}

const py::list&
PyFile::channels() const
{
    if (parts.size() == 0)
        throw std::runtime_error("File has no parts");
    const PyPart& P = py::cast<const PyPart&>(parts[0]);
    return P.channels;
}

//
// Write the PyFile to the given filename
//

void
PyFile::write(const char* filename)
{
    //
    // Open the file for writing
    //

    exr_context_t f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    cinit.error_handler_fn = &core_error_handler_cb;

    exr_result_t result = exr_start_write(&f, filename, EXR_WRITE_FILE_DIRECTLY, &cinit);
    if (result != EXR_ERR_SUCCESS)
    {
        std::stringstream s;
        s << "can't open " << filename << " for write";
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

    _filename = filename;
}

//
// Read an attribute from the file at the given index and create a
// corresponding python object.
//

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
              PyChromaticities c(attr->chromaticities->red_x,
                                 attr->chromaticities->red_y,
                                 attr->chromaticities->green_x,
                                 attr->chromaticities->green_y,
                                 attr->chromaticities->blue_x,
                                 attr->chromaticities->blue_y,
                                 attr->chromaticities->white_x,
                                 attr->chromaticities->white_y);
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
          {
              auto pixels = reinterpret_cast<const PreviewRgba*>(attr->preview->rgba);
              return py::cast(PyPreviewImage(attr->preview->width,
                                           attr->preview->height,
                                           pixels));
          }
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
      default:
          throw std::invalid_argument("unknown attribute type");
          break;
    }
    return py::none();
}

