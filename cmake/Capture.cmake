function(ngm_add_capture)
    add_library(ngm_capture STATIC "${PROJECT_SOURCE_DIR}/src/capture/CaptureService.cpp"
        "${PROJECT_SOURCE_DIR}/src/capture/LaunchEnvironment.cpp")
    target_link_libraries(ngm_capture PUBLIC ngm_artifacts ngm_nsight ngm_core ngm_inspection)
    ngm_first_party(ngm_capture)
    add_executable(ngm-capture "${PROJECT_SOURCE_DIR}/src/capture/Main.cpp")
    target_link_libraries(ngm-capture PRIVATE ngm_capture)
    ngm_first_party(ngm-capture)
endfunction()

function(ngm_add_capture_checks)
    ngm_add_check(ngm_replay_timing_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ReplayTimingCheck.cpp"
        LIBRARIES ngm_capture ARGS "$<TARGET_FILE:ngm_nsight_standin>" DEPENDS ngm_nsight_standin)
    ngm_add_check(ngm_replay_workflow_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ReplayWorkflowCheck.cpp"
        LIBRARIES ngm_capture
        ARGS "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:nsight-graphics-mcp>"
        DEPENDS ngm_nsight_standin nsight-graphics-mcp)
    ngm_add_check(ngm_launch_environment_check SOURCES "${PROJECT_SOURCE_DIR}/tests/LaunchEnvironmentCheck.cpp"
        LIBRARIES ngm_capture)
    ngm_add_check(ngm_profile_compare_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ProfileCompareCheck.cpp"
        LIBRARIES ngm_capture ngm_inspection
        ARGS "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:nsight-graphics-mcp>"
        DEPENDS ngm_nsight_standin nsight-graphics-mcp)
    ngm_add_check(ngm_profile_workflow_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ProfileWorkflowCheck.cpp"
        LIBRARIES ngm_capture ngm_inspection
        ARGS "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:nsight-graphics-mcp>"
        DEPENDS ngm_nsight_standin nsight-graphics-mcp)
    target_compile_definitions(ngm_profile_workflow_check PRIVATE
        NGM_PROFILE_TABLE_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/profile-2026.3.1/header-only"
        NGM_PROFILE_MARKER_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/profile-2026.3.1/markers")
    ngm_add_check(ngm_capture_service_check SOURCES "${PROJECT_SOURCE_DIR}/tests/CaptureServiceCheck.cpp"
        LIBRARIES ngm_capture
        ARGS "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:ngm-capture>"
        DEPENDS ngm_nsight_standin ngm-capture)
endfunction()
