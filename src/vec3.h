#ifndef VEC3_H
#define VEC3_H
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

#include <cmath>

#include "rtweekend.h"
class vec3 {
   public:
    double e[3];

    constexpr vec3() : e{0, 0, 0} {}
    constexpr vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

    constexpr double x() const { return e[0]; }
    constexpr double y() const { return e[1]; }
    constexpr double z() const { return e[2]; }

    constexpr vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
    constexpr double operator[](int i) const { return e[i]; }
    constexpr double &operator[](int i) { return e[i]; }

    constexpr vec3 &operator+=(vec3 const &v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    constexpr vec3 &operator*=(double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    constexpr vec3 &operator/=(double t) { return *this *= 1 / t; }

    double length() const { return std::sqrt(length_squared()); }

    constexpr double length_squared() const {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    constexpr bool near_zero() const {
        // Return true if the vector is close to zero in all dimensions.
        auto s = 1e-8;
        return (std::abs(e[0]) < s) && (std::abs(e[1]) < s) &&
               (std::abs(e[2]) < s);
    }

    static vec3 random() {
        return vec3(random_double(), random_double(), random_double());
    }

    static vec3 random(double min, double max) {
        return vec3(random_double(min, max), random_double(min, max),
                    random_double(min, max));
    }
};

// point3 is just an alias for vec3, but useful for geometric clarity in the
// code.
using point3 = vec3;

// Vector Utility Functions

inline std::ostream &operator<<(std::ostream &out, vec3 v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline constexpr vec3 operator+(vec3 u, vec3 v) {
    return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline constexpr vec3 operator-(vec3 u, vec3 v) {
    return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline constexpr vec3 operator*(vec3 u, vec3 v) {
    return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline constexpr vec3 operator*(double t, vec3 v) {
    return vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline constexpr vec3 operator*(vec3 v, double t) { return t * v; }

inline constexpr vec3 operator/(vec3 v, double t) { return (1 / t) * v; }

inline constexpr double dot(vec3 u, vec3 v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

inline constexpr vec3 cross(vec3 u, vec3 v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline vec3 unit_vector(vec3 v) { return v / v.length(); }

inline vec3 random_in_unit_disk() {
    while (true) {
        auto p = vec3(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() < 1) return p;
    }
}

inline vec3 random_in_unit_sphere() {
    while (true) {
        auto p = vec3::random(-1, 1);
        if (p.length_squared() < 1) return p;
    }
}

inline vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

inline vec3 random_on_hemisphere(vec3 normal) {
    vec3 on_unit_sphere = random_unit_vector();
    if (dot(on_unit_sphere, normal) >
        0.0)  // In the same hemisphere as the normal
        return on_unit_sphere;
    else
        return -on_unit_sphere;
}

inline vec3 reflect(vec3 v, vec3 n) { return v - 2 * dot(v, n) * n; }

// `uv`, `n` are assumed to be unit vectors.
inline vec3 refract(vec3 uv, vec3 n, double etai_over_etat) {
    auto cos_theta = -dot(uv, n);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    vec3 r_out_parallel =
        -std::sqrt(std::abs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

#endif
