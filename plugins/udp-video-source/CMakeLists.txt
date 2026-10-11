
cmake_minimum_required(VERSION 3.10)

project(udp-video-source LANGUAGES C)

add_library(${PROJECT_NAME} SHARED)

# Land the plugin .so in <build>/bin/plugins (VCAT_PLUGIN_DIR), where the plugin
# manager scans for plugins at runtime (<cwd>/plugins relative to the binary).
set_target_properties(${PROJECT_NAME} 
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${VCAT_PLUGIN_DIR}"
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/udp_video_source.c
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-framework
)
