
# Build file for the template video source plugin.
#
# Rename the project/target to your plugin's name, e.g. project(my-source).
# The target name becomes "lib<name>.so" after building.
cmake_minimum_required(VERSION 3.10)

project(template-video-source LANGUAGES C)

add_library(${PROJECT_NAME} SHARED)

# vcat does not enforce where your build puts the plugin .so -- this is your
# build tree, organize it how you like. What matters at runtime is only where
# you *install* it: copy the built .so into ~/.config/vcat/plugins (the
# recommended location, it survives vcat installs and upgrades) or into the
# `plugins` directory that sits next to the vcat executable.
#
# For reference: the in-tree plugins set LIBRARY_OUTPUT_DIRECTORY to
# ${VCAT_PLUGIN_DIR} (<build>/bin/plugins) so they land where a vcat build
# looks for plugins out of the box. That convenience is only meaningful when
# building inside vcat's own tree (VCAT_PLUGIN_DIR is defined by the root
# CMakeLists.txt), and even there it is optional.

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/template_video_source.c
)

# vcat-plugin-framework is a target defined inside vcat's own CMake tree. It
# only exists if your build tree includes vcat:
#   - If your project pulls vcat in as a git submodule, your top-level
#     CMakeLists.txt must add_subdirectory() it before adding this plugin
#     (see "Writing a Plugin" in the README).
#   - If you build this plugin from inside the vcat tree, the root
#     CMakeLists.txt already defines the target.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-plugin-framework
)
