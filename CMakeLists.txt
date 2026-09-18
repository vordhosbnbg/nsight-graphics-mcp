cmake_minimum_required(VERSION 3.25)
project(NsightGraphicsMCP VERSION 0.2.12 LANGUAGES C CXX)

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
add_library(ngm_core STATIC src/core/Version.cpp src/core/Bootstrap.cpp src/core/Hash.cpp src/core/File.cpp
    src/core/Image.cpp src/core/Process.cpp)
target_include_directories(ngm_core PUBLIC include PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ngm_core PUBLIC Threads::Threads PRIVATE ngm_lodepng)
ngm_first_party(ngm_core)
include(cmake/Jobs.cmake)
ngm_add_jobs()
include(cmake/Nsight.cmake)
ngm_add_nsight()
include(cmake/Artifacts.cmake)
ngm_add_artifacts()
include(cmake/Capture.cmake)
ngm_add_capture()
include(cmake/Inspection.cmake)
ngm_add_inspection()
include(cmake/ResourceWorker.cmake)
ngm_add_resource_workers()

add_executable(nsight-graphics-mcp src/server/Main.cpp)
target_link_libraries(nsight-graphics-mcp PRIVATE ngm_core fastmcpp_core)
ngm_first_party(nsight-graphics-mcp)
include(cmake/Mcp.cmake)
ngm_add_mcp()

add_executable(ngm-vulkan-fixture src/fixture/Main.cpp)
target_link_libraries(ngm-vulkan-fixture PRIVATE ngm_core volk)
ngm_first_party(ngm-vulkan-fixture)
include(cmake/Fixture.cmake)
ngm_configure_fixture(ngm-vulkan-fixture)
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
add_custom_target(ngm_fixture_provenance
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DSHADER_DIR=${NGM_FIXTURE_SHADER_OUTPUT_DIR}"
        "-DCOMPILER=$<TARGET_FILE:glslang-standalone>"
        "-DPROJECT_VERSION=${PROJECT_VERSION}"
        "-DHOST_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DHOST_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
        "-DHOST_COMPILER_FLAGS=${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type_upper}}"
        "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
        -P "${PROJECT_SOURCE_DIR}/cmake/FixtureProvenance.cmake"
    DEPENDS ngm_fixture_shaders glslang-standalone
    BYPRODUCTS "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/provenance.json"
    VERBATIM)
add_dependencies(ngm-vulkan-fixture ngm_fixture_provenance)

add_library(ngm_experiments STATIC src/experiment/Experiment.cpp)
target_link_libraries(ngm_experiments PUBLIC ngm_core nlohmann_json::nlohmann_json)
ngm_first_party(ngm_experiments)
add_executable(ngm-experiment src/experiment/Main.cpp)
target_link_libraries(ngm-experiment PRIVATE ngm_experiments)
ngm_first_party(ngm-experiment)

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
