void
writeTiledRgbaONE1 (
    const char  fileName[],
    const Rgba* pixels,
    int         width,
    int         height,
    int         tileWidth,
    int         tileHeight)
{
    TiledRgbaOutputFile out (
        fileName,
        width,
        height,                            // image size
        tileWidth,
        tileHeight,                        // tile size
        ONE_LEVEL,                         // level mode
        ROUND_DOWN,                        // rounding mode
        WRITE_RGBA);                       // channels in file
    out.setFrameBuffer (pixels, 1, width);
    out.writeTiles (0, out.numXTiles () - 1, 0, out.numYTiles () - 1);
}
