#include "Check.hpp"
#include "CppReplay.hpp"
#include "DescriptorState.hpp"
#include "DescriptorStateFixture.hpp"
#include "ReplayValues.hpp"
#include "ValueDescriptors.hpp"

namespace {
using ngm::check::require;
using namespace ngm::detail;
using Json = nlohmann::json;
const std::string initial = "void SetInitialFrameState00() { BEGIN_DATA_SCOPE_FUNCTION(); "
                            "NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));"
                            "NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1)); }";
std::vector<ngm::CppSource> inputs() {
    auto sources = ngm::check::descriptor::sources<ngm::CppSource>();
    sources.push_back({"FrameSetup00.cpp", initial});
    sources[1].text += R"cpp(
void CreateResourcesPool() {
 {
 static VkDescriptorPoolSize VkDescriptorPoolSize_info[2] = {
 VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,10u},
 VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,5u}};
 static VkDescriptorPoolCreateInfo VkDescriptorPoolCreateInfo_info[1] = {
 VkDescriptorPoolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,nullptr,
 VkDescriptorPoolCreateFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT),5u,2u,VkDescriptorPoolSize_info}};
 NV_VK_REPLAY(VulkanReplay_CreateDescriptorPool(VkDevice_uid_4,VkDescriptorPoolCreateInfo_info,nullptr,&VkDescriptorPool_uid_89));
 }
}
)cpp";
    return sources;
}
ReplayCall call(const std::string& statement) {
    const auto replay = parse_cpp_replay({{"FrameSetup00.cpp", initial},
                                          {"Frame0Part00.cpp", "void RunFrame0Part00(uint64_t frameIndex) { "
                                                               "BEGIN_DATA_SCOPE_FUNCTION();\n" +
                                                                   statement + "\n}"}},
                                         ReplayParseScope::Values);
    require(replay.frame.size() == 1, "one parsed descriptor source call");
    return replay.frame[0];
}
std::string copy(unsigned src, unsigned dst, unsigned src_element = 0, unsigned dst_element = 0,
                 unsigned src_binding = 2, unsigned dst_binding = 2, unsigned count = 1) {
    return "VkCopyDescriptorSet{VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,nullptr,VkDescriptorSet_uid_" +
           std::to_string(src) + "," + std::to_string(src_binding) + "u," + std::to_string(src_element) +
           "u,VkDescriptorSet_uid_" + std::to_string(dst) + "," + std::to_string(dst_binding) + "u," +
           std::to_string(dst_element) + "u," + std::to_string(count) + "u}";
}
ReplayCall update(unsigned writes, const std::vector<std::string>& copies = {}) {
    std::string text;
    for(const auto& c : copies)
        text += (text.empty() ? "" : ",") + c;
    return call("NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4," + std::to_string(writes) +
                "u,Serialization::DATABASE_HANDLE(11)," + (writes ? "88ull" : "0ull") + "," +
                std::to_string(copies.size()) + "u," + (copies.empty() ? "nullptr" : "NV_TO_ARRAY(" + text + ")") +
                "));");
}
Json write(unsigned destination, const std::vector<unsigned>& buffers, unsigned first = 0) {
    Json values = Json::array();
    for(const auto b : buffers)
        values.push_back({{"buffer", {{"symbol", "VkBuffer_uid_" + std::to_string(b)}, {"serialized_id", b + 900}}},
                          {"offset", 16},
                          {"range", 16}});
    return {{"dst_set",
             {{"symbol", "VkDescriptorSet_uid_" + std::to_string(destination)}, {"serialized_id", destination + 800}}},
            {"binding", 2},
            {"array_element", first},
            {"descriptor_type", 8},
            {"descriptor_count", buffers.size()},
            {"buffers", values},
            {"images", Json::array()}};
}
template <class F>
void refuses(F&& run) {
    bool rejected = false;
    try {
        run();
    } catch(const Unqualified&) {
        rejected = true;
    }
    require(rejected, "unknown or contradictory descriptor lifetime refuses the composition");
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto allocations = value_descriptor_allocations(parse_cpp_replay(inputs(), ReplayParseScope::Values));
        require(allocations.at("sets").size() == 5 &&
                    allocations.at("sets").at("VkDescriptorSet_uid_100").at("pool") == "VkDescriptorPool_uid_89" &&
                    allocations.at("sets").at("VkDescriptorSet_uid_100").at("bindings")[0].at("binding") == 2,
                "unique captured pool, layout and allocation facts retain sorted bindings");
        ValueDescriptors state(allocations);
        {
            ValueDescriptors sentinel(allocations);
            sentinel.update(update(1), Json::array({write(100, {7})}));
            const auto operation = [&](unsigned count, const std::vector<std::string>& copies) {
                auto text = update(count, copies);
                text.arguments[2].arguments[0].text = "- 1";
                return text;
            };
            const auto actual = call("NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4,0u,"
                                     "Serialization::DATABASE_HANDLE(-1),0ull,1u,NV_TO_ARRAY(" +
                                     copy(100, 101) + ")));");
            require(actual.arguments[2].arguments[0].text == "- 1",
                    "exact exported invalid-handle sentinel is syntax only");
            sentinel.update(actual, Json::array());
            require(sentinel.element("VkDescriptorSet_uid_101", 2, 0).at("value").at("buffer").at("symbol") ==
                        "VkBuffer_uid_7",
                    "copy-only sentinel performs literal copies without reading a database entry");
            refuses([&] { sentinel.update(operation(1, {copy(100, 101)}), Json::array({write(100, {9})})); });
            refuses([&] { sentinel.update(operation(0, {}), Json::array()); });
        }
        state.update(update(1, {copy(100, 101), copy(101, 102, 0, 1)}), Json::array({write(100, {7, 9})}),
                     {{"artifact_id", "private-authenticated-write-test"}});
        const auto copied = state.element("VkDescriptorSet_uid_102", 2, 1);
        require(copied.at("status") == "qualified_source_descriptor" &&
                    copied.at("value").at("buffer").at("symbol") == "VkBuffer_uid_7" &&
                    copied.at("history").size() == 3 &&
                    copied.at("history")[0].at("read").at("artifact_id") == "private-authenticated-write-test" &&
                    copied.at("history")[2].at("copy_source").contains("sha256"),
                "writes precede copies, copies run in order and retain the actual source chain");
        state.update(update(1), Json::array({write(100, {10})}));
        require(state.element("VkDescriptorSet_uid_100", 2, 0).at("value").at("buffer").at("symbol") ==
                        "VkBuffer_uid_10" &&
                    state.element("VkDescriptorSet_uid_100", 2, 1).at("value").at("buffer").at("symbol") ==
                        "VkBuffer_uid_9" &&
                    state.element("VkDescriptorSet_uid_102", 2, 1).at("value").at("buffer").at("symbol") ==
                        "VkBuffer_uid_7",
                "partial writes preserve untouched elements; copies do not follow later source updates");
        state.recorded("recording", {"VkDescriptorSet_uid_101"});
        state.update(update(1), Json::array({write(100, {11})}));
        state.require_recording("recording");
        state.update(update(0, {copy(100, 101)}), Json::array());
        refuses([&] { state.require_recording("recording"); });
        state.recorded("recording", {"VkDescriptorSet_uid_101"});
        state.submitted("recording");
        refuses([&] { state.update(update(1), Json::array({write(101, {12})})); });
        refuses([&] { state.recorded("recording", {"VkDescriptorSet_uid_101"}); });
        refuses([&] { state.forget("recording"); });
        state.completed("recording");
        state.update(update(0, {copy(103, 101, 0, 1)}), Json::array());
        const auto undefined = state.element("VkDescriptorSet_uid_101", 2, 1);
        require(undefined.at("status") == "unavailable" && undefined.at("value").is_null() &&
                    undefined.at("history").size() == 1,
                "copying undefined contents erases old destination values and retains its provenance");
        const auto control = [&](const ReplayCall& operation, const Json& writes = Json::array()) {
            ValueDescriptors altered(allocations);
            refuses([&] { altered.update(operation, writes); });
        };
        control(update(0, {copy(100, 100)}));
        control(update(0, {copy(100, 101, 0, 0, 2, 9)}));
        control(update(0, {copy(100, 101, 1, 0, 2, 2, 2)}));
        control(update(1), Json::array());
        control(update(1), Json::array({write(100, {7, 9}, 1)}));
        auto mismatched = write(100, {7});
        mismatched["descriptor_type"] = 9;
        control(update(1), Json::array({mismatched}));
        state.host(call("NV_VK_REPLAY(VulkanReplay_FreeDescriptorSets(VkDevice_uid_4,VkDescriptorPool_uid_89,"
                        "1u,NV_TO_STACK_PTR(VkDescriptorSet_uid_101)));"));
        require(state.element("VkDescriptorSet_uid_101", 2, 0).at("status") == "unavailable",
                "freed descriptor set loses its old contents");
        refuses([&] { state.update(update(1), Json::array({write(101, {7})})); });
        state.host(call("NV_VK_REPLAY(VulkanReplay_ResetDescriptorPool(VkDevice_uid_4,VkDescriptorPool_uid_89,"
                        "VkDescriptorPoolResetFlags(0)));"));
        require(state.element("VkDescriptorSet_uid_100", 2, 0).at("status") == "unavailable" &&
                    state.element("VkDescriptorSet_uid_102", 2, 1).at("status") == "unavailable",
                "pool reset invalidates every allocated set generation");
        auto short_pool = inputs();
        short_pool[1].text =
            ngm::check::state::replace(short_pool[1].text, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,10u",
                                       "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,9u");
        refuses([&] { (void)value_descriptor_allocations(parse_cpp_replay(short_pool, ReplayParseScope::Values)); });
        auto no_free = inputs();
        no_free[1].text = ngm::check::state::replace(
            no_free[1].text, "VkDescriptorPoolCreateFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)",
            "VkDescriptorPoolCreateFlags(0)");
        ValueDescriptors fixed_pool(value_descriptor_allocations(parse_cpp_replay(no_free, ReplayParseScope::Values)));
        refuses([&] {
            fixed_pool.host(call("NV_VK_REPLAY(VulkanReplay_FreeDescriptorSets(VkDevice_uid_4,"
                                 "VkDescriptorPool_uid_89,1u,NV_TO_STACK_PTR(VkDescriptorSet_uid_100)));"));
        });
        auto duplicate_pool = inputs();
        duplicate_pool[1].text += ngm::check::state::replace(
            duplicate_pool[1].text.substr(duplicate_pool[1].text.find("void CreateResourcesPool()")),
            "CreateResourcesPool", "CreateResourcesPoolAgain");
        refuses(
            [&] { (void)value_descriptor_allocations(parse_cpp_replay(duplicate_pool, ReplayParseScope::Values)); });
    });
}
