ngm_add_check(ngm_dependency_check SOURCES DependencyCheck.cpp LIBRARIES ngm_core fastmcpp_core volk)
ngm_add_check(ngm_shader_check SOURCES ShaderCheck.cpp
    ARGS "${CMAKE_BINARY_DIR}/shaders/build.vert.spv" DEPENDS ngm_shaders)
ngm_add_check(ngm_hash_check SOURCES HashCheck.cpp LIBRARIES ngm_core)
ngm_add_check(ngm_image_check SOURCES ImageCheck.cpp LIBRARIES ngm_core
    ARGS "${CMAKE_CURRENT_BINARY_DIR}/image-check")
add_executable(ngm_process_standin ProcessStandin.cpp)
ngm_first_party(ngm_process_standin)
ngm_add_check(ngm_process_check SOURCES ProcessCheck.cpp LIBRARIES ngm_core
    ARGS "$<TARGET_FILE:ngm_process_standin>" DEPENDS ngm_process_standin)
ngm_add_mcp_checks()
ngm_add_check(ngm_fixture_contract_check SOURCES FixtureContractCheck.cpp
    LIBRARIES ngm_core nlohmann_json::nlohmann_json
    ARGS "$<TARGET_FILE:ngm-vulkan-fixture>" "${NGM_FIXTURE_SHADER_OUTPUT_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/fixture-contract"
    DEPENDS ngm-vulkan-fixture)
add_executable(ngm_experiment_standin ExperimentStandin.cpp)
target_link_libraries(ngm_experiment_standin PRIVATE ngm_core nlohmann_json::nlohmann_json Threads::Threads)
ngm_first_party(ngm_experiment_standin)
ngm_add_check(ngm_experiment_check SOURCES ExperimentCheck.cpp LIBRARIES ngm_experiments
    ARGS "$<TARGET_FILE:ngm_experiment_standin>" "${CMAKE_CURRENT_BINARY_DIR}/experiment-check"
    DEPENDS ngm_experiment_standin)
ngm_add_hardware_check(ngm_fixture_integration SOURCES FixtureIntegration.cpp LIBRARIES ngm_experiments
    ARGS "$<TARGET_FILE:ngm-vulkan-fixture>" "${PROJECT_SOURCE_DIR}/artifacts/fixture-validation")
add_dependencies(ngm_fixture_integration ngm-vulkan-fixture)
add_custom_target(ngm_fixture_validation_run
    COMMAND "$<TARGET_FILE:ngm_fixture_integration>" "$<TARGET_FILE:ngm-vulkan-fixture>"
        "${PROJECT_SOURCE_DIR}/artifacts/fixture-validation" --validation
    DEPENDS ngm_fixture_integration USES_TERMINAL VERBATIM)

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
        "-DBINARIES=$<TARGET_FILE:nsight-graphics-mcp>|$<TARGET_FILE:ngm-vulkan-fixture>|$<TARGET_FILE:ngm-experiment>|$<TARGET_FILE:ngm_dependency_check>|$<TARGET_FILE:glslang-standalone>"
        "-DDESKTOP_BINARY=$<TARGET_FILE:ngm-vulkan-fixture>"
        "-DARCHIVES=$<TARGET_FILE:fastmcpp_core>|$<TARGET_FILE:volk>|$<TARGET_FILE:glslang>|$<TARGET_FILE:glslang-default-resource-limits>"
        -P "${PROJECT_SOURCE_DIR}/cmake/AuditLinkage.cmake"
    DEPENDS nsight-graphics-mcp ngm-vulkan-fixture ngm-experiment ngm_dependency_check glslang-standalone)

ngm_finalize_checks()
