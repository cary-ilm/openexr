//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

//
// PyFile is the object that corresponds to an exr file, either for reading
// or writing, consisting of a simple list of parts.
//

class PyPart;

class PyFile 
{
public:
    PyFile() {}
    PyFile(const std::string& filename);
    PyFile(const py::dict& header, const py::dict& channels,
           exr_storage_t type, exr_compression_t compression);
    PyFile(const py::list& parts);

    py::dict& header(int part_index = 0);
    py::dict& channels(int part_index = 0);

    void      write(const char* filename);
    
    bool operator==(const PyFile& other) const;
    
    std::string  filename;
    py::list     parts;

    void        RgbaInputFile(const char* filename);
    void        TiledRgbaInputFile(const char* filename);
    void        multiPartInputFile(const char* filename);
    void        multiPartOutputFile(const char* filename);
};

//
// PyPart holds the information for a part of an exr file: name, type,
// dimension, compression, the list of attributes (e.g. "header") and the
// list of channels.
//

class PyPart
{
  public:
    PyPart()
        : type(EXR_STORAGE_LAST_TYPE), width(0), height(0),
        compression (EXR_COMPRESSION_LAST_TYPE), part_index(0) {}
    PyPart(const char* name, const py::dict& a, const py::dict& channels,
           exr_storage_t type, exr_compression_t c);
    PyPart(exr_context_t f, int part_index);
    
    bool operator==(const PyPart& other) const;

    std::string           name;
    exr_storage_t         type;
    uint64_t              width;
    uint64_t              height;
    exr_compression_t     compression;

    py::dict              header;
    py::dict              channels;

    int                   part_index;
    
    void read_scanline_part (exr_context_t f);
    void read_tiled_part (exr_context_t f);

    py::object get_attribute_object(exr_context_t f, int32_t attr_index, std::string& name);

    void set_attributes(exr_context_t f); 
    void set_attribute(exr_context_t f, const std::string& name, py::object object);
    void add_channels(exr_context_t f);

    void write(exr_context_t f);
    void write_scanlines(exr_context_t f);
    void write_tiles(exr_context_t f);
};

//
// PyChannel holds information for a channel of a PyPart: name, type, x/y
// sampling, and the array of pixel data.
//
  
class PyChannel 
{
public:

    PyChannel()
        : xSampling(1), ySampling(1), channel_index(0) {}

    PyChannel(int x, int y)
        : xSampling(x), ySampling(y), channel_index(0) {}
    PyChannel(const py::array& p)
        : xSampling(1), ySampling(1), pixels(p), channel_index(0) { validate_pixel_array(); }
    PyChannel(const py::array& p, int x, int y)
        : xSampling(x), ySampling(y), pixels(p), channel_index(0) { validate_pixel_array(); }
        
    PyChannel(const char* n, int x = 1, int y = 1)
        : name(n), xSampling(x), ySampling(y), channel_index(0) {}
    PyChannel(const char* n, const py::array& p)
        : name(n), xSampling(1), ySampling(1), pixels(p), channel_index(0) { validate_pixel_array(); }
    PyChannel(const char* n, const py::array& p, int x, int y)
        : name(n), xSampling(x), ySampling(y), pixels(p), channel_index(0) { validate_pixel_array(); }

    bool operator==(const PyChannel& other) const;
    bool operator!=(const PyChannel& other) const { return !(*this == other); }

    void validate_pixel_array();
    
    exr_pixel_type_t      pixelType() const;

    std::string           name;
    int                   xSampling;
    int                   ySampling;
    py::array             pixels;

    size_t                channel_index;
    
    void set_encoder_channel(exr_encode_pipeline_t& encoder, size_t y, size_t width, size_t scansperchunk) const;
};
    
template <class T>
bool
array_equals(const py::buffer_info& a, const py::buffer_info& b, const std::string& name);

class PyPreviewImage
{
public:
    static constexpr uint32_t style = py::array::c_style | py::array::forcecast;
    static constexpr size_t stride = sizeof(PreviewRgba);

    PyPreviewImage() {}
    
    PyPreviewImage(unsigned int width, unsigned int height,
                   const PreviewRgba* data = nullptr)
        : pixels(py::array_t<PreviewRgba,style>(std::vector<size_t>({height, width}),
                                                std::vector<size_t>({stride*width, stride}),
                                                data)) {}
    
    PyPreviewImage(const py::array_t<PreviewRgba>& p) : pixels(p) {}

    inline bool operator==(const PyPreviewImage& other) const;

    py::array_t<PreviewRgba> pixels;
};
    
