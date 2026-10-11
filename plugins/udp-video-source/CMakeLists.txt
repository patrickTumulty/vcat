
cmake_minimum_required(VERSION 3.10)

project(udp-video-source LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/udp_video_source.c
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-framework
)
