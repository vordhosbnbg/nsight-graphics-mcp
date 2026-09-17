cmake_minimum_required(VERSION 3.25)
project(NsightGraphicsMCP VERSION 0.0.1 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Only Linux builds are currently supported.")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset linux-gcc-debug")
endif()

option(NGM_WARNINGS_AS_ERRORS "Treat first-party warnings as errors" ON)
option(NGM_BUILD_CHECKS "Build CPU-only first-party checks" ON)

function(ngm_first_party target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(NGM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

include(cmake/Dependencies.cmake)
ngm_add_dependencies()

configure_file(cmake/Version.hpp.in generated/ngm/BuildVersion.hpp @ONLY)
add_library(ngm_core STATIC src/core/Version.cpp src/core/Bootstrap.cpp)
target_include_directories(ngm_core PUBLIC include PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
ngm_first_party(ngm_core)

add_executable(nsight-graphics-mcp src/server/Main.cpp)
target_link_libraries(nsight-graphics-mcp PRIVATE ngm_core fastmcpp_core)
ngm_first_party(nsight-graphics-mcp)

add_executable(ngm-vulkan-fixture src/fixture/Main.cpp)
target_link_libraries(ngm-vulkan-fixture PRIVATE ngm_core volk)
ngm_first_party(ngm-vulkan-fixture)

include(cmake/Shaders.cmake)
ngm_compile_shader(
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/build.vert"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders/build.vert.spv")
add_custom_target(ngm_shaders ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/shaders/build.vert.spv")
add_dependencies(ngm-vulkan-fixture ngm_shaders)

if(NGM_BUILD_CHECKS)
    enable_testing()
    include(cmake/Checks.cmake)
    add_subdirectory(tests)
endif()
