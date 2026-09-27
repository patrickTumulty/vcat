
cmake_minimum_required(VERSION 3.10)

project(vcat-core)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video2ascii_converter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/videosrc_udp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/videosrc_test.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        src/
    PUBLIC
        src/include
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${GSTREAMER_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${GSTREAMER_LIBRARIES}
    PUBLIC
        spdlog::spdlog
        vcat-common
)

