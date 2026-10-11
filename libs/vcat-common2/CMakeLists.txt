
cmake_minimum_required(VERSION 3.20)

project(vcat-common2 LANGUAGES C )

add_library(${PROJECT_NAME} STATIC)

# Linked into shared plugin libraries, so its objects must be
# position-independent or the plugin link fails (R_X86_64_PC32 relocations).
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/logging_facade.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)


