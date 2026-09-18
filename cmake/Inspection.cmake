function(ngm_add_inspection)
    add_library(ngm_inspection STATIC "${PROJECT_SOURCE_DIR}/src/inspection/Inspection.cpp")
    target_link_libraries(ngm_inspection PUBLIC ngm_artifacts ngm_nsight)
    ngm_first_party(ngm_inspection)
endfunction()

function(ngm_add_inspection_checks)
    ngm_add_check(ngm_inspection_check SOURCES "${PROJECT_SOURCE_DIR}/tests/InspectionCheck.cpp"
        LIBRARIES ngm_inspection
        ARGS "${PROJECT_SOURCE_DIR}/tests/fixtures/nsight-2026.3.1"
            "${PROJECT_SOURCE_DIR}/tests/fixtures/nsight-2026.2.0")
endfunction()
