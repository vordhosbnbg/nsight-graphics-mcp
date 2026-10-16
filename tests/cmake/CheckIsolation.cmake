file(MAKE_DIRECTORY "${SCRATCH}")
file(WRITE "${SCRATCH}/probe.cpp"
    "#include <fstream>\nint main(int argc, char** argv) { return argc != 2 || !std::ifstream(argv[1]).good(); }\n")
file(WRITE "${SCRATCH}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\nproject(CheckIsolation LANGUAGES CXX)\n"
    "enable_testing()\nfunction(ngm_first_party target)\nendfunction()\n"
    "include(\"${PROJECT_ROOT}/cmake/Checks.cmake\")\n"
    "add_custom_command(OUTPUT input.txt COMMAND \"${CMAKE_COMMAND}\" -E touch input.txt VERBATIM)\n"
    "add_custom_target(generated_input DEPENDS input.txt)\n"
    "ngm_add_check(cpu_probe SOURCES probe.cpp ARGS \"${SCRATCH}/build/input.txt\" DEPENDS generated_input)\n"
    "ngm_add_hardware_check(hardware_probe SOURCES probe.cpp)\nngm_finalize_checks()\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SCRATCH}" -B "${SCRATCH}/build" -G Ninja
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Check helper fixture failed to configure: ${output}${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${SCRATCH}/build" --target cpu_probe
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0" OR NOT EXISTS "${SCRATCH}/build/input.txt"
        OR EXISTS "${SCRATCH}/build/hardware_probe")
    message(FATAL_ERROR "Focused check build omitted its runtime input or built hardware work: ${output}${error}")
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${SCRATCH}/build" --output-on-failure -R "^cpu_probe$"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Focused check could not use its generated runtime input: ${output}${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${SCRATCH}/build" --target ngm_check
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0" OR EXISTS "${SCRATCH}/build/hardware_probe")
    message(FATAL_ERROR "Aggregate checks failed or built hardware work: ${output}${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${SCRATCH}/build"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0" OR EXISTS "${SCRATCH}/build/hardware_probe")
    message(FATAL_ERROR "Default build failed or built hardware work: ${output}${error}")
endif()
file(READ "${SCRATCH}/build/CTestTestfile.cmake" tests)
if(tests MATCHES "hardware_probe" OR NOT tests MATCHES "cpu_probe")
    message(FATAL_ERROR "Ordinary CTest registration must include only CPU work")
endif()
