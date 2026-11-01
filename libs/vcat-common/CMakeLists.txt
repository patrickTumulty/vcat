
cmake_minimum_required(VERSION 3.20)

project(vcat-common LANGUAGES C)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ascii_frame_mailbox.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/color_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/file_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/logging.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        crox
        m
    PRIVATE
        Threads::Threads
        zlog_s
)

if(BUILD_TESTS)
    add_executable(${PROJECT_NAME}-tests)

    target_sources(${PROJECT_NAME}-tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/test/main.c
    )

    target_include_directories(${PROJECT_NAME}-tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    target_link_libraries(${PROJECT_NAME}-tests
        PRIVATE
            ${PROJECT_NAME}
            munit
            Threads::Threads
    )

    add_test(NAME ${PROJECT_NAME}-tests COMMAND ${PROJECT_NAME}-tests)
endif()
