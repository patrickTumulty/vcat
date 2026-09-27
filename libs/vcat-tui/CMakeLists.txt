
cmake_minimum_required(VERSION 3.10)

project(vcat-tui)

find_package(PkgConfig REQUIRED)

find_package(Curses REQUIRED)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tui_session.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CURSES_INCLUDE_DIRS}
    PUBLIC
        src/include
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${GSTREAMER_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${CURSES_LIBRARIES}
    PUBLIC
        spdlog::spdlog
        vcat-common
)
