foreach(required IN ITEMS BUILD_ROOT EXPECTED_VERSION)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef attempt)
set(stage "${BUILD_ROOT}/tests/install-${attempt}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${stage}"
    "${CMAKE_COMMAND}" --install "${BUILD_ROOT}" --prefix /usr --component Runtime
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Runtime installation failed: ${output}\n${error}")
endif()
foreach(name IN ITEMS nsight-graphics-mcp ngm-capture)
    execute_process(COMMAND "${stage}/usr/bin/${name}" --version
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
    if(NOT status STREQUAL "0" OR NOT output STREQUAL "${name} ${EXPECTED_VERSION}\n" OR NOT error STREQUAL "")
        message(FATAL_ERROR "Installed ${name} version failed: ${output}\n${error}")
    endif()
endforeach()
foreach(path IN ITEMS
        share/licenses/nsight-graphics-mcp/LICENSE
        share/licenses/nsight-graphics-mcp/fastmcpp/LICENSE
        share/licenses/nsight-graphics-mcp/fastmcpp/NOTICE
        share/licenses/nsight-graphics-mcp/json/LICENSE.MIT
        share/licenses/nsight-graphics-mcp/cpp-httplib/LICENSE
        share/licenses/nsight-graphics-mcp/lodepng/LICENSE
        share/doc/nsight-graphics-mcp/README.md
        share/doc/nsight-graphics-mcp/docs/MCP.md
        share/doc/nsight-graphics-mcp/docs/images/shader-before.png)
    if(NOT EXISTS "${stage}/usr/${path}")
        message(FATAL_ERROR "Missing runtime payload: ${path}")
    endif()
endforeach()
file(GLOB binaries RELATIVE "${stage}/usr/bin" "${stage}/usr/bin/*")
list(SORT binaries)
if(NOT binaries STREQUAL "ngm-capture;nsight-graphics-mcp" OR EXISTS "${stage}/usr/lib")
    message(FATAL_ERROR "Runtime install contains unexpected executables or development libraries")
endif()
execute_process(COMMAND "${stage}/usr/bin/nsight-graphics-mcp" INPUT_FILE /dev/null
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "")
    message(FATAL_ERROR "Installed server failed clean stdio EOF: ${output}\n${error}")
endif()
