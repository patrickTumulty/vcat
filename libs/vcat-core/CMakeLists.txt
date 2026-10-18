
cmake_minimum_required(VERSION 3.20)

project(vcat-core LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

add_library(${PROJECT_NAME} STATIC)

if(CMAKE_CROSSCOMPILING)
    set(GST_PLUGIN_DIR /usr/lib/aarch64-linux-gnu/gstreamer-1.0)
else()
    set(GST_PLUGIN_DIR /usr/lib/x86_64-linux-gnu/gstreamer-1.0)
endif()

message(STATUS "GStreamer plugin dir: ${GST_PLUGIN_DIR}")

set(GST_REQUIRED_PLUGINS
    coreelements       # capsfilter (and filesrc/fakesink/queue/...)
    app                # appsink
    videoconvertscale  # videoconvert
    videotestsrc       # videotestsrc (test source)
    udp                # udpsrc
    mpegtsdemux        # tsdemux
    videoparsersbad    # h264parse, h265parse
    playback           # decodebin
    typefindfunctions  # typefind; decodebin cannot work without it
    nvcodec            # nvh264dec/nvh265dec: hardware decode on NVIDIA targets
    libav              # avdec_h264/avdec_h265: software decode fallback
)

# Fail at configure time with a clear message if an expected plugin is
# missing, instead of a cryptic copy error during the build.
set(GST_REQUIRED_PLUGIN_LIBS)
foreach(plugin IN LISTS GST_REQUIRED_PLUGINS)
    set(PLUGIN_LIB
        "${GST_PLUGIN_DIR}/libgst${plugin}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    )
    if(NOT EXISTS "${PLUGIN_LIB}")
        message(FATAL_ERROR
            "Required GStreamer plugin not found: ${PLUGIN_LIB}
            (is the matching gst-plugins package installed?)"
        )
    endif()
    list(APPEND GST_REQUIRED_PLUGIN_LIBS "${PLUGIN_LIB}")
endforeach()

# The executable loads plugins from <exe dir>/lib/gstreamer-1.0 (see
# gstreamer_init.c), the same "lib" the $ORIGIN/lib RPATH uses.
set(VCAT_GST_PLUGIN_DIR "${VCAT_BIN_DIR}/lib/gstreamer-1.0")

add_custom_target(copy-gst-plugins ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VCAT_GST_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GST_REQUIRED_PLUGIN_LIBS}
        "${VCAT_GST_PLUGIN_DIR}"
    COMMENT "Copying required GStreamer plugins"
    VERBATIM
)

# Bundled GStreamer plugins (see libs/vcat-core/CMakeLists.txt). Static
# libraries don't propagate custom-target dependencies, so tie the copy
# step to the executable explicitly.
add_dependencies(${PROJECT_NAME} copy-gst-plugins)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gstreamer_init.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_pipeline.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video2ascii_converter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_manager.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vcat-common
        vcat-plugin-api
        vcat-plugin-manager
    PRIVATE
        PkgConfig::GSTREAMER
)
