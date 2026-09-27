cmake_minimum_required(VERSION 3.20)

project(vcat LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPDLOG_COMPILE_LEVEL SPDLOG_LEVEL_DEBUG)

add_compile_options(
    -Wno-unused-variable
    -Wno-unused-parameter
)

find_package(PkgConfig REQUIRED)

set(SPDLOG_FMT_EXTERNAL OFF)
add_subdirectory(extern/spdlog)
add_subdirectory(libs/vcat-common)
add_subdirectory(libs/vcat-core)
add_subdirectory(libs/vcat-tui)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    vcat-common
    vcat-core
    vcat-tui
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Point clangd at this build's compile_commands.json from the repository
# root. The last configured preset wins; re-run `cmake --preset <name>`
# to update.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    file(REMOVE "${CMAKE_SOURCE_DIR}/compile_commands.json")
    file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json"
                     "${CMAKE_SOURCE_DIR}/compile_commands.json"
                     SYMBOLIC)
endif()
