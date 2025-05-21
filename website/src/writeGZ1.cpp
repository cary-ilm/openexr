// [begin writeGZ1]
void
writeGZ1 (
    const char   fileName[],
    const half*  gPixels,
    const float* zPixels,
    int          width,
    int          height)
{
    Header header (width, height);
    header.channels ().insert ("G", Channel (HALF));
    header.channels ().insert ("Z", Channel (FLOAT));

    OutputFile file (fileName, header);

    FrameBuffer frameBuffer;

    frameBuffer.insert (
        "G",                             // name
        Slice (
            HALF,                        // type
            (char*) gPixels,             // base
            sizeof (*gPixels) * 1,       // xStride
            sizeof (*gPixels) * width)); // yStride

    frameBuffer.insert (
        "Z",                             // name
        Slice (
            FLOAT,                       // type
            (char*) zPixels,             // base
            sizeof (*zPixels) * 1,       // xStride
            sizeof (*zPixels) * width)); // yStride

    file.setFrameBuffer (frameBuffer);
    file.writePixels (height);
}
// [end writeGZ1]

// Computing address of G and Z channels
const half*  gPixels;
const float* zPixels;
int x, y, width;

half* G =
// [begin compteChannelG]
(half*)((char*)gPixels + x * sizeof(half) * 1 + y * sizeof(half) * width);
    // = (half*)((char*)gPixels + x * 2 + y * 2 * width);
// [end compteChannelG]

// [begin compteChannelZ]
float* Z =
(float*)((char*)zPixels + x * sizeof(float) * 1 + y * sizeof(float) * width);
    // = (float*)((char*)zPixels + x * 4 + y * 4 * width);
// [end compteChannelZ]
