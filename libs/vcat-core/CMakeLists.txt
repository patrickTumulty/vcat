
cmake_minimum_required(VERSION 3.20)

project(vcat-core)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gstreamer_init.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video2ascii_converter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/videosrc_udp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/videosrc_test.cpp
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# GStreamer types show up in this lib's headers, so consumers need its include dirs too.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        PkgConfig::GSTREAMER
        vcat-common
)
