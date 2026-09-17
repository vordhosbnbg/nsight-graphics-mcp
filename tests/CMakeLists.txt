ngm_add_check(ngm_dependency_check SOURCES DependencyCheck.cpp LIBRARIES ngm_core fastmcpp_core volk)
ngm_add_check(ngm_shader_check SOURCES ShaderCheck.cpp
    ARGS "${CMAKE_BINARY_DIR}/shaders/build.vert.spv" DEPENDS ngm_shaders)

ngm_register_check(ngm_cli_check
    COMMAND "${CMAKE_COMMAND}"
        "-DSERVER=$<TARGET_FILE:nsight-graphics-mcp>"
        "-DFIXTURE=$<TARGET_FILE:ngm-vulkan-fixture>"
        "-DEXPECTED_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCLI.cmake"
    DEPENDS nsight-graphics-mcp ngm-vulkan-fixture)

ngm_register_check(ngm_shader_failure_check
    COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=$<TARGET_FILE:glslang-standalone>"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}" "-DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/shader failure"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckShaderFailure.cmake"
    DEPENDS glslang-standalone)

ngm_register_check(ngm_missing_source_check
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/missing-source"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckMissingSource.cmake")

ngm_register_check(ngm_check_isolation
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/check-isolation"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckIsolation.cmake")

find_program(NGM_READELF readelf REQUIRED)
ngm_register_check(ngm_linkage_check
    COMMAND "${CMAKE_COMMAND}" "-DREADELF=${NGM_READELF}" "-DAR=${CMAKE_AR}"
        "-DBINARIES=$<TARGET_FILE:nsight-graphics-mcp>|$<TARGET_FILE:ngm-vulkan-fixture>|$<TARGET_FILE:ngm_dependency_check>|$<TARGET_FILE:glslang-standalone>"
        "-DARCHIVES=$<TARGET_FILE:fastmcpp_core>|$<TARGET_FILE:volk>|$<TARGET_FILE:glslang>|$<TARGET_FILE:glslang-default-resource-limits>"
        -P "${PROJECT_SOURCE_DIR}/cmake/AuditLinkage.cmake"
    DEPENDS nsight-graphics-mcp ngm-vulkan-fixture ngm_dependency_check glslang-standalone)

ngm_finalize_checks()
