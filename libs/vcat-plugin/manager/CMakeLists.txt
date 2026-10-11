
cmake_minimum_required(VERSION 3.10)

project(vcat-plugin-manager LANGUAGES C )

find_package(PkgConfig REQUIRED)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcat_plugin_manager.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-api
    PRIVATE
        PkgConfig::GSTREAMER
)


