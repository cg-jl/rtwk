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

class rtw_image {
   public:
    constexpr rtw_image() {}

    rtw_image(char const *image_filename);

    ~rtw_image();

    // NOTE: used by texture
    float const *pixel_data(int x, int y) const;

    float *fdata = nullptr;          // Linear floating point pixel data
    int image_width = 0;             // Loaded image width
    int image_height = 0;            // Loaded image height
};
