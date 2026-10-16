
cmake_minimum_required(VERSION 3.10)

project(extern)

add_library(munit STATIC munit/munit.c)

target_include_directories(munit PUBLIC munit)

add_subdirectory(crox)
