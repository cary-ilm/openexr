//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#define PYBIND11_DETAILED_ERROR_MESSAGES 1

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
    
