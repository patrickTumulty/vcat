
cmake_minimum_required(VERSION 3.10)

project(extern)

add_library(munit STATIC munit/munit.c)

target_include_directories(munit PUBLIC munit)

add_subdirectory(crox)

# crox sets POSITION_INDEPENDENT_CODE on the INTERFACE target that aggregates its
# components, which never reaches the static libraries it pulls in. Plugins link
# those statics into shared objects, so enable PIC on the concrete targets here,
# where they are visible. Without this the plugin link fails with R_X86_64_PC32
# relocations.
set_target_properties(crox-data-structures crox-logging PROPERTIES POSITION_INDEPENDENT_CODE ON)
