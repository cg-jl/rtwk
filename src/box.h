#pragma once

#include <array>

#include "aaquad.h"
#include "geometry.h"
#include "hittable.h"

struct box : public geometry {
    std::array<aaquad, 6> faces;

    box(point3 a, point3 b) {
        // Returns the 3D box (six sides) that contains the two opposite
        // vertices a & b.

        // Construct the two opposite vertices with the minimum and maximum
        // coordinates.
        auto min =
            point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
        auto max =
            point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

        auto dx = max.x() - min.x();
        auto dy = max.y() - min.y();
        auto dz = max.z() - min.z();

        // TODO: watch for u, v coordinates being correct!
        faces[0] =
            (aaquad(point3(min.x(), min.y(), max.z()), 2, dx, dy));  // front
        faces[1] =
            (aaquad(point3(max.x(), min.y(), max.z()), 0, dy, -dz));  // right
        faces[2] =
            (aaquad(point3(min.x(), max.y(), max.z()), 1, -dz, dx));  // top
        faces[3] =
            (aaquad(point3(max.x(), min.y(), min.z()), 2, -dx, dy));  // back
        faces[4] =
            (aaquad(point3(min.x(), min.y(), min.z()), 0, dy, dz));  // left
        faces[5] =
            (aaquad(point3(min.x(), min.y(), min.z()), 1, dz, dx));  // bottom
    }

    aabb bounding_box() const final {
        aabb box = empty_aabb;
        for (auto const &face : faces) {
            box = aabb(box, face.bounding_box());
        }
        return box;
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const {
        bool hit = false;
        for (auto const &face : faces) {
            if (face.hit(r, ray_t, rec)) {
                ray_t.max = rec.t;
                hit = true;
            }
        }
        return hit;
    }

    void getUVs(uvs &uv, point3 intersection, vec3 normal) const {
        // know the face by checking the axis.
        int normal_axis = normal.x() == 0 ? normal.y() == 0 ? 2 : 1 : 0;

        for (auto const &face : faces) {
            if (face.axis == normal_axis &&
                std::signbit(face.u * face.v) ==
                    std::signbit(normal[normal_axis])) {
                return face.getUVs(uv, intersection, normal);
            }
        }
        __builtin_unreachable();
    }
};
