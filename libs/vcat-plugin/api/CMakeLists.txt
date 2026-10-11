
cmake_minimum_required(VERSION 3.10)

project(vcat-plugin-api LANGUAGES C)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcat_plugin_api.c

)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-framework
        PkgConfig::GSTREAMER
)

