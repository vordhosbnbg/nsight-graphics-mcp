include(GNUInstallDirs)

# Runtime installation deliberately contains only the server and capture CLI.
# Development fixtures, archives and proprietary toolchain helpers stay in the build tree.
install(TARGETS nsight-graphics-mcp ngm-capture
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
set(ngm_license_dir "${CMAKE_INSTALL_DATAROOTDIR}/licenses/nsight-graphics-mcp")
install(FILES LICENSE DESTINATION "${ngm_license_dir}" COMPONENT Runtime)
foreach(dependency IN ITEMS fastmcpp json cpp-httplib lodepng)
    if(dependency STREQUAL "json")
        set(notice LICENSE.MIT)
    else()
        set(notice LICENSE)
    endif()
    install(FILES "external/${dependency}/${notice}"
        DESTINATION "${ngm_license_dir}/${dependency}" COMPONENT Runtime)
endforeach()
install(FILES external/fastmcpp/NOTICE
    DESTINATION "${ngm_license_dir}/fastmcpp" COMPONENT Runtime)

# Keep README-relative documentation and image links intact.
set(ngm_doc_dir "${CMAKE_INSTALL_DATAROOTDIR}/doc/nsight-graphics-mcp")
install(FILES README.md LICENSE DESTINATION "${ngm_doc_dir}" COMPONENT Runtime)
install(DIRECTORY docs/ DESTINATION "${ngm_doc_dir}/docs" COMPONENT Runtime
    FILES_MATCHING PATTERN "*.md" PATTERN "*.png")
