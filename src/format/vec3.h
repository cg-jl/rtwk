#include <vec3.h>

#include <format>

template <>
struct std::formatter<vec3> : public std::formatter<double> {
    // NOTE: parsing is done by <double>

    auto format(vec3 const &v, auto &ctx) const {
        using df = std::formatter<double>;

        *ctx.out()++ = '[';
        auto out = df::format(v[0], ctx);
        *out++ = ' ';
        out = df::format(v[1], ctx);
        *out++ = ' ';
        out = df::format(v[2], ctx);
        *out++ = ']';
        return ctx.out();
    }
};
