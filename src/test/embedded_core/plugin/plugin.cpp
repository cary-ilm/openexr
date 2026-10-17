//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

// A plugin, as a DCC application would load it, linked against a
// custom build of OpenEXR configured with custom internal namespaces,
// a custom library suffix, and OPENEXR_FORCE_EMBEDDED_CORE=ON.

#include "../roundtrip.h"

#include <cstring>

#if defined(_WIN32)
#    define PLUGIN_EXPORT extern "C" __declspec (dllexport)
#else
#    define PLUGIN_EXPORT extern "C" __attribute__ ((visibility ("default")))
#endif

// Copy the mangled name of the plugin's Imf::Header into buf.
PLUGIN_EXPORT void
plugin_header_type_name (char* buf, size_t size)
{
    std::strncpy (buf, headerTypeName ().c_str (), size - 1);
    buf[size - 1] = '\0';
}

// Read and validate infile (written by the app with seed 1), then
// write outfile with seed 2. Returns 0 on success.
PLUGIN_EXPORT int
plugin_roundtrip (const char* infile, const char* outfile)
{
    try
    {
        if (!checkImage (infile, 1)) return 1;
        writeImage (outfile, 2);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "plugin: " << e.what () << std::endl;
        return 1;
    }
}
