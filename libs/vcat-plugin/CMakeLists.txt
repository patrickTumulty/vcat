
cmake_minimum_required(VERSION 3.20)

project(vcat-plugin LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

add_subdirectory(plugin-framework)
add_subdirectory(api)
add_subdirectory(manager)
