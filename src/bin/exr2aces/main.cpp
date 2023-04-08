//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

//-----------------------------------------------------------------------------
//
//	exr2aces -- a program that converts an
//	OpenEXR file to an ACES image file.
//
//-----------------------------------------------------------------------------

#include <ImfAcesFile.h>
#include <ImfArray.h>
#include <ImfRgbaFile.h>
#include <ImfMisc.h>
#include <OpenEXRConfig.h>
#include <exception>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <OpenEXRConfig.h>
using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;
using namespace std;

namespace
{

void
usageMessage (ostream& stream, bool verbose = false)
{
    stream << "Usage: exr2aces [options] infile outfile" << endl;

    if (verbose)
    {
        stream << "\n"
            "Reads an OpenEXR file from infile and saves the contents\n"
            "in ACES image file outfile.\n"
            "\n"
            "The ACES image file format is a subset of the OpenEXR file\n"
            "format.  ACES image files are restricted as follows:\n"
            "\n"
            "* Images are stored as scanlines; tiles are not allowed.\n"
            "\n"
            "* Images contain three color channels, either\n"
            "      R, G, B (red, green, blue) or\n"
            "      Y, RY, BY (luminance, sub-sampled chroma)\n"
            "\n"
            "* Images may optionally contain an alpha channel.\n"
            "\n"
            "* Only three compression types are allowed:\n"
            "      NO_COMPRESSION (file is not compressed)\n"
            "      PIZ_COMPRESSION (lossless)\n"
            "      B44A_COMPRESSION (lossy)\n"
            "* The \"chromaticities\" header attribute must specify\n"
            "  the ACES RGB primaries and white point.\n"
            "\n"
            "Options:\n"
            "\n"
            "-v, --verbose     verbose mode\n"
            "\n"
            "-h, --help        print this message\n"
            "\n"
            "--version         print version information\n"
            "\n"
            "Report bugs at https://github.com/AcademySoftwareFoundation/openexr/issues or email security@openexr.com"
            "";

        stream << endl;
    }
}

void
exr2aces (const char inFileName[], const char outFileName[], bool verbose)
{
    Array2D<Rgba> p;
    Header        h;
    RgbaChannels  ch;
    Box2i         dw;
    int           width;
    int           height;

    {
        if (verbose) cout << "Reading file " << inFileName << endl;

        AcesInputFile in (inFileName);

        h  = in.header ();
        ch = in.channels ();
        dw = h.dataWindow ();

        width  = dw.max.x - dw.min.x + 1;
        height = dw.max.y - dw.min.y + 1;
        p.resizeErase (height, width);

        in.setFrameBuffer (ComputeBasePointer (&p[0][0], dw), 1, width);
        in.readPixels (dw.min.y, dw.max.y);
    }

    switch (h.compression ())
    {
        case NO_COMPRESSION: break;

        case B44_COMPRESSION:
        case B44A_COMPRESSION: h.compression () = B44A_COMPRESSION; break;

        default: h.compression () = PIZ_COMPRESSION;
    }

    {
        if (verbose) cout << "Writing file " << outFileName << endl;

        AcesOutputFile out (outFileName, h, ch);

        out.setFrameBuffer (ComputeBasePointer (&p[0][0], dw), 1, width);
        out.writePixels (height);
    }
}

} // namespace

int
main (int argc, char** argv)
{
    const char* inFile  = 0;
    const char* outFile = 0;
    bool        verbose = false;

    //
    // Parse the command line.
    //

    if (argc < 2)
    {
        usageMessage (cerr, false);
        exit(-1);
    }

    int i = 1;

    while (i < argc)
    {
        if (!strcmp (argv[i], "-v"))
        {
            //
            // Verbose mode
            //

            verbose = true;
            i += 1;
        }
        else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help"))
        {
            //
            // Print help message
            //

            usageMessage (cout, true);
            exit(0);
        }
        else if (!strcmp (argv[i], "--version"))
        {
            const char* libraryVersion = getLibraryVersion();
            
            cout << "exr2aces (OpenEXR) " << OPENEXR_VERSION_STRING;
            if (strcmp(libraryVersion, OPENEXR_VERSION_STRING))
                cout << "(OpenEXR version " << libraryVersion << ")";
            cout << " https://openexr.com" << endl;
            cout << "Copyright (c) Contributors to the OpenEXR Project" << endl;
            cout << "License BSD-3-Clause" << endl;
            exit(0);
        }
        else
        {
            //
            // Image file name
            //

            if (inFile == 0)
                inFile = argv[i];
            else
                outFile = argv[i];

            i += 1;
        }
    }

    if (inFile == 0 || outFile == 0)
    {
        usageMessage (cerr, false);
        exit(-1);
    }

    //
    // Load inFile, and save a tiled version in outFile.
    //

    int exitStatus = 0;

    try
    {
        exr2aces (inFile, outFile, verbose);
    }
    catch (const exception& e)
    {
        cerr << e.what () << endl;
        exitStatus = 1;
    }

    return exitStatus;
}
