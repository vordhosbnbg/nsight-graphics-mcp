add_custom_target(ngm_check)

# Every registered ordinary check gets a CTest entry and focused run target.
function(ngm_register_check name)
    cmake_parse_arguments(PARSE_ARGV 1 check "" "" "COMMAND;DEPENDS")
    if(check_UNPARSED_ARGUMENTS OR NOT check_COMMAND)
        message(FATAL_ERROR "ngm_register_check(${name}) requires COMMAND and optional DEPENDS")
    endif()
    add_test(NAME ${name} COMMAND ${check_COMMAND})
    set_tests_properties(${name} PROPERTIES LABELS "cpu" TIMEOUT 90)
    add_custom_target(${name}_run
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -R "^${name}$"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS ${check_DEPENDS} USES_TERMINAL VERBATIM)
    set_property(GLOBAL APPEND PROPERTY NGM_CHECK_TARGETS ${check_DEPENDS})
endfunction()

function(ngm_add_check name)
    cmake_parse_arguments(PARSE_ARGV 1 check "" "" "SOURCES;LIBRARIES;ARGS;DEPENDS")
    if(check_UNPARSED_ARGUMENTS OR NOT check_SOURCES)
        message(FATAL_ERROR "ngm_add_check(${name}) requires SOURCES")
    endif()
    add_executable(${name} ${check_SOURCES})
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_link_libraries(${name} PRIVATE ${check_LIBRARIES})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    ngm_first_party(${name})
    ngm_register_check(${name} COMMAND "$<TARGET_FILE:${name}>" ${check_ARGS}
        DEPENDS ${name} ${check_DEPENDS})
endfunction()

# Hardware checks deliberately have no CTest entry or ngm_check dependency.
# Their executables must also be EXCLUDE_FROM_ALL. R-005 adds the first real run.
function(ngm_add_hardware_check name)
    cmake_parse_arguments(PARSE_ARGV 1 check "" "" "SOURCES;LIBRARIES;ARGS")
    if(check_UNPARSED_ARGUMENTS OR NOT check_SOURCES)
        message(FATAL_ERROR "ngm_add_hardware_check(${name}) requires SOURCES")
    endif()
    add_executable(${name} EXCLUDE_FROM_ALL ${check_SOURCES})
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_link_libraries(${name} PRIVATE ${check_LIBRARIES})
    ngm_first_party(${name})
    add_custom_target(${name}_run COMMAND "$<TARGET_FILE:${name}>" ${check_ARGS}
        DEPENDS ${name} USES_TERMINAL VERBATIM)
endfunction()

function(ngm_finalize_checks)
    get_property(targets GLOBAL PROPERTY NGM_CHECK_TARGETS)
    add_custom_target(ngm_check_all
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -L "^cpu$" --no-tests=error
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS ${targets} USES_TERMINAL VERBATIM)
    add_dependencies(ngm_check ngm_check_all)
endfunction()
