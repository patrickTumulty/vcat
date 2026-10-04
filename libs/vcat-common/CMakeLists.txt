
cmake_minimum_required(VERSION 3.20)

project(vcat-common)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ascii_frame_mailbox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/logging.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cpp
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# The logging facade hides spdlog from the other libs, so it is linked privately.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog
)

# if(BUILD_TESTING)
#     include(GoogleTest)
#
#     add_executable(vcat-common-tests)
#
#     target_sources(vcat-common-tests
#         PRIVATE
#             ${CMAKE_CURRENT_SOURCE_DIR}/test/imatrix_test.cpp
#             ${CMAKE_CURRENT_SOURCE_DIR}/test/greedy_matrix_test.cpp
#     )
#
#     target_include_directories(vcat-common-tests
#         PRIVATE
#             ${CMAKE_CURRENT_SOURCE_DIR}/src
#             ${CMAKE_CURRENT_SOURCE_DIR}/test
#     )
#
#     target_link_libraries(vcat-common-tests
#         PRIVATE
#             GTest::gtest_main
#             ${PROJECT_NAME}
#             spdlog::spdlog
#     )
#
#     gtest_discover_tests(vcat-common-tests)
# endif()
