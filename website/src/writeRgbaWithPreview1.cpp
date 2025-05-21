void
writeRgbaWithPreview1 (
    const char fileName[], const Array2D<Rgba>& pixels, int width, int height)
{
    Array2D<PreviewRgba> previewPixels;

    int previewWidth;
    int previewHeight;

    makePreviewImage (
        pixels,
        width,
        height,
        previewPixels,
        previewWidth,
        previewHeight);

    Header header (width, height);
    header.setPreviewImage (
        PreviewImage (previewWidth, previewHeight, &previewPixels[0][0]));

    RgbaOutputFile file (fileName, header, WRITE_RGBA);
    file.setFrameBuffer (&pixels[0][0], 1, width);
    file.writePixels (height);
}
