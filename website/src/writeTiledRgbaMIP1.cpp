void
writeTiledRgbaMIP1 (
    const char fileName[], int width, int height, int tileWidth, int tileHeight)
{
    TiledRgbaOutputFile out (
        fileName,
        width,
        height,
        tileWidth,
        tileHeight,
        MIPMAP_LEVELS,
        ROUND_DOWN,
        WRITE_RGBA);

    Array2D<Rgba> pixels (height, width);

    out.setFrameBuffer (&pixels[0][0], 1, width);

    for (int level = 0; level < out.numLevels (); ++level)
    {
        generatePixels (pixels, width, height, level);

        out.writeTiles (
            0,
            out.numXTiles (level) - 1,
            0,
            out.numYTiles (level) - 1,
            level);
    }
}
