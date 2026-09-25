function(ngm_compile_shader source output)
    set(profile diagnostic)
    if(source MATCHES "/performance-[^/]+\\.comp$")
        set(profile performance)
    endif()
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILER=$<TARGET_FILE:glslang-standalone>"
            "-DSOURCE=${source}" "-DOUTPUT=${output}" "-DPROFILE=${profile}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CompileShader.cmake"
        DEPENDS glslang-standalone "${source}" "${PROJECT_SOURCE_DIR}/cmake/CompileShader.cmake"
        COMMENT "Compiling ${profile} GLSL: ${source}"
        VERBATIM)
endfunction()
