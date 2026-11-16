//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

//
// Aggregate compile-check translation unit for the website documentation
// examples.
//
// The files in this directory are standalone documentation snippets (one
// example per file).  They are compiled only to verify that the examples
// have no compiler errors; the resulting object code is never linked or
// used.  Because they are written to read as independent examples, several
// of them reuse common helper names (GZ, readChannels, readComments,
// makePreviewImage) and one defines a documentation stand-in "IStream"
// class that collides with Imf::IStream.  To compile them all in a single
// translation unit (so this check builds as fast as a unity build without
// disabling unity for the rest of the project), the few colliding symbols
// are renamed with the preprocessor at the point of inclusion.  The
// example sources themselves are left untouched.
//
// This file must be kept in sync with the SOURCES list in CMakeLists.txt.
//

// Define the real Imf::IStream (and Iex) before any macro below renames
// the documentation stand-in "IStream" class.
#include <ImfIO.h>
#include <Iex.h>

//
// C_IStream example member definitions.  C_IStream derives from the real
// Imf::IStream (qualified in its header), so it needs no renaming.
//
#include "C_IStream_read.cpp"
#include "C_IStream_seekg.cpp"
#include "C_IStream_tellg.cpp"
#include "C_IStream_clear.cpp"

//
// MemoryMappedIStream example member definitions (POSIX only).  This
// example's local "IStream.h" defines a documentation stand-in
// "class IStream" that is ambiguous with Imf::IStream once merged into a
// shared TU.  Rename it to ExampleIStream for the duration of these
// includes; Imf::IStream (included above) is unaffected.
//
#ifndef _WIN32
#    define IStream ExampleIStream
#    include "MemoryMappedIStream_constructor.cpp"
#    include "MemoryMappedIStream_destructor.cpp"
#    include "MemoryMappedIStream_isMemoryMapped.cpp"
#    include "MemoryMappedIStream_read.cpp"
#    include "MemoryMappedIStream_readMemoryMapped.cpp"
#    undef IStream
#endif

//
// Standalone examples.  A handful reuse common helper names; rename the
// duplicates per-include so each example's source stays readable.
//
#include "compression.cpp"
#include "envmap.cpp"
#include "multithreading.cpp"
#include "mergeOverlappingSamples.cpp"
#include "readDeepScanLineFile.cpp"
#include "readDeepTiledFile.cpp"
#include "readGZ1.cpp"

#define GZ GZ_readGZ2
#include "readGZ2.cpp"
#undef GZ

#include "readHeader.cpp"

#define readComments readComments_1
#include "readComments1.cpp"
#undef readComments

#define readComments readComments_2
#include "readComments2.cpp"
#undef readComments

#include "readRgba1.cpp"
#include "readRgba2.cpp"
#include "readRgbaFILE.cpp"

#define GZ GZ_readTiled1
#include "readTiled1.cpp"
#undef GZ

#include "readTiledRgba1.cpp"

#define readChannels readChannels_1
#include "readChannels1.cpp"
#undef readChannels

#define readChannels readChannels_2
#include "readChannels2.cpp"
#undef readChannels

#include "readLayers.cpp"
#include "splitVolumeSample.cpp"
#include "writeDeepScanLineFile.cpp"
#include "writeDeepTiledFile.cpp"
#include "writeGZ1.cpp"
#include "writeGZ2.cpp"
#include "writeRgba1.cpp"
#include "writeRgba2.cpp"
#include "writeRgba3.cpp"
#include "writeRgbaMT.cpp"
#include "writeTiledRgbaONE1.cpp"

#define makePreviewImage makePreviewImage_writeRgbaWithPreview1
#include "writeRgbaWithPreview1.cpp"
#undef makePreviewImage

#define GZ GZ_writeTiled1
#include "writeTiled1.cpp"
#undef GZ

#include "tileDescription.cpp"
#include "validExrFile.cpp"

#define makePreviewImage makePreviewImage_previewImageExamples
#include "previewImageExamples.cpp"
#undef makePreviewImage
