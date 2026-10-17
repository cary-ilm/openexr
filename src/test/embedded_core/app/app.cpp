//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

// The application half of the OPENEXR_FORCE_EMBEDDED_CORE test: an
// executable linked against a standard build of OpenEXR, which loads a
// plugin linked against a custom build of OpenEXR (custom namespaces,
// custom library suffix, embedded core). Both libraries are then used
// in the same process, each reading an image written by the other.

#include "../roundtrip.h"

#include <dlfcn.h>
#include <link.h>

#include <set>

typedef void (*HeaderTypeNameFunc) (char*, size_t);
typedef int (*RoundtripFunc) (const char*, const char*);

static int
collectLibs (struct dl_phdr_info* info, size_t, void* data)
{
    std::string name = info->dlpi_name ? info->dlpi_name : "";
    std::string base = name.substr (name.find_last_of ('/') + 1);
    if (base.rfind ("libOpenEXR", 0) == 0)
        static_cast<std::set<std::string>*> (data)->insert (base);
    return 0;
}

int
main (int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " <plugin> <workdir>" << std::endl;
        return 1;
    }

    std::string a = std::string (argv[2]) + "/app.exr";
    std::string b = std::string (argv[2]) + "/plugin.exr";

    try
    {
        writeImage (a.c_str (), 1);
    }
    catch (const std::exception& e)
    {
        std::cerr << "app: " << e.what () << std::endl;
        return 1;
    }

    // RTLD_LOCAL, as an application typically loads a plugin.
    void* plugin = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!plugin)
    {
        std::cerr << "dlopen failed: " << dlerror () << std::endl;
        return 1;
    }

    HeaderTypeNameFunc headerTypeNameFunc = reinterpret_cast<HeaderTypeNameFunc> (
        dlsym (plugin, "plugin_header_type_name"));
    RoundtripFunc roundtrip =
        reinterpret_cast<RoundtripFunc> (dlsym (plugin, "plugin_roundtrip"));
    if (!headerTypeNameFunc || !roundtrip)
    {
        std::cerr << "dlsym failed: " << dlerror () << std::endl;
        return 1;
    }

    // The two modules must see distinct Imf::Header types, i.e. distinct
    // internal namespaces.
    char pluginType[256];
    headerTypeNameFunc (pluginType, sizeof (pluginType));
    std::cout << "app    Imf::Header: " << headerTypeName () << std::endl;
    std::cout << "plugin Imf::Header: " << pluginType << std::endl;
    if (headerTypeName () == pluginType)
    {
        std::cerr << "error: the app and plugin share an OpenEXR namespace"
                  << std::endl;
        return 1;
    }

    // Each side reads the other's image.
    if (roundtrip (a.c_str (), b.c_str ()) != 0)
    {
        std::cerr << "error: plugin round trip failed" << std::endl;
        return 1;
    }

    try
    {
        if (!checkImage (b.c_str (), 2)) return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "app: " << e.what () << std::endl;
        return 1;
    }

    // Both OpenEXR libraries must be loaded, and no OpenEXRCore shared
    // library other than the app's (the plugin's core is embedded).
    std::set<std::string> libs;
    dl_iterate_phdr (collectLibs, &libs);
    int nOpenEXR = 0, nCore = 0;
    for (const std::string& lib: libs)
    {
        std::cout << "loaded: " << lib << std::endl;
        if (lib.rfind ("libOpenEXRCore", 0) == 0)
            ++nCore;
        else if (lib.rfind ("libOpenEXRUtil", 0) != 0)
            ++nOpenEXR;
    }
    if (nOpenEXR != 2 || nCore != 1)
    {
        std::cerr << "error: expected 2 OpenEXR libraries and 1 OpenEXRCore "
                  << "library, found " << nOpenEXR << " and " << nCore
                  << std::endl;
        return 1;
    }

    dlclose (plugin);
    std::cout << "ok" << std::endl;
    return 0;
}
