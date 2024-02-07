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
#include <iostream>

#include "rtweekend.h"

using std::fabs;
using std::sqrt;

struct vec3 {
   public:
    float e[3];

    vec3() : e{0, 0, 0} {}
    vec3(float e0, float e1, float e2) : e{e0, e1, e2} {}

    [[nodiscard]] float x() const { return e[0]; }
    [[nodiscard]] float y() const { return e[1]; }
    [[nodiscard]] float z() const;

    vec3 operator-() const { return {-e[0], -e[1], -e[2]}; }
    float operator[](int i) const { return e[i]; }
    float &operator[](int i) { return e[i]; }

    vec3 &operator+=(vec3 v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }
    vec3 &operator*=(vec3 v) {
        e[0] *= v.e[0];
        e[1] *= v.e[1];
        e[2] *= v.e[2];
        return *this;
    }

    vec3 &operator*=(float t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    vec3 &operator/=(float t) { return *this *= 1 / t; }

    [[nodiscard]] float length() const { return sqrtf(length_squared()); }

    [[nodiscard]] float length_squared() const {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    [[nodiscard]] bool near_zero() const {
        // Return true if the vector is close to zero in all dimensions.
        auto s = 1e-8;
        return (fabs(e[0]) < s) && (fabs(e[1]) < s) && (fabs(e[2]) < s);
    }

    static vec3 random() {
        return {random_float(), random_float(), random_float()};
    }

    static vec3 random(float min, float max) {
        return {random_float(min, max), random_float(min, max),
                random_float(min, max)};
    }
};
float vec3::z() const { return e[2]; }

// point3 is just an alias for vec3, but useful for geometric clarity in the
// code.
using point3 = vec3;

// Vector Utility Functions

static std::ostream &operator<<(std::ostream &out, vec3 v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline vec3 operator+(vec3 u, vec3 v) {
    return {u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]};
}

inline vec3 operator-(vec3 u, vec3 v) {
    return {u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]};
}

static vec3 operator*(vec3 u, vec3 v) {
    return {u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]};
}

static vec3 operator*(float t, vec3 v) {
    return {t * v.e[0], t * v.e[1], t * v.e[2]};
}

static vec3 operator*(vec3 v, float t) { return t * v; }

static vec3 operator/(vec3 v, float t) { return (1 / t) * v; }

static float dot(vec3 u, vec3 v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

static vec3 cross(vec3 u, vec3 v) {
    return {u.e[1] * v.e[2] - u.e[2] * v.e[1],
            u.e[2] * v.e[0] - u.e[0] * v.e[2],
            u.e[0] * v.e[1] - u.e[1] * v.e[0]};
}

static vec3 unit_vector(vec3 v) { return v / v.length(); }

static vec3 random_in_unit_disk() {
    auto p = unit_vector(vec3::random(-1, 1));
    auto s = random_float(0, 1);

    return s * p;
}

static vec3 random_unit_vector() { return unit_vector(vec3::random(-1, 1)); }

static vec3 random_in_unit_sphere() {
    auto p = random_unit_vector();

    auto s = random_float(0, 1);

    return s * p;
}

// if |v| = |n|  = 1 => |reflect(v, n)| = 1
static vec3 reflect(vec3 v, vec3 n) { return v - 2 * dot(v, n) * n; }

// if |v| = |n| = 1 => |refract(uv, n)| = eta_squared
static vec3 refract(vec3 uv, vec3 n, float cos_theta, float etai_over_etat) {
    // if |uv| = |n| = 1 => |r_out_perp|^2 = etai_over_etat^2*(1 - cos_theta^2)
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    auto eta_squared = etai_over_etat * etai_over_etat;

    // |uv| = |n| = 1 => |r_out_parallel|^2 = 1 - eta_squared * sin_theta^2
    vec3 r_out_parallel =
        -sqrtf(1.0f - eta_squared + eta_squared * cos_theta * cos_theta) * n;

    // |r_out_parallel + r_out_perp|^2 = |r_out_perp|^2 + |r_out_parallel|^2 +
    // 2*dot(r_out_perp, r_out_parallel) dot(r_out_parallel, r_out_perp) = 0 by construction, so:
    // |r_out_parallel + r_out_perp|^2 = eta_squared

    return (r_out_perp + r_out_parallel) / eta_squared;
}

#endif
