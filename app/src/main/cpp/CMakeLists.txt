cmake_minimum_required(VERSION 3.22.1)
project(bb C)

# Force optimized C flags even in debug builds
set(CMAKE_C_FLAGS_DEBUG   "-O3 -g0 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG")

# Base optimizations: unroll, no strict aliasing on the vector code
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -funroll-loops -fno-strict-aliasing -fomit-frame-pointer")

# ARM-specific: enable SHA-256 crypto extensions when the target supports them
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crypto")
endif()

add_library(bb SHARED bb.c)
find_library(log-lib log)
target_link_libraries(bb ${log-lib})