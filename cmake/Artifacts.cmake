function(ngm_add_artifacts)
    add_library(ngm_artifacts STATIC "${PROJECT_SOURCE_DIR}/src/artifacts/Artifacts.cpp")
    target_link_libraries(ngm_artifacts PUBLIC ngm_core nlohmann_json::nlohmann_json)
    ngm_first_party(ngm_artifacts)
endfunction()

function(ngm_add_artifact_checks)
    ngm_add_check(ngm_artifacts_check SOURCES "${PROJECT_SOURCE_DIR}/tests/ArtifactsCheck.cpp"
        LIBRARIES ngm_artifacts
        ARGS "${CMAKE_CURRENT_BINARY_DIR}/artifacts-check")
    # Inject faults at the real POSIX boundary without production test hooks.
    target_link_options(ngm_artifacts_check PRIVATE
        "-Wl,--wrap=write" "-Wl,--wrap=fsync" "-Wl,--wrap=renameat" "-Wl,--wrap=unlinkat")
endfunction()
