# Add after nsight-graphics-mcp, ngm_core, and ngm_capture exist. The stdio
# adapters reuse the transport-independent discovery/capture/job/artifact cores.
function(ngm_add_mcp)
    target_sources(ngm_core PRIVATE "${PROJECT_SOURCE_DIR}/src/core/Capabilities.cpp")
    target_sources(nsight-graphics-mcp PRIVATE
        "${PROJECT_SOURCE_DIR}/src/server/Server.cpp"
        "${PROJECT_SOURCE_DIR}/src/server/Http.cpp"
        "${PROJECT_SOURCE_DIR}/src/server/Workflow.cpp")
    target_link_libraries(nsight-graphics-mcp PRIVATE ngm_capture ngm_inspection)
endfunction()

# Invoke from tests/CMakeLists.txt before ngm_finalize_checks().
function(ngm_add_mcp_checks)
    ngm_add_check(ngm_mcp_check SOURCES "${PROJECT_SOURCE_DIR}/tests/McpCheck.cpp"
        LIBRARIES ngm_core nlohmann_json::nlohmann_json fastmcpp_core
        ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "$<TARGET_FILE:ngm_nsight_standin>"
        DEPENDS nsight-graphics-mcp ngm_nsight_standin)
    ngm_add_check(ngm_http_check SOURCES "${PROJECT_SOURCE_DIR}/tests/HttpCheck.cpp"
        LIBRARIES ngm_core nlohmann_json::nlohmann_json fastmcpp_core
        ARGS "$<TARGET_FILE:nsight-graphics-mcp>" "$<TARGET_FILE:ngm_nsight_standin>" "$<TARGET_FILE:ngm_mcp_check>"
        DEPENDS nsight-graphics-mcp ngm_nsight_standin ngm_mcp_check)
endfunction()
