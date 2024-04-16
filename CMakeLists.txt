cmake_minimum_required(VERSION 3.20)

project(rt CXX)

include(FetchContent)

find_package(PNG REQUIRED)

add_executable(rt src/main.cc)

option(TRACY_ENABLE "" OFF)
option(TRACY_ON_DEMAND "" ON)
option(TRACY_NO_BROADCAST "" ON)
option(TRACY_ONLY_LOCALHOST "" ON)
option(TRACY_NO_EXIT "" ON)
option(ASAN_ENABLE "" OFF)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

target_link_libraries(rt PRIVATE TracyClient "${PNG_LIBRARY}")

target_include_directories(rt PRIVATE "${PNG_INCLUDE_DIR}")

target_include_directories(rt PRIVATE src vendor)

target_link_options(rt PRIVATE -lpthread)

set_property(TARGET rt PROPERTY CXX_STANDARD 20)

target_compile_options(rt PRIVATE 
    -fsingle-precision-constant 
    -fdiagnostics-color
    -fdiagnostics-show-template-tree
    -fconcepts-diagnostics-depth=4
)


if (ASAN_ENABLE)
    target_compile_options(rt PRIVATE -fsanitize=address)
    target_link_options(rt PRIVATE -fsanitize=address)
    target_compile_options(rt PRIVATE -fsanitize=undefined)
    target_link_options(rt PRIVATE -fsanitize=undefined)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    message(STATUS "Using debug build. OpenMP is disabled.")
elseif(TRACY_ENABLE)
    message(STATUS "Using tracing build. OpenMP is disabled to avoid big memory consumption.")
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
            #-freciprocal-math
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
