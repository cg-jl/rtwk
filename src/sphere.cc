#include "sphere.h"

#include <ranges>
#include <sys/types.h>
#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "interval.h"
#include "ray.h"

static point3 sphere_center(sphere const &sph, float time)
{
    // Linearly interpolate from center1 to center2 according to time, where
    // t=0 yields center1, and t=1 yields center2.
    return sph.center1 + time * sph.center_vec;
}

void sphere::hit(transposed_ray_array rays, float const *noalias times, Hit_Buffers buffers, uint32 const len, float *noalias results) const noexcept
{
    ZoneNamedN(_tracy, "sphere hit", filters::hit);

    // @perf `times` is not modified until the next pixel. Probably should cache it :]
    std::transform(times, times + len, buffers.ocs, [&](auto const time) {
        return sphere_center(*this, time);
    });

    auto const rin = rays.read_scalars(len);

    // @perf separate ray orig and dir
    std::transform(buffers.ocs, buffers.ocs + len, rin.begin(), buffers.ocs, [&](auto const center, auto const &r) {
        return center - r.orig;
    });

    // @perf separte ray orig and dir
    // @perf think about splitting transform up.
    std::transform(rin.begin(), rin.end(), buffers.ocs, results, [&](auto const &r, auto const oc) -> float {
        // Distance from ray origin to sphere center parallel to the ray
        // direction
        auto oc_alongside_ray = dot(r.dir, oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = oc_alongside_ray * oc_alongside_ray - c;

        // sqrt() will return NaN if the discriminant is negative, which will be discarded by hitSpan.
        auto sqrtd = std::sqrt(discriminant);

        // Always use - sqrtd for the min distance.
        auto root = (oc_alongside_ray - sqrtd);

        return root;
    });
}

void sphere::traverse(transposed_ray_array rays, float const *times, uint32 const len, interval_buffer results) const noexcept
{

    struct zipped_outbuf {
        interval_buffer const *results;
        uint32 index;

        struct assign_proxy {
            float *min;
            float *max;

            assign_proxy &operator=(interval x)
            {
                *min = x.min;
                *max = x.max;
                return *this;
            }

            operator interval() const noexcept { return { *min, *max }; }
        };

        assign_proxy operator*() const noexcept
        {
            return assign_proxy {
                .min = results->mins + index,
                .max = results->maxes + index
            };
        }

        zipped_outbuf &operator++() noexcept
        {
            ++index;
            return *this;
        }

        auto constexpr operator<=>(zipped_outbuf const &other) { return index <=> other.index; }
    };

    auto const rin = rays.read_scalars(len);

    // @perf separate transform into smaller pieces
    // @perf same thing wrt time.
    std::transform(rin.begin(), rin.end(), times, zipped_outbuf { &results, 0 }, [&](auto const &r, auto const time) {
        // NOTE: @cutnpaste from sphere::hit
        ZoneNamedNC(_tracy, "sphere traverse", Ctp::Mantle, filters::hit);
        point3 center = sphere_center(*this, time);
        vec3 oc = center - r.orig;
        // Distance from ray origin to sphere center parallel to the ray
        // direction
        auto oc_alongside_ray = dot(r.dir, oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = oc_alongside_ray * oc_alongside_ray - c;

        auto sqrtd = std::sqrt(discriminant);

        interval intersect;
        intersect.min = (oc_alongside_ray - sqrtd);
        intersect.max = (oc_alongside_ray + sqrtd);

        // min > max? <=> (oc_alongside_ray - sqrtd) / a > (oc_alongside_ray +
        // sqrtd) / a a is always > 0 => oc_alongside_ray - sqrtd > oc_alongside_ray
        // + sqrtd
        // <=> 0 > 2*sqrtd, sqrtd >= 0 hence it's always false.

        return intersect;
    });
}

// normal: Surface normal at the hit point.
// u: returned value [0,1] of angle around the Y axis from X=-1.
// v: returned value [0,1] of angle from Y=-1 to Y=+1.
//     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
//     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
//     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>
void sphere::getUVs(vec3 const *normals, uv_buffer results, uint32 start, uint32 end) noexcept
{
    // @perf could use soa'd vecs

    std::transform(
        normals + start, normals + end,
        results.v, [&](auto const &normal) {
            auto theta = std::acos(-normal.y());
            return theta / pi;
        });

    std::transform(
        normals + start, normals + end,
        results.u, [&](auto const &normal) {
            auto phi = std::atan2(-normal.z(), normal.x()) + pi;
            return phi / (2 * pi);
        });
}

aabb sphere::bounding_box() const
{
    auto rvec = vec3(radius, radius, radius);
    auto center2 = center1 + center_vec;
    aabb box1(center1 - rvec, center1 + rvec);
    aabb box2(center2 - rvec, center2 + rvec);
    return aabb(box1, box2);
}

sphere sphere::applyTransform(sphere a, transform tf) noexcept
{
    auto previous = a.center1;
    a.center1 = tf.applyForward(a.center1);
    a.center_vec = tf.applyForward(previous + a.center_vec) - a.center1;
    return a;
}
[[clang::noinline]] void sphere::getNormals(transposed_ray_array rays, float const *dist, float const *times, vec3 *results, uint32 start, uint32 end) const noexcept
{

    // @perf could use soa'd vecs.
    std::transform(dist + start, dist + end, rays.read_scalars(end, start).begin(), results + start, [&](auto const closestHit, auto const &r) {
        return r.at(closestHit);
    });
    std::transform(results + start, results + end, times, results + start, [&](auto const intersection, auto const time) {
        return intersection - sphere_center(*this, time);
    });

    // normalize.
    auto *fresults = (float *)results;
    auto const fstart = start * 3;
    auto const fend = end * 3;

    std::transform(fresults + fstart, fresults + fend, fresults + fstart, [rad = radius](auto const coord) {
        // FIXME: Some of the intersections here are not at a distance 'radius' away from the calculated center.
        // Are we swapping things correctly in renderer?
        return coord / rad;
    });
}
