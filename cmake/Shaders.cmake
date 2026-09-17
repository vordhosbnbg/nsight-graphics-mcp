function(ngm_compile_shader source output)
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILER=$<TARGET_FILE:glslang-standalone>"
            "-DSOURCE=${source}" "-DOUTPUT=${output}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CompileShader.cmake"
        DEPENDS glslang-standalone "${source}" "${PROJECT_SOURCE_DIR}/cmake/CompileShader.cmake"
        COMMENT "Compiling diagnostic GLSL: ${source}"
        VERBATIM)
endfunction()
