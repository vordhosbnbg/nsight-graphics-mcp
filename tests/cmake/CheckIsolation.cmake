file(MAKE_DIRECTORY "${SCRATCH}")
file(WRITE "${SCRATCH}/probe.cpp" "int main() { return 0; }\n")
file(WRITE "${SCRATCH}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\nproject(CheckIsolation LANGUAGES CXX)\n"
    "enable_testing()\nfunction(ngm_first_party target)\nendfunction()\n"
    "include(\"${PROJECT_ROOT}/cmake/Checks.cmake\")\n"
    "ngm_add_check(cpu_probe SOURCES probe.cpp)\n"
    "ngm_add_hardware_check(hardware_probe SOURCES probe.cpp)\nngm_finalize_checks()\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SCRATCH}" -B "${SCRATCH}/build" -G Ninja
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Check helper fixture failed to configure: ${output}${error}")
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
