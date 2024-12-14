#include "quad.h"

#include <external/glm/glm/ext/matrix_float2x3.hpp>
#include <external/glm/glm/ext/vector_float3.hpp>
#include <external/glm/glm/matrix.hpp>
#include <ranges>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

static auto constexpr eps = 1e-5f;

// Creates pseudoinverse from the (3x2) matrix [u, v], such that
// the new matrix is a base conversion from 3D space to the quad's
// plane, scaled by 'u' and 'v' such that the rectangle [0, 1]x[0, 1]
// represents the full quad surface.
static glm::mat2x3 create_pinv(glm::vec3 u, glm::vec3 v)
{
    glm::mat2x3 xt { u, v };

    auto x = glm::transpose(xt);

    auto xtx = xt * x;

    // this prevents the matrix from becoming singular.
    auto inv = glm::inverse(xtx + eps);

    return inv * xt;
}

void quad::getUVs(ray const *rays, float const *dist, uv_buffer results, uint32 start, uint32 end) const noexcept
{
    using std::ranges::subrange;
    using std::ranges::views::zip;

    auto const get_u = glm::vec3 { pinv[0][0], pinv[0][1], pinv[0][2] };
    auto const get_v = glm::vec3 { pinv[1][0], pinv[1][1], pinv[1][2] };

    // @perf cache intersections

    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
                               subrange(dist + start, dist + end)),
        results.v + start, [&](auto const &t) {
            auto const &[r, closestHit] = t;
            auto p = r.at(closestHit);
            return glm::dot(glm::vec3(p - Q), get_v);
        });
    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
                               subrange(dist + start, dist + end)),
        results.u + start, [&](auto const &t) {
            auto const &[r, closestHit] = t;
            auto p = r.at(closestHit);
            return glm::dot(glm::vec3(p - Q), get_u);
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
        // vertical distance to plane.
        // "vertical" in the sense of "normal to the plane".
        auto vdist2plane = dot(normal, Q - r.orig);

        // Rate of (vertical) approximation to plane. Adding a small constant
        // to avoid divisions by zero: parallel rays will have very high 't's ~= 1/eps,
        // and thus will be discarded.
        auto vrate2plane = dot(normal, r.dir) + eps;

        auto t = vdist2plane / vrate2plane;

        auto uv = t * (r.dir * pinv) - (Q - r.orig) * pinv;

        auto mask = is_interior(uv.x, uv.y);

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        return mask ? t : 0.f;
    });
}

vec3 quad::getNormal() const
{
    return normal;
}

// @perf length(u) == length(v)?
// @perf dot(u,v ) == 0.
quad quad::applyTransform(quad q, transform tf) noexcept
{
    auto oldQ = q.Q;
    q.Q = tf.applyForward(q.Q);
    q.u = tf.applyForward(oldQ + q.u) - q.Q;
    q.v = tf.applyForward(oldQ + q.v) - q.Q;
    q.pinv = create_pinv(q.u, q.v);
    q.normal = unit_vector(cross(q.u, q.v));
    return q;
}

quad::quad(point3 Q, vec3 u, vec3 v) noexcept
    : Q(Q)
    , u(u)
    , v(v)
{
    pinv = create_pinv(u, v);
    normal = unit_vector(cross(u, v));
    assert(dot(v, u) == 0.);
}
