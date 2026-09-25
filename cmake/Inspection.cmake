function(ngm_add_inspection)
    add_library(ngm_inspection STATIC "${PROJECT_SOURCE_DIR}/src/inspection/Inspection.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ImageEvidence.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/CppEvidence.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ResourceRead.cpp"
        "${PROJECT_SOURCE_DIR}/src/inspection/ProfileInspection.cpp")
    target_link_libraries(ngm_inspection PUBLIC ngm_artifacts ngm_nsight)
    ngm_first_party(ngm_inspection)
endfunction()

function(ngm_add_inspection_checks)
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
