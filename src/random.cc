#include "random.h"

#include "rtweekend.h"
#include "vec3.h"

// Adapted from glibc/glibc/stdlib/rand_r.c
/* This algorithm is mentioned in the ISO C standard, here extended
   for 32 bits.  */
static int next_rand(unsigned int *seed)
{
    unsigned int next = *seed;
    int result;
    next *= 1103515245;
    next += 12345;
    result = (unsigned int)(next / 65536) % 2048;
    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int)(next / 65536) % 1024;
    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int)(next / 65536) % 1024;
    *seed = next;
    return result;
}

// Since it's got a thread local static, we should only have one per thread.
// Having one per cc file that uses random util is just wasteful.
float random_float()
{
    static thread_local unsigned int seed = 0;
    return float(next_rand(&seed) & RAND_MAX) / (float(RAND_MAX) + 1);
}

vec3 random_vec(float min, float max)
{
    return vec3(random_float(min, max), random_float(min, max),
        random_float(min, max));
}
