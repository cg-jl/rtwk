#pragma once
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

// Shared version of `rtw_image`. Contains the same data, but none of the
// constructors.
struct rtw_shared_image {
    float const *fdata;
    int image_width;
    int image_height;

    // NOTE: used by texture
    float const *pixel_data(int x, int y) const;
};

// Owned part of an image. Wraps stbi_image code.
class rtw_image {
   public:
    constexpr rtw_image() {}

    rtw_image(rtw_image &&img);
    rtw_image(char const *image_filename);

    constexpr rtw_shared_image share() const {
        return {fdata, image_width, image_height};
    }

    ~rtw_image();

    float *fdata = nullptr;  // Linear floating point pixel data
    int image_width = 0;     // Loaded image width
    int image_height = 0;    // Loaded image height
};
