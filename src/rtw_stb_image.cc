// Disable strict warnings for this header from the Microsoft Visual C++
// compiler.
#include <ostream>
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

// Disable deprecation of getenv(), even though I don't know what I'm doing
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#define STBI_FAILURE_USERMSG
#include <cstdlib>
#include <iostream>
#include <string>

#include "external/stb_image.h"
#include "rtw_stb_image.h"

static constexpr int floats_per_pixel = 3;

static unsigned char float_to_byte(float value) {
    if (value <= 0.0) return 0;
    if (1.0 <= value) return 255;
    return static_cast<unsigned char>(256.0 * value);
}

static bool load(rtw_image &img, std::string const &filename) {
    // Loads the linear (gamma=1) image data from the given file name.
    // Returns true if the load succeeded. The resulting data buffer
    // contains the three [0.0, 1.0] floating-point values for the first
    // pixel (red, then green, then blue). Pixels are contiguous, going left
    // to right for the width of the image, followed by the next row below,
    // for the full height of the image.

    auto n = floats_per_pixel;  // Dummy out parameter: original components
                                // per pixel
    img.fdata = stbi_loadf(filename.c_str(), &img.image_width,
                           &img.image_height, &n, floats_per_pixel);
    if (img.fdata == nullptr) {
        img.image_width = 0;
        img.image_height = 0;
        return false;
    }

    return true;
}

rtw_image::rtw_image(char const *image_filename) {
    // Loads image data from the specified file. If the RTW_IMAGES
    // environment variable is defined, looks only in that directory for the
    // image file. If the image was not found, searches for the specified
    // image file first from the current directory, then in the images/
    // subdirectory, then the _parent's_ images/ subdirectory, and then
    // _that_ parent, on so on, for six levels up. If the image was not
    // loaded successfully, width() and height() will return 0.

    auto filename = std::string(image_filename);
    auto imagedir = getenv("RTW_IMAGES");

    // Hunt for the image file in some likely locations.
    if (imagedir && load(*this, std::string(imagedir) + "/" + image_filename))
        return;
    if (load(*this, filename)) return;
    if (load(*this, "images/" + filename)) return;
    if (load(*this, "../images/" + filename)) return;
    if (load(*this, "../../images/" + filename)) return;
    if (load(*this, "../../../images/" + filename)) return;
    if (load(*this, "../../../../images/" + filename)) return;
    if (load(*this, "../../../../../images/" + filename)) return;
    if (load(*this, "../../../../../../images/" + filename)) return;

    std::cerr << "ERROR: Could not load image file '" << image_filename
              << "'.\n";
}

static int clamp(int x, int low, int high) {
    // Return the value clamped to the range [low, high).
    if (x < low) return low;
    if (x < high) return x;
    return high - 1;
}

float const *rtw_shared_image::pixel_data(int x, int y) const {
    // Return the address of the three RGB bytes of the pixel at x,y. If
    // there is no image data, returns magenta.
    static float magenta[] = {1, 0, 1};
    if (fdata == nullptr) return magenta;

    x = clamp(x, 0, image_width);
    y = clamp(y, 0, image_height);

    return fdata + y * image_width * floats_per_pixel + x * floats_per_pixel;
}

rtw_image::~rtw_image() { free(fdata); }
rtw_image::rtw_image(rtw_image &&img)
    : fdata(std::exchange(img.fdata, nullptr)),
      image_width(img.image_width),
      image_height(img.image_height) {}

// Restore MSVC compiler warnings
#ifdef _MSC_VER
#pragma warning(pop)
#endif
