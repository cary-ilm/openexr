# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

openexr_define_library(IlmThread
  PRIV_EXPORT ILMTHREAD_EXPORTS
  CURDIR ${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES
    IlmThread.cpp
    IlmThreadPool.cpp
    IlmThreadPosix.cpp
    IlmThreadSemaphore.cpp
    IlmThreadSemaphorePosixCompat.cpp
    IlmThreadSemaphorePosix.cpp
    IlmThreadSemaphoreOSX.cpp
    IlmThreadSemaphoreWin32.cpp
    IlmThreadWin32.cpp
  HEADERS
    IlmThreadPool.h
    IlmThread.h
    IlmThreadSemaphore.h
    IlmThreadMutex.h
    IlmThreadNamespace.h
    IlmThreadExport.h
    IlmThreadForward.h
  DEPENDENCIES
    OpenEXR::Iex
    Threads::Threads
  )