ngm_add_check(ngm_dependency_check SOURCES DependencyCheck.cpp LIBRARIES ngm_core fastmcpp_core volk)
ngm_add_check(ngm_shader_check SOURCES ShaderCheck.cpp
    ARGS "${CMAKE_BINARY_DIR}/shaders/build.vert.spv" DEPENDS ngm_shaders)
ngm_add_check(ngm_hash_check SOURCES HashCheck.cpp LIBRARIES ngm_core)
ngm_add_check(ngm_image_check SOURCES ImageCheck.cpp LIBRARIES ngm_core
    ARGS "${CMAKE_CURRENT_BINARY_DIR}/image-check" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/images")
add_executable(ngm_process_standin ProcessStandin.cpp)
ngm_first_party(ngm_process_standin)
ngm_add_check(ngm_process_check SOURCES ProcessCheck.cpp LIBRARIES ngm_core
    ARGS "$<TARGET_FILE:ngm_process_standin>" DEPENDS ngm_process_standin)
ngm_add_jobs_checks()
ngm_add_nsight_checks()
ngm_add_artifact_checks()
ngm_add_capture_checks()
ngm_add_inspection_checks()
ngm_add_mcp_checks()
ngm_add_check(ngm_capture_validation_check SOURCES CaptureValidationCheck.cpp)
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

set(NGM_NSIGHT_ROOT "" CACHE PATH "Explicit Nsight installation for opt-in capture integration")
set(NGM_CAPTURE_ARTIFACT_ROOT "${PROJECT_SOURCE_DIR}/artifacts/nsight-evidence" CACHE PATH
    "Managed evidence store for opt-in capture integration")
set(NGM_CAPTURE_VALIDATION_ROOT "${PROJECT_SOURCE_DIR}/artifacts/capture-validation" CACHE PATH
    "Isolated reports for opt-in capture integration")
ngm_add_hardware_check(ngm_capture_integration SOURCES CaptureIntegration.cpp LIBRARIES ngm_experiments
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "$<TARGET_FILE:ngm-vulkan-fixture>"
        "${NGM_NSIGHT_ROOT}" "${NGM_CAPTURE_ARTIFACT_ROOT}" "${NGM_CAPTURE_VALIDATION_ROOT}")
add_dependencies(ngm_capture_integration nsight-graphics-mcp ngm-vulkan-fixture)

ngm_add_hardware_check(ngm_cpp_capture_integration SOURCES CppCaptureIntegration.cpp LIBRARIES ngm_capture
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "$<TARGET_FILE:ngm-vulkan-fixture>" "${NGM_FIXTURE_SHADER_OUTPUT_DIR}"
        "${NGM_NSIGHT_ROOT}" "${NGM_CAPTURE_ARTIFACT_ROOT}" "${NGM_CAPTURE_VALIDATION_ROOT}"
        "reference" "shader-error")
add_dependencies(ngm_cpp_capture_integration nsight-graphics-mcp ngm-vulkan-fixture)

set(NGM_REPAIR_SHADER_ROOT "" CACHE PATH "Prebuilt edited shader bundle for opt-in repair validation")
set(NGM_REPAIR_RECORD_ROOT "" CACHE PATH "Source patch and build record for opt-in repair validation")
ngm_add_hardware_check(ngm_shader_repair_integration SOURCES ShaderRepairIntegration.cpp
    LIBRARIES ngm_capture ngm_experiments
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "$<TARGET_FILE:ngm-vulkan-fixture>" "${NGM_FIXTURE_SHADER_OUTPUT_DIR}"
        "${NGM_REPAIR_SHADER_ROOT}" "${NGM_REPAIR_RECORD_ROOT}" "${NGM_NSIGHT_ROOT}"
        "${NGM_CAPTURE_ARTIFACT_ROOT}" "${NGM_CAPTURE_VALIDATION_ROOT}")
add_dependencies(ngm_shader_repair_integration nsight-graphics-mcp ngm-vulkan-fixture)

set(NGM_REPAIR_FIXTURE "" CACHE FILEPATH "Rebuilt C++ fixture for opt-in source repair validation")
set(NGM_SOURCE_REPAIR_ORIGINAL_FIXTURE "$<TARGET_FILE:ngm-vulkan-fixture>" CACHE FILEPATH
    "Frozen original fixture matching the source repair record")
set(NGM_SOURCE_REPAIR_SHADERS "${NGM_FIXTURE_SHADER_OUTPUT_DIR}" CACHE PATH
    "Common frozen shader bundle matching the source repair record")
ngm_add_hardware_check(ngm_source_repair_integration SOURCES SourceRepairIntegration.cpp
    LIBRARIES ngm_capture ngm_experiments
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "${NGM_SOURCE_REPAIR_ORIGINAL_FIXTURE}" "${NGM_REPAIR_FIXTURE}"
        "${NGM_SOURCE_REPAIR_SHADERS}" "${NGM_REPAIR_RECORD_ROOT}" "${NGM_NSIGHT_ROOT}"
        "${NGM_CAPTURE_ARTIFACT_ROOT}" "${NGM_CAPTURE_VALIDATION_ROOT}" "combined-reference" "combined-pass-error")
add_dependencies(ngm_source_repair_integration nsight-graphics-mcp ngm-vulkan-fixture)

set(NGM_IMAGE_CASES "" CACHE FILEPATH "Explicit retained-image inputs for opt-in validation")
ngm_add_hardware_check(ngm_retained_image_integration SOURCES RetainedImageIntegration.cpp
    LIBRARIES ngm_core fastmcpp_core
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "${NGM_CAPTURE_ARTIFACT_ROOT}"
        "${NGM_IMAGE_CASES}" "${NGM_CAPTURE_VALIDATION_ROOT}")
add_dependencies(ngm_retained_image_integration nsight-graphics-mcp)

set(NGM_CPP_CASES "" CACHE FILEPATH "Explicit retained C++ inputs for opt-in validation")
ngm_add_hardware_check(ngm_retained_cpp_integration SOURCES RetainedCppIntegration.cpp
    LIBRARIES ngm_core nlohmann_json::nlohmann_json
    ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "${NGM_CAPTURE_ARTIFACT_ROOT}"
        "${NGM_CPP_CASES}" "${NGM_CAPTURE_VALIDATION_ROOT}")
add_dependencies(ngm_retained_cpp_integration nsight-graphics-mcp)

# Explicit trusted-capture experiment; never a default check or product tool.
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/descriptor-hydration/cases.json" descriptor_cases_sha256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/descriptor-hydration/cases.json")
ngm_add_hardware_check(ngm_descriptor_hydration_integration SOURCES DescriptorHydrationIntegration.cpp
    LIBRARIES ngm_artifacts
    ARGS "${NGM_CAPTURE_ARTIFACT_ROOT}" "${NGM_CAPTURE_VALIDATION_ROOT}")
