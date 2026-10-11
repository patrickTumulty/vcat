
cmake_minimum_required(VERSION 3.10)

project(template-video-source LANGUAGES C)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/template_video_source.c
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-framework
)
