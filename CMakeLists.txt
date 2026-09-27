cmake_minimum_required(VERSION 3.20)

project(tplay LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPDLOG_COMPILE_LEVEL SPDLOG_LEVEL_DEBUG)

add_compile_options(
    -Wno-unused-variable
    -Wno-unused-parameter
)

find_package(PkgConfig REQUIRED)

find_package(Curses REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

set(SPDLOG_FMT_EXTERNAL OFF)
add_subdirectory(extern/spdlog)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/logging.cpp
    src/utils.cpp
    src/tui_session.cpp
    src/video_pipeline.cpp
    src/video2ascii_converter.cpp
    src/videosrc_udp.cpp
    src/videosrc_test.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${CURSES_LIBRARIES}
    spdlog::spdlog
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

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
