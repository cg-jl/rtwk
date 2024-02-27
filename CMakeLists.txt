cmake_minimum_required(VERSION 3.20)

project(rt CXX)

find_package(PNG REQUIRED)

add_executable(rt src/main.cc)

target_include_directories(rt PRIVATE "${PNG_INCLUDE_DIR}")
target_link_libraries(rt ${PNG_LIBRARY})

target_include_directories(rt PRIVATE src vendor)

target_link_options(rt PRIVATE -lpthread)

set_property(TARGET rt PROPERTY CXX_STANDARD 20)

target_compile_options(rt PRIVATE -fsingle-precision-constant -fdiagnostics-color)


if (FALSE)
    target_compile_options(rt PRIVATE -fsanitize=address)
    target_link_options(rt PRIVATE -fsanitize=address)
endif ()

if (FALSE)
    target_compile_options(rt PRIVATE -fsanitize=undefined)
    target_link_options(rt PRIVATE -fsanitize=undefined)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    message(STATUS "Using debug build. OpenMP is disabled.")
else ()
    target_compile_options(rt PRIVATE

            -march=native
            -mavx2
            -fno-exceptions

            # NOTE: all of these have been tested to not modify
            # the final image. Mileage may vary!
            -fwhole-program
            -fno-math-errno
            -fno-trapping-math
            -fno-signed-zeros
            -fno-rounding-math
            -freciprocal-math
            -fassociative-math
            -ffinite-math-only
            -fallow-store-data-races
            #-ffast-math

            # This shouldn't have a big impact, since it's only one file, and nothing
            # *should* be exported
            -fno-semantic-interposition
    )
    target_compile_options(rt PRIVATE -fopenmp)
    target_link_options(rt PRIVATE -fopenmp)
endif ()
