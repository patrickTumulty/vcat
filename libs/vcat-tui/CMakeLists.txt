
cmake_minimum_required(VERSION 3.20)

project(vcat-tui)

# Link ncurses statically so the bundle does not have to ship libncurses/libtinfo.
# find_package(Curses) only locates the shared libraries, so look the archives up
# directly. ncurses keeps its terminfo routines in libtinfo, hence both archives.
set(_vcat_tui_saved_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_library(NCURSES_STATIC_LIBRARY NAMES ncurses REQUIRED)
find_library(TINFO_STATIC_LIBRARY NAMES tinfo REQUIRED)
set(CMAKE_FIND_LIBRARY_SUFFIXES "${_vcat_tui_saved_suffixes}")

find_path(NCURSES_INCLUDE_DIR NAMES ncurses.h REQUIRED)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tui_session.cpp
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${NCURSES_INCLUDE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${NCURSES_STATIC_LIBRARY}
        ${TINFO_STATIC_LIBRARY}
    PUBLIC
        vcat-common
)
