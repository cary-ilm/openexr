# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

openexr_define_library(OpenEXRUtil
  PRIV_EXPORT OPENEXRUTIL_EXPORTS
  CURDIR ${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES
    ImfCheckFile.cpp
    ImfDeepImage.cpp
    ImfDeepImageChannel.cpp
    ImfDeepImageIO.cpp
    ImfDeepImageLevel.cpp
    ImfFlatImage.cpp
    ImfFlatImageChannel.cpp
    ImfFlatImageIO.cpp
    ImfFlatImageLevel.cpp
    ImfImage.cpp
    ImfImageChannel.cpp
    ImfImageDataWindow.cpp
    ImfImageIO.cpp
    ImfImageLevel.cpp
    ImfSampleCountChannel.cpp
  HEADERS
    ImfCheckFile.h
    ImfDeepImage.h
    ImfDeepImageChannel.h
    ImfDeepImageIO.h
    ImfDeepImageLevel.h
    ImfFlatImage.h
    ImfFlatImageChannel.h
    ImfFlatImageIO.h
    ImfFlatImageLevel.h
    ImfImage.h
    ImfImageChannel.h
    ImfImageChannelRenaming.h
    ImfImageDataWindow.h
    ImfImageIO.h
    ImfImageLevel.h
    ImfSampleCountChannel.h
    ImfUtilExport.h
  DEPENDENCIES
    OpenEXR::OpenEXR
    OpenEXR::OpenEXRCore
)

# ImfDeepImageChannel.cpp and ImfFlatImageChannel.cpp each define
# COMPILING_IMF_DEEP_IMAGE_CHANNEL/COMPILING_IMF_FLAT_IMAGE_CHANNEL before
# including their own header, which suppresses that header's "extern
# template class" declaration so the file's explicit specializations
# (e.g. TypedDeepImageChannel<half>::pixelType()) and explicit
# instantiation definitions take effect. If any other source in the same
# translation unit includes the same header first (without the macro), the
# extern-template interpretation is locked in and the later specialization
# becomes ill-formed ("explicit specialization after instantiation") - the
# same hazard guarded for the Imf*Attribute.cpp files in OpenEXR's own
# CMakeLists.txt.
#
# The two channel headers have disjoint include trees (deep never includes
# flat and vice versa), and these two .cpp files are the only in-library
# sources that define the COMPILING_ macros. They can therefore still be
# combined into a single unity translation unit as long as that unit is
# kept separate from the other sources that pull in the channel headers
# (ImfDeepImage*.cpp, ImfFlatImage*.cpp, ImfSampleCountChannel.cpp). Use
# grouped unity mode to place them in their own group while keeping the
# rest of the library unity-batched together.
set_target_properties(OpenEXRUtil PROPERTIES UNITY_BUILD_MODE GROUP)
set_property(SOURCE
    ImfDeepImageChannel.cpp
    ImfFlatImageChannel.cpp
  PROPERTY UNITY_GROUP "channels")
set_property(SOURCE
    ImfCheckFile.cpp
    ImfDeepImage.cpp
    ImfDeepImageIO.cpp
    ImfDeepImageLevel.cpp
    ImfFlatImage.cpp
    ImfFlatImageIO.cpp
    ImfFlatImageLevel.cpp
    ImfImage.cpp
    ImfImageChannel.cpp
    ImfImageDataWindow.cpp
    ImfImageIO.cpp
    ImfImageLevel.cpp
    ImfSampleCountChannel.cpp
  PROPERTY UNITY_GROUP "util")
