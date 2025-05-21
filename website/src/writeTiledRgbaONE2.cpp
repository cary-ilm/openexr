void
writeTiledRgbaONE2 (
    const char fileName[], int width, int height, int tileWidth, int tileHeight)
{
    TiledRgbaOutputFile out (
        fileName,
        width,
        height, // image size
        tileWidth,
        tileHeight,  // tile size
        ONE_LEVEL,   // level mode
        ROUND_DOWN,  // rounding mode
        WRITE_RGBA); // channels in file

    Array2D<Rgba> pixels (tileHeight, tileWidth);

    for (int tileY = 0; tileY < out.numYTiles (); ++tileY)
    {
        for (int tileX = 0; tileX < out.numXTiles (); ++tileX)
        {
            Box2i range = out.dataWindowForTile (tileX, tileY);

            generatePixels (pixels, width, height, range);

            out.setFrameBuffer (
                &pixels[-range.min.y][-range.min.x],
                1,          // xStride
                tileWidth); // yStride

            out.writeTile (tileX, tileY);
        }
    }
}
