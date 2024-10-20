#include "random.h"


#include "rtweekend.h"
#include "vec3.h"

// Adapted from glibc/glibc/stdlib/rand_r.c
/* This algorithm is mentioned in the ISO C standard, here extended
   for 32 bits.  */
static int next_rand(unsigned int *seed) {
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
double random_double() {
    static thread_local unsigned int seed = 0;
    return double(next_rand(&seed) & RAND_MAX) / (double(RAND_MAX) + 1);
}

vec3 random_vec(double min, double max) {
    return vec3(random_double(min, max), random_double(min, max),
                random_double(min, max));
}
