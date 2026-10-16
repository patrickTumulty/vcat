
cmake_minimum_required(VERSION 3.10)

project(extern)

# munit backs crox's tests, which a cross build leaves out, so skip it there too.
if(BUILD_TESTING)
    add_library(munit STATIC munit/munit.c)

    target_include_directories(munit PUBLIC munit)
endif()

add_subdirectory(crox)
