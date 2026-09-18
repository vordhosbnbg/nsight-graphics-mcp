# Add after ngm_core exists. The coordinator has no MCP or artifact-store dependency.
function(ngm_add_jobs)
    target_sources(ngm_core PRIVATE "${PROJECT_SOURCE_DIR}/src/jobs/Jobs.cpp")
endfunction()

# Invoke after ngm_process_standin exists and before ngm_finalize_checks().
function(ngm_add_jobs_checks)
    ngm_add_check(ngm_jobs_check SOURCES "${PROJECT_SOURCE_DIR}/tests/JobsCheck.cpp"
        LIBRARIES ngm_core
        ARGS "$<TARGET_FILE:ngm_process_standin>" DEPENDS ngm_process_standin)
endfunction()
