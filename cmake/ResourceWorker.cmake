# Optional generated helpers are outputs of the separately installed Nsight
# toolchain. No acquisition, generated build scripts, or runtime compiler.
set(NGM_RESOURCE_HELPERS_2026_3 "" CACHE PATH "Generated C++ helper directory from qualified Nsight 2026.3")
set(NGM_RESOURCE_HELPERS_2026_2 "" CACHE PATH "Generated C++ helper directory from qualified Nsight 2026.2")

function(ngm_add_resource_workers)
    add_library(ngm_worker_confinement STATIC "${PROJECT_SOURCE_DIR}/src/inspection/WorkerConfinement.cpp")
    target_include_directories(ngm_worker_confinement PUBLIC "${PROJECT_SOURCE_DIR}/include")
    ngm_first_party(ngm_worker_confinement)
    foreach(profile IN ITEMS 2026_3 2026_2)
        if(NOT NGM_RESOURCE_HELPERS_${profile})
            continue()
        endif()
        if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            message(FATAL_ERROR "Resource workers currently require Linux x86-64")
        endif()
        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles("#include <linux/landlock.h>\n#include <sys/syscall.h>\nint main() { return LANDLOCK_ACCESS_FS_TRUNCATE && SYS_landlock_create_ruleset && SYS_landlock_add_rule && SYS_landlock_restrict_self ? 0 : 1; }"
            NGM_HAVE_LANDLOCK_HEADERS)
        if(NOT NGM_HAVE_LANDLOCK_HEADERS)
            message(FATAL_ERROR "Optional resource workers need Linux UAPI headers with Landlock ABI 3 (Linux 6.2+)")
        endif()
        set(root "${NGM_RESOURCE_HELPERS_${profile}}")
        if(NOT IS_ABSOLUTE "${root}")
            message(FATAL_ERROR "NGM_RESOURCE_HELPERS_${profile} must be an absolute generated helper directory")
        endif()
        if(profile STREQUAL "2026_3")
            set(hashes c0297095e1a7e728688b6288dc8a7dcf455c54ad0af4c2e5f8ba6ce05bbfd687
                b3c8689f67cdfe2ef0b529f90953bd0ceb17589b8444c98e34f26dd15281c7da
                dc1ec0e3738e4f1650d90625fd2ee304be187afe5ac3d43b2a88b666618ce1b4
                cd79e92f6b783b1f0b884cc1ba5fb0740a8cbf027f9eca52b731636dc8d32ab0
                de074900baba02ca4ee46580d5b7bfee87f7902c544fef7eb06c0f107f27a0e6)
            set(identity nsight-2026.3.1-build-38722833-linux-x86_64)
        else()
            set(hashes 8a5dc3b886288f534babbcfb0e26844a1c0cc78fc98cf296e76c31edb4ea9227
                0d1c7a3011cfc0732505ad3fdfeb495a6541494162b1f6f45c41255d375f3f45
                dc1ec0e3738e4f1650d90625fd2ee304be187afe5ac3d43b2a88b666618ce1b4
                e6475c9a52effd068b7cf84fd80c4f42c280e8d2c8b1bc20891ea266e9ddb974
                de074900baba02ca4ee46580d5b7bfee87f7902c544fef7eb06c0f107f27a0e6)
            set(identity nsight-2026.2.0-build-37991608-linux-x86_64)
        endif()
        set(names ReadOnlyDatabase.cpp ReadOnlyDatabase.h DataScope.cpp DataScope.h DllCommon.h)
        set(snapshot "${CMAKE_CURRENT_BINARY_DIR}/generated/resource-worker-${profile}")
        file(MAKE_DIRECTORY "${snapshot}")
        set(files "{}")
        foreach(index RANGE 0 4)
            list(GET names ${index} name)
            list(GET hashes ${index} expected)
            if(NOT EXISTS "${root}/${name}" OR IS_SYMLINK "${root}/${name}" OR IS_DIRECTORY "${root}/${name}")
                message(FATAL_ERROR "Missing regular helper ${root}/${name}")
            endif()
            # Compile only a copy of this closure, never against the generated
            # project include path. A changed input triggers reconfiguration.
            configure_file("${root}/${name}" "${snapshot}/${name}" COPYONLY)
            file(SHA256 "${snapshot}/${name}" actual)
            if(NOT actual STREQUAL expected)
                message(FATAL_ERROR "Unqualified resource helper ${name}: ${actual}; expected ${expected}")
            endif()
            string(JSON files SET "${files}" "${name}" "\"${actual}\"")
        endforeach()
        set(manifest "{\"schema_version\":1,\"profile\":\"${identity}\",\"project_version\":\"${PROJECT_VERSION}\",\"files\":${files}}")
        file(CONFIGURE OUTPUT "${snapshot}/ResourceWorkerBuild.hpp" CONTENT
            "#pragma once\nnamespace ngm { inline constexpr char resource_worker_profile[] = \"${identity}\"; inline constexpr char resource_worker_build_json[] = R\"ngm_worker(${manifest})ngm_worker\"; }\n" @ONLY)
        file(CONFIGURE OUTPUT "${snapshot}/profile.json" CONTENT "${manifest}\n" @ONLY)
        add_library(ngm_resource_helper_${profile} STATIC "${snapshot}/ReadOnlyDatabase.cpp" "${snapshot}/DataScope.cpp")
        target_compile_features(ngm_resource_helper_${profile} PRIVATE cxx_std_20)
        target_include_directories(ngm_resource_helper_${profile} SYSTEM PUBLIC "${snapshot}")
        add_executable(ngm-resource-worker-${profile} "${PROJECT_SOURCE_DIR}/src/inspection/ResourceWorker.cpp")
        target_link_libraries(ngm-resource-worker-${profile} PRIVATE ngm_worker_confinement ngm_resource_helper_${profile})
        ngm_first_party(ngm-resource-worker-${profile})
    endforeach()
endfunction()
