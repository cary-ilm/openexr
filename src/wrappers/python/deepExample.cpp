#include <ImfArray.h>

#include <ImfDeepScanLineInputFile.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfHeader.h>
#include <ImfDeepFrameBuffer.h>

#include <cfloat>
#include <ImathBox.h>
#include <half.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

using namespace std;

float
pw (float x, int y)
{
    float p = 1;

    while (y)
    {
        if (y & 1) p *= x;

        x *= x;
        y >>= 1;
    }

    return p;
}

void
zsp (
    Array2D<half>&  gpx,
    Array2D<float>& zpx,
    int             w,
    int             h,
    float           xc,
    float           yc,
    float           zc,
    float           rc,
    float           gn)
{
    int x1 = int (max ((float) floor (xc - rc), 0.0f));
    int x2 = int (min ((float) ceil (xc + rc), w - 1.0f));
    int y1 = int (max ((float) floor (yc - rc), 0.0f));
    int y2 = int (min ((float) ceil (yc + rc), h - 1.0f));

    for (int x = x1; x <= x2; ++x)
    {
        for (int y = y1; y <= y2; ++y)
        {
            float xl = (x - xc) / rc;
            float yl = (y - yc) / rc;
            float r  = sqrt (xl * xl + yl * yl);

            if (r >= 1) continue;

            float zl = sqrt (1 - r * r);
            float zp = zc - rc * zl;

            if (zp >= zpx[y][x]) continue;

            float dl = xl * 0.42426 - yl * 0.56568 + zl * 0.70710;

            if (dl < 0) dl *= -0.1;

            float hl = pw (dl, 50) * 4;
            float dg = (dl + hl) * gn;

            gpx[y][x] = dg;
            zpx[y][x] = zp;
        }
    }
}

void
drawImage2 (Array2D<half>& gpx, Array2D<float>& zpx, Array2D<uint32_t>& bpx, int w, int h)
{
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            gpx[y][x] = 0;
            zpx[y][x] = FLT_MAX;
            bpx[y][x] = y * w + x;
        }
    }

    int n = 2000;

    for (int i = 0; i < n; ++i)
    {
        float t  = (i * 2.0 * M_PI) / n;
        float xp = sin (t * 4.0) + 0.2 * sin (t * 15.0);
        float yp = cos (t * 3.0) + 0.2 * cos (t * 15.0);
        float zp = sin (t * 5.0);
        float rd = 0.7 + 0.3 * sin (t * 15.0);
        float gn = 0.5 - 0.5 * zp + 0.2;

        zsp (
            gpx,
            zpx,
            w,
            h,
            xp * w / 3 + w / 2,
            yp * h / 3 + h / 2,
            zp * w + 3 * w,
            w * rd * 0.05,
            2.5 * gn * gn);
    }
}


void
readDeepScanlineFile (const char             filename[],
                      Box2i&                 displayWindow,
                      Box2i&                 dataWindow,
                      Array2D<float*>&       dataZ,
                      Array2D<half*>&        dataA,
                      Array2D<uint32_t*>&    dataB,
                      Array2D<unsigned int>& sampleCount)
{
    //
    // Read a deep image using class DeepScanLineInputFile.  Try to read one
    // channel, A, of type HALF, and one channel, Z,
    // of type FLOAT.  Store the A, and Z pixels in two
    // separate memory buffers.
    //
    //    - open the file
    //    - allocate memory for the pixels
    //    - describe the layout of the A, and Z pixel buffers
    //    - read the sample counts from the file
    //    - allocate the memory requred to store the samples
    //    - read the pixels from the file
    //

    DeepScanLineInputFile file (filename);

    const Header& header = file.header ();
    dataWindow           = header.dataWindow ();
    displayWindow        = header.displayWindow ();

    auto channels = header.channels();
    for (auto c = channels.begin(); c != channels.end(); c++)
        std::cout << "channel " << c.name() << " type=" << c.channel().type << std::endl;
        
    int width  = dataWindow.max.x - dataWindow.min.x + 1;
    int height = dataWindow.max.y - dataWindow.min.y + 1;

    sampleCount.resizeErase (height, width);

    dataZ.resizeErase (height, width);
    dataA.resizeErase (height, width);
    dataB.resizeErase (height, width);

    DeepFrameBuffer frameBuffer;

    frameBuffer.insertSampleCountSlice (Slice (
        UINT,
        (char*) (&sampleCount[0][0] - dataWindow.min.x -
                 dataWindow.min.y * width),
        sizeof (unsigned int) * 1,       // xStride
        sizeof (unsigned int) * width)); // yStride

    frameBuffer.insert (
        "dataZ",
        DeepSlice (
            FLOAT,
            (char*) (&dataZ[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),

            sizeof (float*) * 1,     // xStride for pointer array
            sizeof (float*) * width, // yStride for pointer array
            sizeof (float) * 1));    // stride for Z data sample

    frameBuffer.insert (
        "dataA",
        DeepSlice (
            HALF,
            (char*) (&dataA[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),
            sizeof (half*) * 1,     // xStride for pointer array
            sizeof (half*) * width, // yStride for pointer array
            sizeof (half) * 1));    // stride for O data sample

    frameBuffer.insert (
        "dataB",
        DeepSlice (
            UINT,
            (char*) (&dataB[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),
            sizeof (uint32_t*) * 1,     // xStride for pointer array
            sizeof (uint32_t*) * width, // yStride for pointer array
            sizeof (uint32_t) * 1));    // stride for O data sample

    file.setFrameBuffer (frameBuffer);

    file.readPixelSampleCounts (dataWindow.min.y, dataWindow.max.y);

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {

            dataZ[i][j] = new float[sampleCount[i][j]];
            dataA[i][j] = new half[sampleCount[i][j]];
            dataB[i][j] = new uint32_t[sampleCount[i][j]];
        }
    }

    file.readPixels (dataWindow.min.y, dataWindow.max.y);

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            delete[] dataZ[i][j];
            delete[] dataA[i][j];
            delete[] dataB[i][j];
        }
    }
}

