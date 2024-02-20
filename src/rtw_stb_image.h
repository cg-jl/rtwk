#ifndef RTW_STB_IMAGE_H
#define RTW_STB_IMAGE_H

// Disable strict warnings for this header from the Microsoft Visual C++
// compiler.
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <cstdlib>
#include <iostream>

#include "stb_image.h"

struct rtw_image {
   public:
    rtw_image() : data(nullptr) {}

    explicit rtw_image(char const* image_filename) : rtw_image() {
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
        if (imagedir && load(std::string(imagedir) + "/" + image_filename))
            return;
        if (load(filename)) return;
        if (load("images/" + filename)) return;
        if (load("../images/" + filename)) return;
        if (load("../../images/" + filename)) return;
        if (load("../../../images/" + filename)) return;
        if (load("../../../../images/" + filename)) return;
        if (load("../../../../../images/" + filename)) return;
        if (load("../../../../../../images/" + filename)) return;

        std::cerr << "ERROR: Could not load image file '" << image_filename
                  << "'.\n";
    }

    ~rtw_image() { STBI_FREE(data); }

    bool load(std::string const& filename) {
        // Loads image data from the given file name. Returns true if the load
        // succeeded.
        int _n;
        // per pixel
        data = stbi_load(filename.c_str(), &image_width, &image_height, &_n,
                         bytes_per_pixel);
        return data != nullptr;
    }

    [[nodiscard]] constexpr size_t bytes_per_scanline() const noexcept {
        return image_width * bytes_per_pixel;
    }

    [[nodiscard]] color sample(float u, float v) const {
        // Return the address of the three bytes of the pixel at x,y (or magenta
        // if no data).
        static auto magenta = color{1, 0, 1};
        if (data == nullptr) return magenta;

        assume(u >= 0 && u <= 1);
        assume(v >= 0 && v <= 1);

        auto x = uint32_t(float(image_width) * u);
        auto y = uint32_t(float(image_height) * (1 - v));

        auto pxdata = data + y * bytes_per_scanline() + x * bytes_per_pixel;
        static constexpr auto scale = 1 / 255.f;

        auto unscaled =
            color(static_cast<float>(pxdata[0]), static_cast<float>(pxdata[1]),
                  static_cast<float>(pxdata[2]));
        return scale * unscaled;
    }

   private:
    static constexpr uint32_t bytes_per_pixel = 3;
    // NOTE: we have to do one RGB8->RGB32f conversion per sample here... Is it
    // ok?
    uint8_t* data;
    int image_width{}, image_height{};
};

// Restore MSVC compiler warnings
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
