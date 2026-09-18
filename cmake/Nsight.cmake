function(ngm_add_nsight)
    add_library(ngm_nsight STATIC
        "${PROJECT_SOURCE_DIR}/src/nsight/Nsight.cpp"
        "${PROJECT_SOURCE_DIR}/src/nsight/Evidence.cpp")
    target_link_libraries(ngm_nsight PUBLIC ngm_core nlohmann_json::nlohmann_json)
    ngm_first_party(ngm_nsight)
endfunction()

function(ngm_add_nsight_checks)
    add_executable(ngm_nsight_standin "${PROJECT_SOURCE_DIR}/tests/NsightStandin.cpp")
    target_link_libraries(ngm_nsight_standin PRIVATE Threads::Threads)
    ngm_first_party(ngm_nsight_standin)
    ngm_add_check(ngm_nsight_check SOURCES "${PROJECT_SOURCE_DIR}/tests/NsightCheck.cpp"
        LIBRARIES ngm_nsight
        ARGS "$<TARGET_FILE:ngm_nsight_standin>" DEPENDS ngm_nsight_standin)
    ngm_add_check(ngm_nsight_evidence_check SOURCES "${PROJECT_SOURCE_DIR}/tests/NsightEvidenceCheck.cpp"
        LIBRARIES ngm_nsight
        ARGS "${PROJECT_SOURCE_DIR}/tests/fixtures/nsight-2026.3.1")
endfunction()
