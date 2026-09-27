
cmake_minimum_required(VERSION 3.20)

project(vcat-common)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
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
