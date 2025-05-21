void
writeTiled1 (
    const char   fileName[],
    Array2D<GZ>& pixels,
    int          width,
    int          height,
    int          tileWidth,
    int          tileHeight)
{

    Header header (width, height);
    header.channels ().insert ("G", Channel (HALF));
    header.channels ().insert ("Z", Channel (FLOAT));

    header.setTileDescription (
        TileDescription (tileWidth, tileHeight, ONE_LEVEL));

    TiledOutputFile out (fileName, header);

    FrameBuffer frameBuffer;

    frameBuffer.insert (
        "G",                                 // name
        Slice (
            HALF,                            // type
            (char*) &pixels[0][0].g,         // base
            sizeof (pixels[0][0]) * 1,       // xStride
            sizeof (pixels[0][0]) * width)); // yStride

    frameBuffer.insert (
        "Z",                                 // name
        Slice (
            FLOAT,                           // type
            (char*) &pixels[0][0].z,         // base
            sizeof (pixels[0][0]) * 1,       // xStride
            sizeof (pixels[0][0]) * width)); // yStride

    out.setFrameBuffer (frameBuffer);
    out.writeTiles (0, out.numXTiles () - 1, 0, out.numYTiles () - 1);
}