unsigned int
getPixelSampleCount (int i, int j)
{
    // Dummy code creating deep data from a flat image
    return 1;
}

Array2D<float>     testDataZ;
Array2D<half>      testDataA;
Array2D<uint32_t>  testDataB;

void
getPixelSampleData (int i, int j, Array2D<float*>& dataZ, Array2D<half*>& dataA, Array2D<uint32_t*>& dataB)
{
    // Dummy code creating deep data from a flat image
    dataZ[i][j][0] = testDataZ[i][j];
    dataA[i][j][0] = testDataA[i][j];
    dataB[i][j][0] = testDataB[i][j];
}

void
writeDeepScanlineFile (
    const char       filename[],
    Box2i            displayWindow,
    Box2i            dataWindow,
    Array2D<float*>& dataZ,
    Array2D<half*>& dataA,
    Array2D<uint32_t*>& dataB,
    Array2D<unsigned int>& sampleCount)

{
    //
    // Write a deep image with only a A (alpha) and a Z (depth) channel,
    // using class DeepScanLineOutputFile.
    //
    //    - create a file header
    //    - add A and Z channels to the header
    //    - open the file, and store the header in the file
    //    - describe the memory layout of the A and Z pixels
    //    - store the pixels in the file
    //

    int height = dataWindow.max.y - dataWindow.min.y + 1;
    int width  = dataWindow.max.x - dataWindow.min.x + 1;

    Header header (displayWindow, dataWindow);

    header.channels ().insert ("Z", Channel (FLOAT));
    header.channels ().insert ("A", Channel (HALF));
    header.channels ().insert ("B", Channel (UINT));
    header.setType (DEEPSCANLINE);
    header.compression () = ZIPS_COMPRESSION;

    DeepScanLineOutputFile file (filename, header);

    DeepFrameBuffer frameBuffer;

    frameBuffer.insertSampleCountSlice (Slice (
        UINT,
        (char*) (&sampleCount[0][0] - dataWindow.min.x -
                 dataWindow.min.y * width),
        sizeof (unsigned int) * 1, // xS

        sizeof (unsigned int) * width)); // yStride

    frameBuffer.insert (
        "Z",
        DeepSlice (
            FLOAT,
            (char*) (&dataZ[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),
            sizeof (float*) * 1, // xStride for pointer

            sizeof (float*) * width, // yStride for pointer array
            sizeof (float) * 1));    // stride for Z data sample

    frameBuffer.insert (
        "A",
        DeepSlice (
            HALF,
            (char*) (&dataA[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),
            sizeof (half*) * 1,     // xStride for pointer array
            sizeof (half*) * width, // yStride for pointer array
            sizeof (half) * 1));    // stride for A data sample

    frameBuffer.insert (
        "B",
        DeepSlice (
            UINT,
            (char*) (&dataB[0][0] - dataWindow.min.x -
                     dataWindow.min.y * width),
            sizeof (uint32_t*) * 1,     // xStride for pointer array
            sizeof (uint32_t*) * width, // yStride for pointer array
            sizeof (uint32_t) * 1));    // stride for A data sample

    file.setFrameBuffer (frameBuffer);

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            sampleCount[i][j] = getPixelSampleCount (i, j);
            dataZ[i][j]       = new float[sampleCount[i][j]];
            dataA[i][j]       = new half[sampleCount[i][j]];
            dataB[i][j]       = new uint32_t[sampleCount[i][j]];
            // Generate data for dataZ and dataA.
            getPixelSampleData (i, j, dataZ, dataA, dataB);
        }

        file.writePixels (1);
    }

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            delete[] dataZ[i][j];
            delete[] dataA[i][j];
            delete[] dataB[i][j];
        }
    }
}

void
writeDeepExample ()
{
    int w = 10;
    int h = 20;

    Box2i window;
    window.min.setValue (0, 0);
    window.max.setValue (w - 1, h - 1);

    Array2D<float*> dataZ;
    dataZ.resizeErase (h, w);

    Array2D<half*> dataA;
    dataA.resizeErase (h, w);

    Array2D<uint32_t*> dataB;
    dataB.resizeErase (h, w);

    Array2D<unsigned int> sampleCount;
    sampleCount.resizeErase (h, w);

    // Create an image to be used as a source for deep data
    testDataA.resizeErase (h, w);
    testDataZ.resizeErase (h, w);
    testDataB.resizeErase (h, w);
    drawImage2 (testDataA, testDataZ, testDataB, w, h);

    writeDeepScanlineFile ("test.deep.exr", window, window, dataZ, dataA, dataB, sampleCount);
}

void
readDeepExample ()
{
    int w = 800;
    int h = 600;

    Box2i window;
    window.min.setValue (0, 0);
    window.max.setValue (w - 1, h - 1);

    Array2D<float*> dataZ;
    dataZ.resizeErase (h, w);

    Array2D<half*> dataA;
    dataA.resizeErase (h, w);

    Array2D<uint32_t*> dataB;
    dataB.resizeErase (h, w);

    Array2D<unsigned int> sampleCount;
    sampleCount.resizeErase (h, w);

    readDeepScanlineFile ("test.deep.exr",
                          window, window, dataZ, dataA, dataB, sampleCount);
}
