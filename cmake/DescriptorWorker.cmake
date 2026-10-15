# Owner-approved 2026-10-15 seven-file extension. No generated replay is built.
set(NGM_DESCRIPTOR_HELPERS_2026_3 "" CACHE PATH "Exact generated 2026.3 descriptor helper directory")

function(ngm_descriptor_replace variable before after)
    string(FIND "${${variable}}" "${before}" position)
    if(position LESS 0)
        message(FATAL_ERROR "Descriptor bounds instrumentation anchor changed")
    endif()
    string(LENGTH "${before}" length)
    math(EXPR end "${position} + ${length}")
    string(SUBSTRING "${${variable}}" ${end} -1 tail)
    string(FIND "${tail}" "${before}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "Descriptor bounds instrumentation anchor is ambiguous")
    endif()
    string(REPLACE "${before}" "${after}" result "${${variable}}")
    set(${variable} "${result}" PARENT_SCOPE)
endfunction()

function(ngm_add_descriptor_worker)
    set(names ReadOnlyDatabase.cpp ReadOnlyDatabase.h DataScope.cpp DataScope.h DllCommon.h
        VulkanStructHydrator.cpp VulkanStructHydrator.h)
    set(hashes c0297095e1a7e728688b6288dc8a7dcf455c54ad0af4c2e5f8ba6ce05bbfd687
        b3c8689f67cdfe2ef0b529f90953bd0ceb17589b8444c98e34f26dd15281c7da
        dc1ec0e3738e4f1650d90625fd2ee304be187afe5ac3d43b2a88b666618ce1b4
        cd79e92f6b783b1f0b884cc1ba5fb0740a8cbf027f9eca52b731636dc8d32ab0
        de074900baba02ca4ee46580d5b7bfee87f7902c544fef7eb06c0f107f27a0e6
        1e68e3163910a896250b1290c9aaf7e6cd27079bb0a649724de55e667edf534a
        34a9c79394a6dae41e785202aeaceb927e614966389002648a70b7e62d064c3d)
    set(profile "nsight-2026.3.1-build-38722833-linux-x86_64-descriptor-bounds-v1")
    set(expected_header_sha "2edf95461d138df834f578a93b39852b3e7152786dfa080b368ccc025fcaeb5e")
    set(expected_cpp_sha "50596f650f16fe78657611b8eb29dc6deb26fe2b9ed4ed38029ea31b62889f72")
    set(expected_patch_sha "521311fa3521fa4e44c625e191f78e73872af729df0df5caedfe8f89d552e1a0")
    file(SHA256 "${PROJECT_SOURCE_DIR}/include/ngm/DescriptorBounds.hpp" bounds_sha)
    file(SHA256 "${PROJECT_SOURCE_DIR}/cmake/DescriptorWorker.cmake" recipe_sha)
    set(expected_original "{}")
    foreach(index RANGE 0 6)
        list(GET names ${index} name)
        list(GET hashes ${index} digest)
        string(JSON expected_original SET "${expected_original}" "${name}" "\"${digest}\"")
    endforeach()
    set(expected "{\"schema_version\":1,\"profile\":\"${profile}\",\"original_files\":${expected_original},\"instrumented_files\":{\"VulkanStructHydrator.h\":\"${expected_header_sha}\",\"VulkanStructHydrator.cpp\":\"${expected_cpp_sha}\"},\"bounds_sha256\":\"${bounds_sha}\",\"recipe_sha256\":\"${recipe_sha}\",\"patch_sha256\":\"${expected_patch_sha}\"}")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/DescriptorProfiles.hpp" CONTENT
        "#pragma once\nnamespace ngm { inline constexpr char descriptor_profile_json[] = R\"ngm_descriptor(${expected})ngm_descriptor\"; }\n" @ONLY)
    target_include_directories(ngm_inspection PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    if(NOT NGM_DESCRIPTOR_HELPERS_2026_3)
        return()
    endif()
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" OR NOT IS_ABSOLUTE "${NGM_DESCRIPTOR_HELPERS_2026_3}")
        message(FATAL_ERROR "Descriptor worker needs Linux x86-64 and an absolute helper directory")
    endif()
    set(snapshot "${CMAKE_CURRENT_BINARY_DIR}/generated/descriptor-worker-2026_3")
    file(MAKE_DIRECTORY "${snapshot}/original" "${snapshot}/instrumented")
    set(original "{}")
    foreach(index RANGE 0 6)
        list(GET names ${index} name)
        list(GET hashes ${index} expected)
        set(source "${NGM_DESCRIPTOR_HELPERS_2026_3}/${name}")
        if(name STREQUAL "VulkanStructHydrator.h" AND NOT EXISTS "${source}")
            set(source "${NGM_DESCRIPTOR_HELPERS_2026_3}/include/${name}")
        endif()
        if(NOT EXISTS "${source}" OR IS_SYMLINK "${source}" OR IS_DIRECTORY "${source}")
            message(FATAL_ERROR "Missing regular descriptor helper ${source}")
        endif()
        configure_file("${source}" "${snapshot}/original/${name}" COPYONLY)
        file(SHA256 "${snapshot}/original/${name}" actual)
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR "Unqualified descriptor helper ${name}: ${actual}; expected ${expected}")
        endif()
        string(JSON original SET "${original}" "${name}" "\"${actual}\"")
    endforeach()
    file(READ "${snapshot}/original/VulkanStructHydrator.h" header)
    ngm_descriptor_replace(header "#pragma once" "#pragma once\n#include \"ngm/DescriptorBounds.hpp\"")
    ngm_descriptor_replace(header "#ifndef NV_ASSERT\n#define NV_ASSERT(X)\n#endif"
        "#ifdef NV_ASSERT\n#undef NV_ASSERT\n#endif\n#define NV_ASSERT(X) ::ngm::descriptor_assert(static_cast<bool>(X))")
    ngm_descriptor_replace(header "    FnDecodeObject m_fnDecodeObject;"
        "    FnDecodeObject m_fnDecodeObject;\n    ::ngm::DescriptorBounds m_ngmBounds;")
    ngm_descriptor_replace(header "const FnDecodeObject& fnDecodeObject)"
        "const FnDecodeObject& fnDecodeObject, const void* input, size_t inputBytes)")
    ngm_descriptor_replace(header "        , m_fnDecodeObject(fnDecodeObject)"
        "        , m_fnDecodeObject(fnDecodeObject)\n        , m_ngmBounds(input, inputBytes, buffer.data(), buffer.size())")
    ngm_descriptor_replace(header "        const size_t size = sizeof(T) * count;"
        "        m_ngmBounds.copy<T>(count, pIn, m_offset);\n        const size_t size = sizeof(T) * count;")
    ngm_descriptor_replace(header [=[        const uintptr_t offset = reinterpret_cast<uintptr_t>(pSrc);
        const uint8_t* pData = reinterpret_cast<const uint8_t*>(pParent) + offset;
        return reinterpret_cast<const T*>(pData);]=]
        "        return m_ngmBounds.pointer<T>(pParent, pSrc);")
    file(READ "${snapshot}/original/VulkanStructHydrator.cpp" implementation)
    ngm_descriptor_replace(implementation
        "void* StructHydrator::RehydratePNext(const void* pBase, const void* pOffset)\n{"
        "void* StructHydrator::RehydratePNext(const void* pBase, const void* pOffset)\n{\n    if (pOffset) throw std::runtime_error(\"Descriptor pNext is unqualified\");")
    file(CONFIGURE OUTPUT "${snapshot}/instrumented/VulkanStructHydrator.h" CONTENT "${header}" @ONLY)
    file(CONFIGURE OUTPUT "${snapshot}/instrumented/VulkanStructHydrator.cpp" CONTENT "${implementation}" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/include/ngm/DescriptorBounds.hpp"
        "${snapshot}/support/ngm/DescriptorBounds.hpp" COPYONLY)
    find_program(NGM_DESCRIPTOR_DIFF NAMES diff REQUIRED)
    set(patch "")
    foreach(name IN ITEMS VulkanStructHydrator.h VulkanStructHydrator.cpp)
        execute_process(COMMAND "${NGM_DESCRIPTOR_DIFF}" -u --label "original/${name}" --label "instrumented/${name}"
            "${snapshot}/original/${name}" "${snapshot}/instrumented/${name}"
            RESULT_VARIABLE status OUTPUT_VARIABLE changes ERROR_VARIABLE error)
        if(NOT status EQUAL 1 OR error)
            message(FATAL_ERROR "Descriptor instrumentation diff failed: ${status} ${error}")
        endif()
        string(APPEND patch "${changes}")
    endforeach()
    file(CONFIGURE OUTPUT "${snapshot}/instrumentation.patch" CONTENT "${patch}" @ONLY)
    set(instrumented "{}")
    foreach(name IN ITEMS VulkanStructHydrator.h VulkanStructHydrator.cpp)
        file(SHA256 "${snapshot}/instrumented/${name}" digest)
        string(JSON instrumented SET "${instrumented}" "${name}" "\"${digest}\"")
    endforeach()
    file(SHA256 "${PROJECT_SOURCE_DIR}/include/ngm/DescriptorBounds.hpp" bounds_sha)
    file(SHA256 "${PROJECT_SOURCE_DIR}/cmake/DescriptorWorker.cmake" recipe_sha)
    file(SHA256 "${snapshot}/instrumentation.patch" patch_sha)
    if(NOT patch_sha STREQUAL expected_patch_sha)
        message(FATAL_ERROR "Descriptor instrumentation patch changed without a matching parent profile")
    endif()
    string(JSON actual_header_sha GET "${instrumented}" "VulkanStructHydrator.h")
    string(JSON actual_cpp_sha GET "${instrumented}" "VulkanStructHydrator.cpp")
    if(NOT actual_header_sha STREQUAL expected_header_sha OR NOT actual_cpp_sha STREQUAL expected_cpp_sha)
        message(FATAL_ERROR "Descriptor instrumented helpers differ from the parent profile")
    endif()
    set(manifest "{\"schema_version\":1,\"profile\":\"${profile}\",\"project_version\":\"${PROJECT_VERSION}\",\"original_files\":${original},\"instrumented_files\":${instrumented},\"bounds_sha256\":\"${bounds_sha}\",\"recipe_sha256\":\"${recipe_sha}\",\"patch_sha256\":\"${patch_sha}\"}")
    file(CONFIGURE OUTPUT "${snapshot}/profile.json" CONTENT "${manifest}\n" @ONLY)
    file(CONFIGURE OUTPUT "${snapshot}/DescriptorWorkerBuild.hpp" CONTENT
        "#pragma once\nnamespace ngm { inline constexpr char descriptor_worker_build_json[] = R\"ngm_worker(${manifest})ngm_worker\"; inline constexpr char descriptor_worker_profile[] = \"${profile}\"; }\n" @ONLY)
    add_library(ngm_descriptor_helper_2026_3 STATIC EXCLUDE_FROM_ALL
        "${snapshot}/original/ReadOnlyDatabase.cpp" "${snapshot}/original/DataScope.cpp"
        "${snapshot}/instrumented/VulkanStructHydrator.cpp")
    target_compile_features(ngm_descriptor_helper_2026_3 PRIVATE cxx_std_20)
    target_include_directories(ngm_descriptor_helper_2026_3 SYSTEM PUBLIC
        "${snapshot}/instrumented" "${snapshot}/original")
    target_include_directories(ngm_descriptor_helper_2026_3 PUBLIC "${snapshot}/support")
    target_link_libraries(ngm_descriptor_helper_2026_3 PUBLIC Vulkan::Headers)
    add_executable(ngm-descriptor-worker-2026_3 EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/src/inspection/DescriptorWorker.cpp")
    target_include_directories(ngm-descriptor-worker-2026_3 PRIVATE "${snapshot}")
    target_link_libraries(ngm-descriptor-worker-2026_3 PRIVATE ngm_core ngm_worker_confinement
        ngm_descriptor_helper_2026_3 nlohmann_json::nlohmann_json)
    ngm_first_party(ngm-descriptor-worker-2026_3)
endfunction()
