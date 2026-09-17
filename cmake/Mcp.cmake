# Add after nsight-graphics-mcp and ngm_core exist. Keep the protocol adapter
# separate from prerequisite discovery so later transports can reuse the core.
function(ngm_add_mcp)
    target_sources(ngm_core PRIVATE "${PROJECT_SOURCE_DIR}/src/core/Capabilities.cpp")
    target_sources(nsight-graphics-mcp PRIVATE "${PROJECT_SOURCE_DIR}/src/server/Server.cpp")
endfunction()

# Invoke from tests/CMakeLists.txt before ngm_finalize_checks().
function(ngm_add_mcp_checks)
    ngm_add_check(ngm_mcp_check SOURCES "${PROJECT_SOURCE_DIR}/tests/McpCheck.cpp"
        LIBRARIES ngm_core nlohmann_json::nlohmann_json
        ARGS "$<TARGET_FILE:nsight-graphics-mcp>" DEPENDS nsight-graphics-mcp)
endfunction()
