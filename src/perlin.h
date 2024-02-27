#ifndef PERLIN_H
#define PERLIN_H
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

#include <array>
#include <cmath>
#include <span>

#include "rtweekend.h"
#include "vec3.h"

struct perlin {
   public:
    perlin() {
        for (auto& v : ranvec) {
            v = unit_vector(vec3::random(-1, 1));
        }

        for (int i = 0; i < perm_x.size(); i++) perm_x[i] = i;
        for (int i = 0; i < perm_y.size(); i++) perm_y[i] = i;
        for (int i = 0; i < perm_z.size(); i++) perm_z[i] = i;

        permute(perm_x);
        permute(perm_y);
        permute(perm_z);
    }

    [[nodiscard]] float noise(point3 const& p) const {
        auto u = p.x() - std::floor(p.x());
        auto v = p.y() - std::floor(p.y());
        auto w = p.z() - std::floor(p.z());
        auto i = static_cast<int>(std::floor(p.x()));
        auto j = static_cast<int>(std::floor(p.y()));
        auto k = static_cast<int>(std::floor(p.z()));
        vec3 c[2][2][2];

        for (int di = 0; di < 2; di++)
            for (int dj = 0; dj < 2; dj++)
                for (int dk = 0; dk < 2; dk++)
                    c[di][dj][dk] = ranvec[perm_x[(i + di) % point_count] ^
                                           perm_y[(j + dj) % point_count] ^
                                           perm_z[(k + dk) % point_count]];

        auto uu = u * u * (3 - 2 * u);
        auto vv = v * v * (3 - 2 * v);
        auto ww = w * w * (3 - 2 * w);
        auto accum = 0.0f;

        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    // NOTE: These are positive only when their i/j/k is 0,
                    // since u, v, w are fractional parts (< 1).
                    vec3 weightV(u - float(i), v - float(j), w - float(k));
                    accum += (float(i) * uu + float(1 - i) * (1 - uu)) *
                             (float(j) * vv + float(1 - j) * (1 - vv)) *
                             (float(k) * ww + float(1 - k) * (1 - ww)) *
                             dot(c[i][j][k], weightV);
                }

        return accum;
    }

    [[nodiscard]] float turb(point3 const& p) const {
        constexpr int depth = 2;
        auto accum = 0.0f;
        auto temp_p = p;
        auto weight = 1.0f;

        for (int i = 0; i < depth; i++) {
            accum += weight * noise(temp_p);
            weight *= 0.5f;
            temp_p *= 2;
        }

        return std::fabs(accum);
    }

   private:
    static int constexpr point_count = 128;
    std::array<vec3, point_count> ranvec;
    std::array<int, point_count> perm_x{};
    std::array<int, point_count> perm_y{};
    std::array<int, point_count> perm_z{};

    static void permute(std::span<int> p) {
        for (int i = int(p.size()); i > 0;) {
            --i;
            int target = random_int(0, i);
            std::swap(p[i], p[target]);
        }
    }
};

#endif
