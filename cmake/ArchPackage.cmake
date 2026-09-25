if(NOT NGM_ENABLE_ARCH_PACKAGING)
    return()
endif()
if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    message(FATAL_ERROR "The Arch package is currently qualified only for native Linux x86-64")
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
find_program(NGM_MAKEPKG makepkg REQUIRED)
set(ngm_arch_dir "${CMAKE_BINARY_DIR}/arch")
file(MAKE_DIRECTORY "${ngm_arch_dir}")
add_custom_target(ngm_arch_sources
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/packaging/arch/prepare.py"
        --source "${PROJECT_SOURCE_DIR}" --output "${ngm_arch_dir}"
        --version "${PROJECT_VERSION}" --makepkg "${NGM_MAKEPKG}"
    COMMENT "Preparing pinned source archive, PKGBUILD and .SRCINFO"
    USES_TERMINAL VERBATIM)
add_custom_target(ngm_arch_package
    COMMAND "${CMAKE_COMMAND}" -E env
        "PKGDEST=${ngm_arch_dir}" "SRCDEST=${ngm_arch_dir}"
        "SRCPKGDEST=${ngm_arch_dir}" "BUILDDIR=${ngm_arch_dir}" "LOGDEST=${ngm_arch_dir}"
        "PKGEXT=.pkg.tar.zst"
        "${NGM_MAKEPKG}" --dir "${ngm_arch_dir}" --force --cleanbuild --check --log
    DEPENDS ngm_arch_sources
    COMMENT "Building and checking the Arch Linux package with makepkg"
    USES_TERMINAL VERBATIM)