target_compile_definitions(ngm_descriptor_hydration_integration PRIVATE
    NGM_SOURCE_ROOT="${PROJECT_SOURCE_DIR}" NGM_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
    NGM_CORE_ARCHIVE="$<TARGET_FILE:ngm_core>" NGM_DESCRIPTOR_CASES_SHA256="${descriptor_cases_sha256}")

ngm_register_check(ngm_cli_check
    COMMAND "${CMAKE_COMMAND}"
        "-DSERVER=$<TARGET_FILE:nsight-graphics-mcp>"
        "-DFIXTURE=$<TARGET_FILE:ngm-vulkan-fixture>"
        "-DCAPTURE=$<TARGET_FILE:ngm-capture>"
        "-DEXPECTED_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCLI.cmake"
    DEPENDS nsight-graphics-mcp ngm-vulkan-fixture ngm-capture)

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
        "-DBINARIES=$<TARGET_FILE:nsight-graphics-mcp>|$<TARGET_FILE:ngm-vulkan-fixture>|$<TARGET_FILE:ngm-experiment>|$<TARGET_FILE:ngm-capture>|$<TARGET_FILE:ngm_dependency_check>|$<TARGET_FILE:glslang-standalone>"
        "-DDESKTOP_BINARY=$<TARGET_FILE:ngm-vulkan-fixture>"
        "-DARCHIVES=$<TARGET_FILE:fastmcpp_core>|$<TARGET_FILE:volk>|$<TARGET_FILE:glslang>|$<TARGET_FILE:glslang-default-resource-limits>|$<TARGET_FILE:ngm_lodepng>"
        -P "${PROJECT_SOURCE_DIR}/cmake/AuditLinkage.cmake"
    DEPENDS nsight-graphics-mcp ngm-vulkan-fixture ngm-experiment ngm-capture ngm_dependency_check glslang-standalone)

ngm_finalize_checks()
