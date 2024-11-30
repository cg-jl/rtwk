#include "sphere.h"

#include <ranges>
#include <sys/types.h>
#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "trace_colors.h"

static point3 sphere_center(sphere const &sph, float time)
{
    // Linearly interpolate from center1 to center2 according to time, where
    // t=0 yields center1, and t=1 yields center2.
    return sph.center1 + time * sph.center_vec;
}

void sphere::hit(ray const *rays, float const *times, uint32 const len, float *results) const noexcept
{
    ZoneNamedN(_tracy, "sphere hit", filters::hit);
    // @perf think about splitting transform up.
    // @perf `times` is not modified until the next pixel. Probably should cache it :]
    std::transform(rays, rays + len, times, results, [&](auto const &r, auto const time) -> float {
        point3 center = sphere_center(*this, time);
        vec3 oc = center - r.orig;
        auto a = r.dir.length_squared();
        // Distance from ray origin to sphere center parallel to the ray
        // direction
        auto oc_alongside_ray = dot(r.dir, oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = oc_alongside_ray * oc_alongside_ray - a * c;
        if (discriminant < 0)
            return 0;

        auto sqrtd = std::sqrt(discriminant);

        // If the ray is inside the sphere, we want the cut that gets
        // us further, since the other cut is in the other direction.
        // Otherwise we want the cut that is in the negative direction from the
        // middle point (oc_alongside_ray). If the ray is pointing away from the
        // sphere, we will catch this in `ray_t.contains`.

        // c > 0 <=> |oc|^2 > r^2
        auto selectedSqrt = c < minRayDist ? sqrtd : -sqrtd;

        // Find the nearest root that lies in the acceptable range.
        auto root = (oc_alongside_ray + selectedSqrt) / a;

        return root;
    });
}

void sphere::traverse(ray const *rays, float const *times, uint32 const len, interval *results) const noexcept
{
    // @perf separate transform into smaller pieces
    // @perf same thing wrt time.
    std::transform(rays, rays + len, times, results, [&](auto const &r, auto const time) {
        // NOTE: @cutnpaste from sphere::hit
        ZoneNamedNC(_tracy, "sphere traverse", Ctp::Mantle, filters::hit);
        point3 center = sphere_center(*this, time);
        vec3 oc = center - r.orig;
        auto a = r.dir.length_squared();
        // Distance from ray origin to sphere center parallel to the ray
        // direction
        auto oc_alongside_ray = dot(r.dir, oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = oc_alongside_ray * oc_alongside_ray - a * c;
        if (discriminant < 0)
            return interval { infinity, -infinity };

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
void sphere::getUVs(vec3 const *normals, uvs *results, uint32 start, uint32 end) noexcept
{
    // @perf check out.
    using std::ranges::subrange;
    std::ranges::transform(
        subrange(normals + start, normals + end),
        results + start, [&](auto const &normal) -> uvs {
            auto theta = std::acos(-normal.y());
            auto phi = std::atan2(-normal.z(), normal.x()) + pi;

            uvs uv;
            uv.u = phi / (2 * pi);
            uv.v = theta / pi;
            return uv;
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

static vec3 getNormal(sphere const &sph, point3 const intersection, float time)
{
    return (intersection - sphere_center(sph, time)) / sph.radius;
}

sphere sphere::applyTransform(sphere a, transform tf) noexcept
{
    auto previous = a.center1;
    a.center1 = tf.applyForward(a.center1);
    a.center_vec = tf.applyForward(previous + a.center_vec) - a.center1;
    return a;
}
void sphere::getNormals(ray const *rays, float const *dist, float const *times, vec3 *results, uint32 start, uint32 end) const noexcept
{
    // @pefr check out.
    std::transform(dist + start, dist + end, std::views::iota(start).begin(), results + start, [&](auto const closestHit, auto const i) {
        auto r = rays[i];
        auto time = times[i];
        auto intersection = r.at(closestHit);
        return ::getNormal(*this, intersection, time);
    });
}
