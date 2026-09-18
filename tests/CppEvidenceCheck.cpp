#include "Check.hpp"
#include "CppEvidenceFixture.hpp"
#include "ngm/CppEvidence.hpp"
#include "ngm/Inspection.hpp"

#include <string>
#include <vector>

namespace {
using ngm::check::require;
using Json = nlohmann::json;
using namespace ngm::check::cpp;
std::string replace(std::string s, const std::string& from, const std::string& to) {
    const auto at = s.find(from);
    require(at != std::string::npos, "mutation anchor missing");
    s.replace(at, from.size(), to);
    return s;
}
Json inspect(const std::string& commands, const std::string& objects = resources) {
    return ngm::inspect_cpp_draws({{"CommandList00.cpp", commands}, {"Resources00.cpp", objects}});
}
void rejected(const std::string& commands, const std::string& objects = resources) {
    try {
        const auto result = inspect(commands, objects);
        require(!result["unsupported_recordings"].empty() || !result["unsupported_objects"].empty() ||
                    !result["draws"].empty(),
                "missing explicit unsupported coverage");
        for(const auto& entry : result["draws"])
            require(entry["association_status"] == "unavailable", "unsupported input resolved an association");
    } catch(const ngm::InspectionError&) {
        // Global malformed/over-limit inputs fail the whole parse explicitly.
    }
}
} // namespace

