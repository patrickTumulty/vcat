
cmake_minimum_required(VERSION 3.10)

project(vcat-plugin-framework LANGUAGES C)

# Shared plugin utility code lives here. The library is consumed by plugin
# shared objects, hence the explicit PIC.
add_library(${PROJECT_NAME} STATIC)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcat_network_endpoint.c
        ${CMAKE_CURRENT_SOURCE_DIR}/vcat_video_source_utils.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        PkgConfig::GSTREAMER
        crox
)
