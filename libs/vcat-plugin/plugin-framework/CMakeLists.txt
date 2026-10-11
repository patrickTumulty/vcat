
cmake_minimum_required(VERSION 3.10)

project(vcat-plugin-framework LANGUAGES C)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    INTERFACE
        PkgConfig::GSTREAMER
)