int main() {
    return ngm::check::run([] {
        const auto command = recording(begin + draw + end);
        const auto result = inspect(command);
        require(result["unsupported_recordings"].empty() && result["unsupported_objects"].empty(),
                "base grammar unsupported");
        require(result["draws"].size() == 1, "one literal draw expected");
        const auto& entry = result["draws"][0];
        require(entry["event_index"] == 18 && entry["pipeline_bind"]["event_index"] == 16 &&
                    entry["pipeline"]["symbol"] == "VkPipeline_uid_38" &&
                    entry["association_status"] == "resolved_source_relationship",
                "draw/pipeline link incorrect");
        const auto& stage = entry["pipeline"]["stages"][0];
        require(stage["module"] == "VkShaderModule_uid_36" && stage["resource_handle"] == 14 &&
                    stage["declared_bytes"] == 128 && stage["entry_point_expression"] == "\"main\"",
                "shader resource reference incorrect");
        require(entry["source"]["start_line"].get<unsigned>() >
                    entry["pipeline_bind"]["source"]["start_line"].get<unsigned>(),
                "source line order incorrect");
        // A later direct binding wins; missing creation stays explicit.
        const auto rebound = inspect(recording(begin +
                                               "NV_VK_REPLAY(VulkanReplay_CmdBindPipeline(cmdBuffer, "
                                               "VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline_uid_99)); // Event #17\n" +
                                               draw + end));
        require(rebound["draws"][0]["pipeline_bind"]["symbol"] == "VkPipeline_uid_99" &&
                    rebound["draws"][0]["association_status"] == "unavailable",
                "last bind not respected");
        const auto no_bind =
            inspect(recording(replace(begin,
                                      "NV_VK_REPLAY(VulkanReplay_CmdBindPipeline(cmdBuffer, "
                                      "VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline_uid_38)); // Event #16\n",
                                      "") +
                              draw + end));
        require(no_bind["draws"][0]["pipeline_bind"].is_null(), "binding leaked into recording");
        const auto indirect =
            inspect(replace(command, "VulkanReplay_CmdDraw(cmdBuffer, 3u, 1u, 0u, 0u)",
                            "VulkanReplay_CmdDrawIndirect(cmdBuffer, VkBuffer_uid_99, 0ull, 1u, 16u)"));
        require(indirect["draws"][0]["association_status"] == "resolved_source_relationship",
                "indirect source link missing");
        // Decoys in comments and strings cannot manufacture commands or declarations.
        const auto decoy = inspect("// NV_VK_REPLAY(VulkanReplay_CmdDraw());\n" + command,
                                   replace(resources, "\"main\"", "\"not_a_call(1, 2)\""));
        require(decoy["draws"].size() == 1 &&
                    decoy["draws"][0]["pipeline"]["stages"][0]["entry_point_expression"] == "\"not_a_call(1, 2)\"",
                "quoted commas/comment decoys corrupted parse");
        // Unrelated platform branches in Resources do not hide direct supported blocks.
        require(inspect(command, "#if PLATFORM\nvoid unrelated() {}\n#endif\n" +
                                     resources)["draws"][0]["association_status"] == "resolved_source_relationship",
                "unrelated preprocessor branch rejected");
        rejected(recording(begin + "if (condition) {\n" + draw + "}\n" + end));
        rejected(replace(
            command, "3u, 1u",
            "(VulkanReplay_CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline_uid_99), 3u), 1u"));
        rejected(replace(command, "3u, 1u", "(VkPipeline_uid_38 = VkPipeline_uid_99), 1u"));
        rejected(replace(command, "3u, 1u", "VkUnknownHelper(), 1u"));
        rejected(replace(command, "CmdDraw(cmdBuffer", "CmdDraw(otherBuffer"));
        rejected(replace(command, "// Event #18", "// Event #16"));
        rejected(replace(command, "// Event #18", "// no event"));
        rejected(command + "#define VkPipeline_uid_38 VkPipeline_uid_99\n");
        rejected("# if 0\n" + command + "# endif\n");
        rejected(command, "# if 0\n" + resources + "# endif\n");
        rejected(replace(command, "BEGIN_DATA_SCOPE_FUNCTION();", "hidden_helper();"));
        rejected(command + "void Record_VkCommandBuffer_uid_42_r1_Part01(VkCommandBuffer cmdBuffer) {}\n");
        rejected(command.substr(0, command.find("void Record_VkCommandBuffer_uid_42_r1(VkCommandBuffer")));
        rejected(command, resources + resources);
        rejected(command,
                 replace(resources, "{ VkGraphicsPipelineCreateInfo{", "{ condition ? VkGraphicsPipelineCreateInfo{"));
        rejected(command, replace(resources, "VkGraphicsPipelineCreateInfo_temp_1[1]",
                                  "VkGraphicsPipelineCreateInfo_temp_1[2]"));
        rejected(command, replace(resources, "VkPipelineShaderStageCreateInfo_stages[1]",
                                  "VkPipelineShaderStageCreateInfo_stages[2]"));
        rejected(command, replace(resources, "VK_SHADER_STAGE_FRAGMENT_BIT", "VK_SHADER_STAGE_COMPUTE_BIT"));
        rejected(command, replace(resources, "14, 128ull", "14, 124ull"));
        rejected(command, replace(resources, "14, 128ull", "014, 128ull"));
        rejected(command, replace(resources, "14, 128ull", "14uUlL, 128ull"));
        rejected(command, replace(resources, "14, 128ull", "18446744073709551616, 128ull"));
        rejected(command,
                 "( a b ) { { NV_VK_REPLAY(VulkanReplay_CreateShaderModule(d, p, a, &VkShaderModule_uid_36)); } }");
        rejected(command, "NV_VK_REPLAY(VulkanReplay_CreateShaderModule(d, p, a, &VkShaderModule_uid_36))");
        rejected(command + "const char* decoy = R\"(raw \"quoted\" text)\";");
        rejected(command, replace(resources, "\"main\"", "\"main\"_side_effect"));
        rejected(command + "/* unterminated");
        rejected(command + std::string(1, '\0'));
        rejected(command + std::string(1, static_cast<char>(0xff)));
        rejected("#include \"" + std::string(65537, 'x') + "\"\n" + command);
        // Complete records from an earlier part of a malformed function must not leak.
        rejected(recording(begin + draw + "unknown();\n" + end));
        // Unsupported alternate outputs must invalidate earlier apparent definitions.
        const auto alternate = replace(resources, "&VkShaderModule_uid_36", "&(VkShaderModule_uid_36)");
        rejected(command, resources + alternate);
        rejected(command, replace(resources, "void CreateResources00() {",
                                  "void CreateResources00() { VkShaderModule VkShaderModule_uid_36;"));
        rejected(command,
                 replace(resources, "void CreateResources00() {", "void CreateResources00() { hidden_helper();"));
        auto conditional =
            replace(resources, "{ VkGraphicsPipelineCreateInfo{", "{ condition ? VkGraphicsPipelineCreateInfo{");
        conditional =
            replace(conditional, "VkPipeline(VK_NULL_HANDLE), 0} };", "VkPipeline(VK_NULL_HANDLE), 0} : other };");
        rejected(command, conditional);
        rejected(command, replace(resources, "void CreateResources00() {",
                                  "void CreateResources00() { { VkPipeline_uid_38 = VkPipeline_uid_99; }"));
        rejected(command,
                 replace(resources, "void CreateResources00() {", "void CreateResources00() { { hidden_helper(); }"));
        rejected(recording(begin + "// continued comment \\\n" + draw + end));
        rejected(recording(begin + "// continued comment \\\r\n" + draw + end));
        rejected(command, replace(resources, "void CreateResources00() {",
                                  "void CreateResources00() { { UnknownSideEffectHelper{}; }"));
        rejected(command, replace(resources, "void CreateResources00() {",
                                  "void CreateResources00() { { UnknownSideEffectHelper instance; }"));
        rejected(command, replace(resources, "void CreateResources00() {",
                                  "void CreateResources00() { { *hidden_pointer = other_value; }"));
        rejected(command,
                 replace(resources, "void CreateResources00() {",
                         "void CreateResources00() { { VkPipeline_uid_38 = VkPipeline_uid_99; "
                         "NV_VK_REPLAY(VulkanReplay_CreateShaderModule(d, p, a, &VkShaderModule_uid_100)); }"));
        require(inspect(command, replace(resources, "void CreateResources00() {",
                                         "void CreateResources00() { NV_MESSAGE_VERBOSE(\"Initializing resources\");"))
                        ["draws"][0]["association_status"] == "resolved_source_relationship",
                "literal generated resource progress message rejected");
        require(inspect(command, replace(resources, "void CreateResources00() {",
                                         "void CreateResources00() { {\n#if PLATFORM\n{ NV_MESSAGE_VERBOSE(\"platform "
                                         "setup\"); }\n#endif\n}"))["draws"][0]["association_status"] ==
                    "resolved_source_relationship",
                "nested unrelated platform setup rejected");
        const std::string window_setup = R"cpp(void CreateResources00() {
            { static VkXcbSurfaceCreateInfoKHR VkXcbSurfaceCreateInfoKHR_temp[1] = {
                VkXcbSurfaceCreateInfoKHR{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr,
                VkXcbSurfaceCreateFlagsKHR(0), (*reinterpret_cast<xcb_connection_t**>(WindowSystemInstance().GetDisplayAddr())),
                (*reinterpret_cast<xcb_window_t*>(WindowSystemInstance().GetWindowAddr()))}}; }
        )cpp";
        require(inspect(command, replace(resources, "void CreateResources00() {",
                                         window_setup))["draws"][0]["association_status"] ==
                    "resolved_source_relationship",
                "qualified XCB surface handle expressions rejected");
        rejected(command, replace(resources, "void CreateResources00() {",
                                  replace(window_setup, "GetDisplayAddr()", "hidden_helper()")));
        // A later local declaration does not identify a value used earlier.
        auto late = resources;
        const auto declaration_start = late.find("        static VkGraphicsPipelineCreateInfo");
        const auto declaration_end =
            late.find("        NV_VK_REPLAY(VulkanReplay_CreateGraphicsPipelines", declaration_start);
        const auto declaration_text = late.substr(declaration_start, declaration_end - declaration_start);
        late.erase(declaration_start, declaration_end - declaration_start);
        const auto call_end = late.find("));", declaration_start) + 3;
        late.insert(call_end, "\n" + declaration_text);
        rejected(command, late);
        auto late_stages = resources;
        const auto stages_start = late_stages.find("        static VkPipelineShaderStageCreateInfo");
        const auto stages_end = late_stages.find("        static VkGraphicsPipelineCreateInfo", stages_start);
        const auto stages_text = late_stages.substr(stages_start, stages_end - stages_start);
        late_stages.erase(stages_start, stages_end - stages_start);
        late_stages.insert(late_stages.find("        NV_VK_REPLAY(VulkanReplay_CreateGraphicsPipelines"), stages_text);
        rejected(command, late_stages);
        for(const auto& inputs : std::vector<std::vector<ngm::CppSource>>{
                {{"CommandList00.cpp", command}, {"CommandList00.cpp", command}},
                {{"CommandList00.cpp", command}, {"CommandList01.cpp", command}},
                {{std::string(4097, 'x') + "/CommandList00.cpp", command}},
                {{std::string(1, static_cast<char>(0xff)) + "/CommandList00.cpp", command}}}) {
            bool failed = false;
            try {
                (void)ngm::inspect_cpp_draws(inputs);
            } catch(const ngm::InspectionError&) {
                failed = true;
            }
            require(failed, "invalid path or duplicate source/function accepted");
        }
        // Repeated evidence cannot expand beyond a global derived-result budget.
        std::string many;
        for(unsigned i = 0; i < 10001; ++i)
            many += replace(draw, "#18", "#" + std::to_string(100 + i));
        rejected(recording(begin + many + end));
        // Byte budget is independent of record count; long source paths repeat in every span.
        std::string medium;
        for(unsigned i = 0; i < 2000; ++i)
            medium += replace(draw, "#18", "#" + std::to_string(100 + i));
        bool byte_limit = false;
        try {
            const auto prefix = std::string(4000, 'x') + "/";
            (void)ngm::inspect_cpp_draws({{prefix + "CommandList00.cpp", recording(begin + medium + end)},
                                          {prefix + "Resources00.cpp", resources}});
        } catch(const ngm::InspectionError& error) {
            byte_limit = error.code() == ngm::InspectionErrorCode::LimitExceeded;
        }
        require(byte_limit, "derived byte budget did not fail before record budget");
        std::string invalid_objects;
        for(unsigned i = 0; i < 10001; ++i)
            invalid_objects += "NV_VK_REPLAY(VulkanReplay_CreateShaderModule(d,p,a,unknown));\n";
        rejected(command, invalid_objects);
    });
}
