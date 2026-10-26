function(ngm_add_inspection)
    add_library(ngm_shader_trace STATIC "${PROJECT_SOURCE_DIR}/src/inspection/ShaderTrace.cpp")
    target_include_directories(ngm_shader_trace PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(ngm_shader_trace PUBLIC nlohmann_json::nlohmann_json)
    ngm_first_party(ngm_shader_trace)
    add_library(ngm_inspection STATIC "${PROJECT_SOURCE_DIR}/src/inspection/Inspection.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ImageEvidence.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/CppEvidence.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/DrawState.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/PushState.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/DescriptorState.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ResourceRead.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/DescriptorRead.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/SpirvUniforms.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ValueMemory.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ValueUploads.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ValueDescriptors.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ValueTimeline.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ValueSourceContract.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ProfileInspection.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ProfileMarkers.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ReplayTiming.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ReplayTimingInspection.cpp")
    target_link_libraries(ngm_inspection PUBLIC ngm_artifacts ngm_nsight ngm_shader_trace)
    target_include_directories(ngm_inspection SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/glslang/SPIRV")
    ngm_first_party(ngm_inspection)
endfunction()

function(ngm_add_inspection_checks)
    ngm_add_check(ngm_shader_trace_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ShaderTraceCheck.cpp"
        LIBRARIES ngm_inspection ngm_capture ARGS "$<TARGET_FILE:ngm_nsight_standin>" DEPENDS ngm_nsight_standin)
    ngm_add_check(ngm_shader_trace_mcp_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ShaderTraceMcpCheck.cpp"
        LIBRARIES ngm_inspection ARGS "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:nsight-graphics-mcp>"
        DEPENDS ngm_nsight_standin nsight-graphics-mcp)
    ngm_add_check(ngm_value_source_contract_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ValueSourceContractCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_value_source_contract_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_descriptor_lifetime_check SOURCES "${PROJECT_SOURCE_DIR}/tests/DescriptorLifetimeCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_descriptor_lifetime_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_value_upload_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ValueUploadCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_value_upload_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_value_memory_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ValueMemoryCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_value_memory_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_value_source_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ValueSourceCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_value_source_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_compile_shader("${PROJECT_SOURCE_DIR}/tests/shaders/uniforms.frag"
        "${CMAKE_BINARY_DIR}/shaders/uniforms.frag.spv")
    add_custom_target(ngm_uniform_test_shader DEPENDS "${CMAKE_BINARY_DIR}/shaders/uniforms.frag.spv")
    ngm_add_check(ngm_spirv_uniform_check SOURCES "${PROJECT_SOURCE_DIR}/tests/SpirvUniformCheck.cpp"
        LIBRARIES ngm_inspection
        ARGS "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/scene.frag.spv" "${CMAKE_BINARY_DIR}/shaders/uniforms.frag.spv"
        DEPENDS ngm_fixture_shaders ngm_uniform_test_shader)
    ngm_compile_shader("${PROJECT_SOURCE_DIR}/tests/shaders/uniform-storage-write.frag"
        "${CMAKE_BINARY_DIR}/shaders/uniform-storage-write.frag.spv")
    add_custom_target(ngm_effect_test_shader DEPENDS "${CMAKE_BINARY_DIR}/shaders/uniform-storage-write.frag.spv")
    ngm_add_check(ngm_spirv_effect_check SOURCES "${PROJECT_SOURCE_DIR}/tests/SpirvEffectCheck.cpp"
        LIBRARIES ngm_inspection
        ARGS "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/scene.vert.spv" "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/scene.frag.spv"
            "${CMAKE_BINARY_DIR}/shaders/uniform-storage-write.frag.spv"
        DEPENDS ngm_fixture_shaders ngm_effect_test_shader)
    ngm_add_check(ngm_value_timeline_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ValueTimelineCheck.cpp"
        LIBRARIES ngm_inspection
        ARGS "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/scene.vert.spv" "${NGM_FIXTURE_SHADER_OUTPUT_DIR}/scene.frag.spv"
            "${CMAKE_BINARY_DIR}/shaders/uniform-storage-write.frag.spv"
        DEPENDS ngm_fixture_shaders ngm_effect_test_shader)
    target_include_directories(ngm_value_timeline_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_descriptor_source_check SOURCES "${PROJECT_SOURCE_DIR}/tests/DescriptorSourceCheck.cpp"
        LIBRARIES ngm_inspection)
    add_executable(ngm_descriptor_read_standin "${PROJECT_SOURCE_DIR}/tests/DescriptorReadStandin.cpp")
    target_include_directories(ngm_descriptor_read_standin PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_link_libraries(ngm_descriptor_read_standin PRIVATE ngm_core nlohmann_json::nlohmann_json)
    ngm_first_party(ngm_descriptor_read_standin)
    ngm_add_check(ngm_descriptor_read_check SOURCES "${PROJECT_SOURCE_DIR}/tests/DescriptorReadCheck.cpp"
        LIBRARIES ngm_inspection ARGS "$<TARGET_FILE:ngm_descriptor_read_standin>" DEPENDS ngm_descriptor_read_standin)
    ngm_add_check(ngm_replay_timing_inspection_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ReplayTimingInspectionCheck.cpp"
        LIBRARIES ngm_inspection ngm_capture ARGS "$<TARGET_FILE:ngm_nsight_standin>" DEPENDS ngm_nsight_standin)
    ngm_add_check(ngm_replay_timing_correlation_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ReplayTimingCorrelationCheck.cpp"
        LIBRARIES ngm_inspection)
    target_include_directories(ngm_replay_timing_correlation_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_profile_marker_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ProfileMarkerCheck.cpp" LIBRARIES ngm_inspection
        ARGS "${PROJECT_SOURCE_DIR}/tests/fixtures/profile-2026.3.1/markers")
    ngm_add_check(ngm_descriptor_state_check SOURCES "${PROJECT_SOURCE_DIR}/tests/DescriptorStateCheck.cpp" LIBRARIES ngm_inspection)
    ngm_add_check(ngm_push_state_check SOURCES "${PROJECT_SOURCE_DIR}/tests/PushStateCheck.cpp" LIBRARIES ngm_inspection)
    target_include_directories(ngm_push_state_check PRIVATE "${PROJECT_SOURCE_DIR}/src/inspection")
    ngm_add_check(ngm_draw_state_check SOURCES "${PROJECT_SOURCE_DIR}/tests/DrawStateCheck.cpp" LIBRARIES ngm_inspection)
    ngm_add_check(ngm_resource_reference_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ResourceReferenceCheck.cpp"
        LIBRARIES ngm_inspection)
    add_executable(ngm_resource_read_standin "${PROJECT_SOURCE_DIR}/tests/ResourceReadStandin.cpp")
    target_link_libraries(ngm_resource_read_standin PRIVATE ngm_core nlohmann_json::nlohmann_json)
    ngm_first_party(ngm_resource_read_standin)
    ngm_add_check(ngm_resource_read_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ResourceReadCheck.cpp"
        LIBRARIES ngm_inspection ARGS "$<TARGET_FILE:ngm_resource_read_standin>" DEPENDS ngm_resource_read_standin)
    ngm_add_check(ngm_cpp_inspection_check SOURCES "${PROJECT_SOURCE_DIR}/tests/CppInspectionCheck.cpp"
        LIBRARIES ngm_inspection)
    ngm_add_check(ngm_cpp_evidence_check SOURCES "${PROJECT_SOURCE_DIR}/tests/CppEvidenceCheck.cpp"
        LIBRARIES ngm_inspection)
    ngm_add_check(ngm_inspection_check SOURCES "${PROJECT_SOURCE_DIR}/tests/InspectionCheck.cpp"
        LIBRARIES ngm_inspection
        ARGS "${PROJECT_SOURCE_DIR}/tests/fixtures/nsight-2026.3.1"
            "${PROJECT_SOURCE_DIR}/tests/fixtures/nsight-2026.2.0")
endfunction()
