cmake_minimum_required(VERSION 3.20)

project(vcat LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# POSIX APIs (clock_nanosleep, sigwait-family, setenv) are hidden behind
# feature macros under -std=c11; _DEFAULT_SOURCE exposes them, the same way
# zlog's own build does.
add_compile_definitions(_DEFAULT_SOURCE)

execute_process(
    COMMAND git describe --tag --always
    OUTPUT_VARIABLE VCAT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "vcat version: ${VCAT_VERSION}")

add_compile_options(
    -Wno-unused-parameter
)

add_compile_definitions(
    -DVCAT_VERSION="${VCAT_VERSION}"
)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

include(CTest)
enable_testing()
if(CMAKE_CROSSCOMPILING)
    set(BUILD_TESTS OFF)
    set(CROX_BUILD_TESTS OFF)
else()
    set(BUILD_TESTS ON)
    set(CROX_BUILD_TESTS ON)
endif()

add_subdirectory(extern)
add_subdirectory(extern/zlog)
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

# The logging configuration ships next to the executable, where
# vcatLogInit() looks for it (see libs/vcat-common/src/logging.c).
file(MAKE_DIRECTORY "${VCAT_BIN_DIR}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    configure_file(
        "${CMAKE_SOURCE_DIR}/vcat.zlog.conf"
        "${VCAT_BIN_DIR}/vcat.zlog.conf"
        COPYONLY
    )
else()
    configure_file(
        "${CMAKE_SOURCE_DIR}/vcat.zlog.dev.conf"
        "${VCAT_BIN_DIR}/vcat.zlog.conf"
        COPYONLY
    )
endif()

add_subdirectory(plugins/template-video-source)
add_subdirectory(plugins/test-video-source)
add_subdirectory(plugins/udp-video-source)

add_executable(${PROJECT_NAME}
    src/main.c
    src/ascii_tui_renderer.c
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

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
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
