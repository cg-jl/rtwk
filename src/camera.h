#pragma once
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "hittable_list.h"

class camera {
   public:
    // Ratio of image width over height. NOTE: Not used during render.
    double aspect_ratio = 1.0;

    int image_width = 100;       // Rendered image width in pixel count
    int samples_per_pixel = 10;  // Count of random samples for each pixel
    int max_depth = 10;          // Maximum number of ray bounces into scene
    color background;            // Scene background color

    // NOTE: These are only used for config. Consider moving them.
    double vfov = 90;                   // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, 0);  // Point camera is looking from
    point3 lookat = point3(0, 0, -1);   // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);           // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist =
        10;  // Distance from camera lookfrom point to plane of perfect focus

    void render(hittable_list const &world);

    int image_height;  // Rendered image height
    double
        pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    point3 center;            // Camera center
    point3 pixel00_loc;       // Location of pixel 0, 0
    vec3 pixel_delta_u;       // Offset to pixel to the right
    vec3 pixel_delta_v;       // Offset to pixel below
    vec3 u, v, w;             // Camera frame basis vectors
    vec3 defocus_disk_u;      // Defocus disk horizontal radius
    vec3 defocus_disk_v;      // Defocus disk vertical radius
};
