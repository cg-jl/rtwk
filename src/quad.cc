#include "quad.h"

#include <ranges>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

static float getUVs_u(quad const &q, point3 intersection)
{
    // I have to make a base change, from [x y z] to [n u v], then extract the u
    // and the v

    vec3 pq = intersection - q.Q;
    auto u_squared = q.u.length_squared();
    auto dot_uq = dot(q.u, pq);
    // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
    return dot_uq / u_squared;
}

static float getUVs_v(quad const &q, point3 intersection)
{
    // I have to make a base change, from [x y z] to [n u v], then extract the u
    // and the v

    vec3 pq = intersection - q.Q;
    auto v_squared = q.v.length_squared();
    auto dot_vq = dot(q.v, pq);
    // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
    return dot_vq / v_squared;
}

void quad::getUVs(ray const *rays, float const *dist, uv_buffer results, uint32 start, uint32 end) const noexcept
{
    using std::ranges::subrange;
    using std::ranges::views::zip;

    // @perf cache intersections

    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
                               subrange(dist + start, dist + end)),
        results.v + start, [&](auto const &t) {
            auto const &[r, closestHit] = t;
            auto p = r.at(closestHit);
            return ::getUVs_v(*this, p);
        });
    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
                               subrange(dist + start, dist + end)),
        results.u + start, [&](auto const &t) {
            auto const &[r, closestHit] = t;
            auto p = r.at(closestHit);
            return ::getUVs_u(*this, p);
        });
}

static bool is_interior(float a, float b)
{
    static constexpr interval unit_interval = interval(0, 1);
    // Given the hit point in plane coordinates, return false if it is
    // outside the primitive, otherwise set the hit record UV coordinates
    // and return true.

    return unit_interval.contains(a) & unit_interval.contains(b);
}

// @perf length(u) == length(v)?
// @perf dot(u,v ) == 0.
void quad::hit(ray const *rays, uint32 const len, float *results) const noexcept
{
    ZoneNamedN(_tracy, "quad hit", filters::hit);
    // @perf think about splitting this transform up.
    // @perf getUVs() could be cached :]
    std::transform(rays, rays + len, results, [&](auto const &r) -> float {
        auto n = cross(u, v);
        auto normal = unit_vector(n);
        // n.Q = (uxv).Q =(triple product expansion) = u.(vxQ) = v.(uxQ)
        // trying to use dot(u,v) = 0 here?
        auto D = dot(normal, Q);
        auto denom = dot(normal, r.dir);

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8)
            return 0;

        // Return false if the hit point parameter t is outside the ray
        // interval.
        auto t = (D - dot(normal, r.orig)) / denom;
        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        // @perf dot(u,v) == 0. Try to find a relationship with `t`.
        // @perf may want to make a bulk visit :]
        auto u = ::getUVs_u(*this, intersection);
        auto v = ::getUVs_v(*this, intersection);

        if (!is_interior(u, v))
            return {};

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        return t;
    });
}

vec3 quad::getNormal() const
{
    return unit_vector(cross(u, v));
}

// @perf length(u) == length(v)?
// @perf dot(u,v ) == 0.
quad quad::applyTransform(quad q, transform tf) noexcept
{
    auto oldQ = q.Q;
    q.Q = tf.applyForward(q.Q);
    q.u = tf.applyForward(oldQ + q.u) - q.Q;
    q.v = tf.applyForward(oldQ + q.v) - q.Q;
    return q;
}
