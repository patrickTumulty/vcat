cmake_minimum_required(VERSION 3.20)

project(vcat LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SPDLOG_COMPILE_LEVEL SPDLOG_LEVEL_DEBUG)

execute_process(
    COMMAND git describe --tag --always
    OUTPUT_VARIABLE VCAT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "vcat version: ${VCAT_VERSION}")

add_compile_options(
    -Wno-unused-variable
    -Wno-unused-parameter
)

add_compile_definitions(
    -DVCAT_VERSION="${VCAT_VERSION}"
)

find_package(PkgConfig REQUIRED)

# CTest defines the BUILD_TESTING option, which each lib checks before adding
# its tests. A cross build compiles for a machine that cannot run them, so it
# turns them off instead of paying to build Google Test for nothing.
# enable_testing() is called either way, so a `ctest --test-dir` on a build
# with no tests stays inside the build tree instead of dropping a Testing/ dir
# in the source.
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
add_subdirectory(libs/vcat-common2)
add_subdirectory(libs/vcat-common)
add_subdirectory(libs/vcat-plugin)
add_subdirectory(libs/vcat-core)
add_subdirectory(libs/vcat-tui)

# Where shippable artifacts land. <build>/bin mirrors an install root: the vcat
# executable goes directly in it and the plugin modules land in bin/plugins,
# where the plugin manager scans for them at runtime (<cwd>/plugins). Static
# archives and test binaries stay in the build tree, and dev-only targets (the
# template plugin) keep their default output rather than reaching bin/.
set(VCAT_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(VCAT_PLUGIN_DIR "${VCAT_BIN_DIR}/plugins")

add_subdirectory(plugins/template-video-source)
add_subdirectory(plugins/test-video-source)
add_subdirectory(plugins/udp-video-source)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ascii_tui_renderer.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${VCAT_BIN_DIR}"
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
