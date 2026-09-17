foreach(required IN ITEMS COMPILER SOURCE OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
get_filename_component(directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${directory}")
# A failed compile must not leave an older successful output available.
file(REMOVE "${OUTPUT}" "${OUTPUT}.tmp")
execute_process(COMMAND "${COMPILER}" -V --target-env vulkan1.3 -g -Od
        -o "${OUTPUT}.tmp" "${SOURCE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
if(NOT result STREQUAL "0" OR NOT EXISTS "${OUTPUT}.tmp")
    file(REMOVE "${OUTPUT}.tmp")
    message(FATAL_ERROR "GLSL compilation failed (${result}) for ${SOURCE}\n${output}${error}")
endif()
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
