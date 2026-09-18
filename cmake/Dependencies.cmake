# All acquisition happens through git submodule update, never through CMake.
function(ngm_require_source name marker)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/${name}/${marker}")
        message(FATAL_ERROR
            "Missing dependency source: external/${name}/${marker}\n"
            "Run from the repository root: git submodule update --init --recursive\n"
            "Then configure again. Installed packages are not a substitute.")
    endif()
endfunction()

function(ngm_add_dependencies)
    # Check the entire graph before configuring any dependency.
    ngm_require_source(fastmcpp CMakeLists.txt)
    ngm_require_source(json CMakeLists.txt)
    ngm_require_source(cpp-httplib httplib.h)
    ngm_require_source(glslang CMakeLists.txt)
    ngm_require_source(vulkan-headers CMakeLists.txt)
    ngm_require_source(volk CMakeLists.txt)
    ngm_require_source(lodepng lodepng.cpp)
    ngm_require_source(lodepng lodepng.h)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Vendored libraries are static" FORCE)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Never download while configuring" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Never update while configuring" FORCE)

    # Compile only the pinned PNG codec, without disk I/O, metadata expansion,
    # tests, tools or transitive discovery. Its zlib implementation is included.
    add_library(ngm_lodepng STATIC "${PROJECT_SOURCE_DIR}/external/lodepng/lodepng.cpp")
    target_include_directories(ngm_lodepng SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/external/lodepng")
    target_compile_definitions(ngm_lodepng PUBLIC LODEPNG_NO_COMPILE_DISK
        LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS PRIVATE LODEPNG_MAX_ALLOC=134217728)

    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_CI OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/json SYSTEM EXCLUDE_FROM_ALL)

    # Upstream supports header-only consumption. Avoid its optional system TLS
    # and compression discovery and propagate one consistent set of headers.
    find_package(Threads REQUIRED)
    add_library(ngm_httplib INTERFACE)
    target_include_directories(ngm_httplib SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/external/cpp-httplib")
    target_link_libraries(ngm_httplib INTERFACE Threads::Threads)
    add_library(httplib::httplib ALIAS ngm_httplib)

    foreach(option IN ITEMS FASTMCPP_BUILD_TESTS FASTMCPP_BUILD_EXAMPLES FASTMCPP_BUILD_CLI
            FASTMCPP_ENABLE_POST_STREAMING FASTMCPP_FETCH_CURL FASTMCPP_ENABLE_SAMPLING_HTTP_HANDLERS
            FASTMCPP_ENABLE_OPENSSL FASTMCPP_ENABLE_WINHTTP FASTMCPP_ENABLE_CURL_TRANSPORT)
        set(${option} OFF CACHE BOOL "Disabled in the local stdio build" FORCE)
    endforeach()
    add_subdirectory(external/fastmcpp SYSTEM EXCLUDE_FROM_ALL)

    set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/vulkan-headers SYSTEM EXCLUDE_FROM_ALL)
    # Do not let volk discover installed headers or a Vulkan SDK.
    set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "Use the pinned Vulkan headers" FORCE)
    set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
    set(VOLK_HEADERS_ONLY OFF CACHE BOOL "Build the static loader" FORCE)
    add_subdirectory(external/volk SYSTEM EXCLUDE_FROM_ALL)
    target_link_libraries(volk PUBLIC Vulkan::Headers)

    # GLSL diagnostics need the SPIR-V generator, but not HLSL, the optimizer,
    # remapper, JS, gtest, or installed SPIRV-Tools. glslang includes the generator
    # headers it needs in its own pinned tree; no additional sources are fetched.
    foreach(option IN ITEMS BUILD_EXTERNAL BUILD_WERROR GLSLANG_TESTS GLSLANG_ENABLE_INSTALL
            ENABLE_HLSL ENABLE_OPT ENABLE_GLSLANG_JS ENABLE_SPVREMAPPER
            ALLOW_EXTERNAL_SPIRV_TOOLS ALLOW_EXTERNAL_GTEST)
        set(${option} OFF CACHE BOOL "Not required for GLSL diagnostic builds" FORCE)
    endforeach()
    set(ENABLE_SPIRV ON CACHE BOOL "Build the SPIR-V generator" FORCE)
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "Build our shader compiler" FORCE)
    add_subdirectory(external/glslang SYSTEM EXCLUDE_FROM_ALL)

    foreach(target IN ITEMS fastmcpp_core volk glslang glslang-default-resource-limits ngm_lodepng)
        get_target_property(kind ${target} TYPE)
        if(NOT kind STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR "Vendored target ${target} must be a static library, got ${kind}")
        endif()
    endforeach()
endfunction()
