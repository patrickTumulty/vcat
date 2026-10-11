cmake_minimum_required(VERSION 3.20)

project(vcat LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_definitions(_DEFAULT_SOURCE)
add_compile_definitions(_FILE_OFFSET_BITS=64)

set(SPDLOG_COMPILE_LEVEL SPDLOG_LEVEL_DEBUG)

add_compile_options(
    -Wno-unused-variable
    -Wno-unused-parameter
)

find_package(PkgConfig REQUIRED)

# CTest defines the BUILD_TESTING option, which each lib checks before adding its tests. A cross
# build compiles for a machine that cannot run them, so it turns them off instead of paying to
# build Google Test for nothing. enable_testing() is called either way, so a `ctest --test-dir` on a
# build with no tests stays inside the build tree instead of dropping a Testing/ dir in the source.
include(CTest)
enable_testing()
if(CMAKE_CROSSCOMPILING)
    set(BUILD_TESTING OFF)
endif()

set(SPDLOG_FMT_EXTERNAL OFF)
add_subdirectory(extern/spdlog)
if(BUILD_TESTING)
    add_subdirectory(extern/googletest)
endif()
add_subdirectory(libs/vcat-common)
add_subdirectory(libs/vcat-plugin)
add_subdirectory(libs/vcat-core)
add_subdirectory(libs/vcat-tui)

add_subdirectory(plugins/template-video-source)
add_subdirectory(plugins/udp-video-source)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ascii_tui_renderer.cpp
)


add_executable(${PROJECT_NAME}alt
    src/main.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    vcat-common
    vcat-core
    vcat-tui
)

target_link_libraries(${PROJECT_NAME}alt PRIVATE
    vcat-plugin-manager
)


target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Point clangd at this build's compile_commands.json from the repository
# root. The last configured preset wins; re-run `cmake --preset <name>`
# to update.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    file(REMOVE "${CMAKE_SOURCE_DIR}/compile_commands.json")
    file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json"
                     "${CMAKE_SOURCE_DIR}/compile_commands.json"
                     SYMBOLIC)
endif()
