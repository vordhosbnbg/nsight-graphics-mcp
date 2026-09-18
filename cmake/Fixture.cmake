include("${CMAKE_CURRENT_LIST_DIR}/Shaders.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/NsightSdk.cmake")

function(ngm_configure_fixture target)
    ngm_configure_fixture_sdk(${target})
    # XCB is the selected desktop/runtime exception. Vulkan and the shader
    # compiler continue to come from the pinned source dependencies.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NGM_XCB REQUIRED IMPORTED_TARGET xcb>=1.13)
    target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/fixture/Fixture.cpp"
        "${PROJECT_SOURCE_DIR}/src/fixture/Compute.cpp")
    target_link_libraries(${target} PRIVATE PkgConfig::NGM_XCB nlohmann_json::nlohmann_json)
    target_compile_definitions(${target} PRIVATE VK_USE_PLATFORM_XCB_KHR)

    set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/fixture")
    target_compile_definitions(${target} PRIVATE NGM_FIXTURE_SHADER_DIR="${shader_dir}")

    # This identity belongs to the running C++ executable, independently of a
    # --shader-dir override. Regenerate at every build, but only touch the header
    # when its contents change so ordinary no-op builds remain incremental.
    set(identity_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/fixture")
    set(identity_header "${identity_dir}/FixtureBuildIdentity.hpp")
    target_include_directories(${target} PRIVATE "${identity_dir}")
    set_property(TARGET ${target} PROPERTY EXPORT_COMPILE_COMMANDS ON)
    set_property(TARGET ngm_core PROPERTY EXPORT_COMPILE_COMMANDS ON)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
    add_custom_target(ngm_fixture_identity
        COMMAND "${CMAKE_COMMAND}"
            "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
            "-DBUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DOUTPUT_HEADER=${identity_header}"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            "-DHOST_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DHOST_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
            "-DHOST_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
            "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DHOST_COMPILER_FLAGS=${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type_upper}}"
            "-DSDK_MANIFEST=${NGM_FIXTURE_SDK_MANIFEST}"
            -P "${PROJECT_SOURCE_DIR}/cmake/FixtureBuildIdentity.cmake"
        BYPRODUCTS "${identity_header}" "${identity_dir}/identity.json"
        VERBATIM)
    add_dependencies(${target} ngm_fixture_identity)

    set(outputs)
    foreach(name IN ITEMS scene.vert scene.frag shader-error.frag indirect.vert bindless.frag post.vert post.frag
            compute-reference.comp compute-index-error.comp compute-arithmetic-error.comp)
        set(source "${PROJECT_SOURCE_DIR}/shaders/fixture/${name}")
        set(spirv "${shader_dir}/${name}.spv")
        ngm_compile_shader("${source}" "${spirv}")
        add_custom_command(OUTPUT "${shader_dir}/${name}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${shader_dir}/${name}"
            DEPENDS "${source}" VERBATIM)
        list(APPEND outputs "${spirv}" "${shader_dir}/${name}")
    endforeach()
    add_custom_target(ngm_fixture_shaders DEPENDS ${outputs})
    add_dependencies(${target} ngm_fixture_shaders)
    set(NGM_FIXTURE_SHADER_OUTPUT_DIR "${shader_dir}" PARENT_SCOPE)
    set(NGM_FIXTURE_SHADER_OUTPUTS "${outputs}" PARENT_SCOPE)
endfunction()