inline std::ostream&
operator<< (std::ostream& s, const PreviewRgba& p)
{
    s << " (" << int(p.r)
      << "," << int(p.g)
      << "," << int(p.b)
      << "," << int(p.a)
      << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const PyPreviewImage& P)
{
    auto width = P.pixels.shape(1);
    auto height = P.pixels.shape(0);
    
    s << "PreviewImage(" << width << ", " << height << "," << std::endl;
    py::buffer_info buf = P.pixels.request();
    const PreviewRgba* rgba = static_cast<PreviewRgba*>(buf.ptr);
    for (decltype(height) y = 0; y<height; y++)
    {
        for (decltype(width) x = 0; x<width; x++)
            s << rgba[y*width+x];
        s << std::endl;
    }
    return s;
}
    
inline bool
PyPreviewImage::operator==(const PyPreviewImage& other) const
{
    py::buffer_info buf = pixels.request();
    py::buffer_info obuf = other.pixels.request();
    
    const PreviewRgba* apixels = static_cast<PreviewRgba*>(buf.ptr);
    const PreviewRgba* bpixels = static_cast<PreviewRgba*>(obuf.ptr);
    for (decltype(buf.size) i = 0; i < buf.size; i++)
        if (!(apixels[i] == bpixels[i]))
            return false;
    return true;
}


//
// PyDouble supports the "double" attribute.
//
// When reading an attribute of type "double", a python object of type
// PyDouble is created, so that when the header is written, it will be
// of type double, since python makes no distinction between float and
// double numerical types.
//

class PyDouble
{
public:
    PyDouble(double x) : d(x)  {}

    bool operator==(const PyDouble& other) const { return d == other.d; }
    
    double d;
};
                         
class PyChromaticities
{
  public:
    PyChromaticities(float rx, float ry,
                     float gx, float gy, 
                     float bx, float by, 
                     float wx, float wy)
        : red_x(rx), red_y(ry),
          green_x(gx), green_y(gy),
          blue_x(bx), blue_y(by),
          white_x(wx), white_y(wy)
        {
        }

    bool operator==(const PyChromaticities& other) const
        {
            return (red_x == other.red_x &&
                    red_y == other.red_y &&
                    green_x == other.green_x &&
                    green_y == other.green_y &&
                    blue_x == other.blue_x &&
                    blue_y == other.blue_y &&
                    white_x == other.white_x &&
                    white_y == other.white_y);
        }
    
    float red_x;
    float red_y;
    float green_x;
    float green_y;
    float blue_x;
    float blue_y;
    float white_x;
    float white_y;
};

inline std::ostream&
operator<< (std::ostream& s, const PyChromaticities& c)
{
    s << "(" << c.red_x
      << ", " << c.red_y
      << ", " << c.green_x 
      << ", " << c.green_y
      << ", " << c.blue_x
      << ", " << c.blue_y
      << ", " << c.white_x
      << ", " << c.white_y
      << ")";
    return s;
}
    
inline std::ostream&
operator<< (std::ostream& s, const Rational& v)
{
    s << v.n << "/" << v.d;
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const KeyCode& v)
{
    s << "(" << v.filmMfcCode()
      << ", " << v.filmType()
      << ", " << v.prefix()
      << ", " << v.count()
      << ", " << v.perfOffset()
      << ", " << v.perfsPerFrame()
      << ", " << v.perfsPerCount()
      << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const TimeCode& v)
{
    s << "(" << v.hours()
      << ", " << v.minutes()
      << ", " << v.seconds()
      << ", " << v.frame()
      << ", " << v.dropFrame()
      << ", " << v.colorFrame()
      << ", " << v.fieldPhase()
      << ", " << v.bgf0()
      << ", " << v.bgf1()
      << ", " << v.bgf2()
      << ")";
    return s;
}


inline std::ostream&
operator<< (std::ostream& s, const TileDescription& v)
{
    s << "TileDescription(" << v.xSize
      << ", " << v.ySize
      << ", " << py::cast(v.mode)
      << ", " << py::cast(v.roundingMode)
      << ")";

    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const Box2i& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}

inline std::ostream&
operator<< (std::ostream& s, const Box2f& v)
{
    s << "(" << v.min << "  " << v.max << ")";
    return s;
}


inline std::ostream&
operator<< (std::ostream& s, const PyChannel& C)
{
    return s << "Channel(\"" << C.name 
             << "\", xSampling=" << C.xSampling 
             << ", ySampling=" << C.ySampling
             << ")";
}

inline std::ostream&
operator<< (std::ostream& s, const PyPart& P)
{
    return s << "Part(\"" << P.name 
             << "\", type=" << py::cast(P.type) 
             << ", width=" << P.width
             << ", height=" << P.height
             << ", compression=" << P.compression
             << ")";
}

