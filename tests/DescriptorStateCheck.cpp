#include "Check.hpp"
#include "DescriptorStateFixture.hpp"
#include "ngm/DrawState.hpp"

namespace {
using Json = nlohmann::json;
using ngm::check::require;
namespace ds = ngm::check::descriptor;
namespace st = ngm::check::state;
Json inspect(const std::vector<ngm::CppSource>& sources) {
    const auto parsed = ngm::inspect_draw_executions(sources);
    require(!parsed.at("executions").empty(), "descriptor execution association: " + parsed.at("unsupported").dump());
    return parsed.at("executions")[0].at("state").at("descriptors");
}
Json after(const std::string& statements) {
    return inspect(ds::sources<ngm::CppSource>(st::begin + st::bind + statements + st::draw + st::end));
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto initial = inspect(ds::sources<ngm::CppSource>());
        require(initial.at("status") == "qualified_recorded_state" && initial.at("value").size() == 3 &&
                    initial.at("coverage").at("contents").at("status") == "unavailable",
                "binding identity is available independently of descriptor contents: " + initial.dump() +
                    ngm::inspect_draw_executions(ds::sources<ngm::CppSource>()).at("unsupported").dump());
        auto stack = ds::sources<ngm::CppSource>();
        stack[0].text = st::replace(
            stack[0].text, "NV_TO_ARRAY(VkDescriptorSet_uid_100,VkDescriptorSet_uid_101,VkDescriptorSet_uid_102)",
            "NV_TO_STACK_PTR(VkDescriptorSet_uid_100,VkDescriptorSet_uid_101,VkDescriptorSet_uid_102)");
        require(inspect(stack).at("value") == initial.at("value"),
                "observed variadic stack-pointer array preserves literal element order");
        const Json expected =
            Json::array({{{"binding", 2}, {"array_element", 0}, {"offset", 220}, {"source_index", 6}},
                         {{"binding", 2}, {"array_element", 1}, {"offset", 221}, {"source_index", 7}},
                         {{"binding", 9}, {"array_element", 0}, {"offset", 290}, {"source_index", 8}}});
        require(initial.at("value")[2].at("dynamic_offsets") == expected,
                "independent offset oracle orders set, binding and array element, not declaration order");
        const auto compatible = after(ds::initial + ds::bind(5, 48, 1, {103}, {11, 12, 19}));
        require(compatible.at("status") == "qualified_recorded_state" &&
                    compatible.at("value")[0].at("descriptor_set") == "VkDescriptorSet_uid_100" &&
                    compatible.at("value")[1].at("descriptor_set") == "VkDescriptorSet_uid_103" &&
                    compatible.at("value")[2].at("descriptor_set") == "VkDescriptorSet_uid_102",
                "identically defined layouts preserve lower and higher sets during partial rebinding");
        const auto rows = [](const Json& field) { return field.at("coverage").at("sets"); };
        const auto lower = after(ds::initial + ds::bind(5, 49, 1, {101}, {11, 12, 19}));
        require(lower.at("status") == "unavailable" && lower.at("value").is_null() &&
                    rows(lower)[0].at("reason") == "descriptor set was disturbed by a later bind" &&
                    rows(lower)[1].at("reason") == "last bound layout is incompatible for this set" &&
                    rows(lower)[2].at("reason") == "descriptor set was disturbed by a later bind",
                "incompatible set zero disturbs lower set zero and higher set two");
        const auto restored =
            after(ds::initial + ds::bind(5, 49, 1, {101}, {11, 12, 19}) + ds::bind(6, 37, 0, {100}, {20, 21, 90}));
        require(rows(restored)[0].at("status") == "qualified_recorded_state" &&
                    rows(restored)[1].at("status") == "unavailable" && rows(restored)[2].at("status") == "unavailable",
                "disturbed slots retain the disturbing layout; restoring set zero cannot revive higher sets");
        const auto higher = after(ds::initial + ds::bind(5, 50, 1, {104}, {11, 12, 19}));
        require(rows(higher)[0].at("status") == "qualified_recorded_state" &&
                    rows(higher)[1].at("status") == "unavailable" && rows(higher)[2].at("status") == "unavailable",
                "matching prefix preserves lower set zero, while a changed set one invalidates higher set two");
        const auto push = after(ds::initial + ds::bind(5, 51, 2, {102}, {11, 12, 19}));
        require(rows(push)[0].at("status") == "unavailable" && rows(push)[1].at("status") == "unavailable" &&
                    rows(push)[2].at("status") == "unavailable",
                "push-range compatibility is part of descriptor compatibility");
        const auto rebound = after(
            ds::initial +
            st::replace(st::replace(st::bind, "VkPipeline_uid_38", "VkPipeline_uid_999"), "Event #3", "Event #5") +
            st::replace(st::bind, "Event #3", "Event #6"));
        require(rebound.at("value") == initial.at("value"), "pipeline binds do not disturb descriptor bindings");
        const auto compute =
            after(ds::initial + st::replace(ds::bind(5, 37, 0, {103}, {1, 2, 3}), "VK_PIPELINE_BIND_POINT_GRAPHICS",
                                            "VK_PIPELINE_BIND_POINT_COMPUTE"));
        require(compute.at("value") == initial.at("value"), "compute descriptor bind point is independent");
        const auto before_pipeline =
            inspect(ds::sources<ngm::CppSource>(st::begin + ds::initial + st::bind + st::draw + st::end));
        require(before_pipeline.at("value") == initial.at("value"),
                "descriptor binding does not require an earlier pipeline bind");
        const auto partial = after(ds::bind(4, 37, 1, {101}, {1, 2, 3}));
        require(partial.at("value").is_null() && rows(partial)[1].at("status") == "qualified_recorded_state" &&
                    rows(partial)[0].at("status") == "unavailable",
                "missing sets never become fabricated bindings");
        for(const auto& invalid : {ds::bind(4, 37, 0, {100}, {1, 2}), ds::bind(4, 37, 0, {100}, {1, 2, 3, 4}),
                                   ds::bind(4, 37, 0, {104}, {1, 2, 3}), ds::bind(4, 37, 0, {999}, {1, 2, 3})})
            require(after(invalid + ds::bind(5, 37, 0, {100}, {1, 2, 3})).at("status") == "unsupported",
                    "unknown or mismatched bind poisons descriptor coverage instead of retaining stale values");
        auto source = ds::sources<ngm::CppSource>();
        source[1].text =
            st::replace(source[1].text, "VkDescriptorSetLayoutCreateFlags(0)",
                        "VkDescriptorSetLayoutCreateFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)");
        require(inspect(source).at("status") == "unsupported",
                "update-after-bind layouts require separate qualification");
        source = ds::sources<ngm::CppSource>();
        source[1].text = st::replace(
            source[1].text,
            "VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,nullptr",
            "VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,extension");
        require(inspect(source).at("status") == "unsupported",
                "descriptor extension chains cannot inherit known coverage");
        source = ds::sources<ngm::CppSource>();
        source[1].text = st::replace(source[1].text, "uint64_t(VkDescriptorSet_uid_100),100ull",
                                     "uint64_t(VkDescriptorSet_uid_101),100ull");
        require(inspect(source).at("status") == "unsupported",
                "mismatched registration cannot establish allocation identity");
        source = ds::sources<ngm::CppSource>();
        source[1].text = st::replace(
            source[1].text, "VkDescriptorPool_uid_89,1u,NV_TO_STACK_PTR(VkDescriptorSetLayout_uid_90)",
            "VkDescriptorPool_uid_89,2u,NV_TO_ARRAY(VkDescriptorSetLayout_uid_90,VkDescriptorSetLayout_uid_90)");
        require(inspect(source).at("status") == "unsupported", "allocation extent must match output handles");
        source = ds::sources<ngm::CppSource>();
        source[1].text += R"cpp(
void CreateResourcesMulti() {
 { static VkDescriptorSetAllocateInfo VkDescriptorSetAllocateInfo_multi[1] = {
 VkDescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,nullptr,VkDescriptorPool_uid_89,
 2u,NV_TO_ARRAY(VkDescriptorSetLayout_uid_90,VkDescriptorSetLayout_uid_90)}};
 NV_VK_REPLAY(VulkanReplay_AllocateDescriptorSets(VkDevice_uid_4,VkDescriptorSetAllocateInfo_multi,
 NV_TO_OUTPUT_ARRAY(&VkDescriptorSet_uid_200,&VkDescriptorSet_uid_201))); }
}
)cpp";
        source[0].text = st::replace(
            st::replace(source[0].text, "NV_TO_ARRAY(VkDescriptorSet_uid_100,", "NV_TO_ARRAY(VkDescriptorSet_uid_200,"),
            "VkDescriptorSet_uid_101,", "VkDescriptorSet_uid_201,");
        const auto multiple = inspect(source);
        require(multiple.at("status") == "qualified_recorded_state" &&
                    multiple.at("value")[0].at("descriptor_set") == "VkDescriptorSet_uid_200" &&
                    multiple.at("value")[1].at("descriptor_set") == "VkDescriptorSet_uid_201",
                "multi-output allocation maps each output to the corresponding declared layout");
        source[1].text = st::replace(source[1].text, "&VkDescriptorSet_uid_201", "&VkDescriptorSet_uid_200");
        require(inspect(source).at("status") == "unsupported",
                "duplicate allocation outputs cannot resolve a unique set");
        source = ds::sources<ngm::CppSource>();
        source[0].text = st::replace(source[0].text, "20u,21u,90u", "4294967296ull,21u,90u");
        require(inspect(source).at("status") == "unsupported", "dynamic offsets must be uint32 values");
        source = ds::sources<ngm::CppSource>();
        for(auto& file : source) {
            for(const auto* type : {"UNIFORM", "STORAGE"}) {
                const std::string from = std::string("VK_DESCRIPTOR_TYPE_") + type + "_BUFFER_DYNAMIC";
                for(std::size_t at = 0; (at = file.text.find(from, at)) != std::string::npos;)
                    file.text.replace(at, from.size(), from.substr(0, from.size() - 8));
            }
        }
        source[0].text =
            st::replace(source[0].text, "9u,NV_TO_ARRAY(20u,21u,90u,120u,121u,190u,220u,221u,290u)", "0u,nullptr");
        require(inspect(source).at("status") == "qualified_recorded_state" &&
                    inspect(source).at("value")[0].at("dynamic_offsets").empty(),
                "ordinary layouts require no dynamic offsets");
        source = ds::sources<ngm::CppSource>(st::begin + st::bind + st::draw + st::end);
        source[1].text =
            st::replace(source[1].text, "3u,VkDescriptorSetLayout_s37,0u,nullptr", "0u,nullptr,0u,nullptr");
        require(inspect(source).at("status") == "not_applicable",
                "pipeline layout with zero sets differs from a missing descriptor bind");
    });
}
