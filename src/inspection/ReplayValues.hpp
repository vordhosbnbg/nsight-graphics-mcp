#pragma once

#include "CppReplay.hpp"
#include <charconv>
#include <stdexcept>
#include <string_view>

namespace ngm::detail {
struct Unqualified : std::runtime_error {
    using std::runtime_error::runtime_error;
};
inline void require(bool condition, const char* reason) {
    if(!condition)
        throw Unqualified(reason);
}
inline std::uint64_t number(std::string text) {
    const auto end = text.find_first_not_of("0123456789");
    if(end != std::string::npos) {
        const auto suffix = text.substr(end);
        require(suffix == "u" || suffix == "U" || suffix == "ull" || suffix == "ULL", "nonliteral integer");
        text.resize(end);
    }
    std::uint64_t result = 0;
    const auto [last, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    require(!text.empty() && (text.size() == 1 || text[0] != '0') && error == std::errc{} &&
                last == text.data() + text.size(),
            "invalid unsigned decimal");
    return result;
}
inline std::uint64_t number(const ReplayValue& value) {
    return number(value.text);
}
inline const ReplayValue& unwrap(const ReplayValue& value, std::string_view type) {
    if(value.constructor == type) {
        require(value.arguments.size() == 1, "invalid scalar cast");
        return value.arguments[0];
    }
    return value;
}
inline std::string symbol(const ReplayValue& value, const std::string& type) {
    const auto prefix = type + "_uid_";
    require(value.text.starts_with(prefix), "expected capture-local object symbol");
    (void)number(value.text.substr(prefix.size()));
    return value.text;
}
inline std::vector<ReplayValue> array(const ReplayValue& reference, const ReplayDeclarations& values,
                                      const std::string& type, std::size_t count) {
    require(count > 0 && count <= 64, "array count outside qualified bounds");
    std::vector<ReplayValue> result;
    if(reference.constructor == "NV_TO_ARRAY")
        result = reference.arguments;
    else if(reference.constructor == "NV_TO_STACK_PTR") {
        result = reference.arguments;
    } else if(reference.constructor == type)
        result = {reference};
    else {
        const auto found = values.find(reference.text);
        require(found != values.end() && found->second.type == type && found->second.end < reference.position,
                "missing, forward or mismatched declaration");
        result = found->second.elements;
    }
    require(result.size() == count, "array extent/count mismatch");
    return result;
}
inline std::vector<ReplayValue> fields(const ReplayValue& reference, const ReplayDeclarations& values,
                                       const std::string& type, std::size_t count) {
    const auto value = array(reference, values, type, 1)[0];
    require(value.constructor == type && value.arguments.size() == count, "unsupported aggregate shape");
    return value.arguments;
}
inline void structure(const std::vector<ReplayValue>& f, const std::string& type, const std::string& flags = {}) {
    require(f.size() >= 2 && f[0].text == type && f[1].text == "nullptr", "unsupported structure type or pNext");
    if(!flags.empty())
        require(f.size() >= 3 && number(unwrap(f[2], flags)) == 0, "unsupported structure flags");
}
// Shared literal submission grammar. No semaphore completion or GPU ordering
// beyond the recorded command-buffer list is inferred here.
inline std::vector<ReplayValue> submitted_buffers(const ReplayValue& info, const ReplayDeclarations& values,
                                                  bool sync2) {
    const auto& f = info.arguments;
    require(info.constructor == (sync2 ? "VkSubmitInfo2" : "VkSubmitInfo") && f.size() == 9, "invalid submit info");
    structure(f, sync2 ? "VK_STRUCTURE_TYPE_SUBMIT_INFO_2" : "VK_STRUCTURE_TYPE_SUBMIT_INFO",
              sync2 ? "VkSubmitFlags" : "");
    const auto count = number(f[5]);
    require(count <= 64 && (count != 0 || f[6].text == "nullptr"), "unqualified submitted buffer count");
    if(!sync2)
        return count ? array(f[6], values, "VkCommandBuffer", count) : std::vector<ReplayValue>{};
    for(const auto at : {3, 7}) {
        const auto size = number(f[at]);
        require(size <= 64 && (size != 0 || f[at + 1].text == "nullptr"), "unqualified semaphore submit count");
        if(size)
            for(const auto& semaphore : array(f[at + 1], values, "VkSemaphoreSubmitInfo", size)) {
                require(semaphore.constructor == "VkSemaphoreSubmitInfo" && semaphore.arguments.size() == 6,
                        "invalid semaphore submit info");
                structure(semaphore.arguments, "VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO");
                (void)symbol(semaphore.arguments[2], "VkSemaphore");
                require(number(semaphore.arguments[3]) == 0 && number(semaphore.arguments[5]) == 0,
                        "timeline or device-group semaphore submit unsupported");
                require(semaphore.arguments[4].constructor == "VkPipelineStageFlags2" &&
                            semaphore.arguments[4].arguments.size() == 1,
                        "semaphore stage mask must be an explicit flags2 value");
            }
    }
    std::vector<ReplayValue> buffers;
    if(count)
        for(const auto& buffer : array(f[6], values, "VkCommandBufferSubmitInfo", count)) {
            require(buffer.constructor == "VkCommandBufferSubmitInfo" && buffer.arguments.size() == 4,
                    "invalid command buffer submit info");
            structure(buffer.arguments, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO");
            require(number(buffer.arguments[3]) == 0, "device-group command buffer submit unsupported");
            (void)symbol(buffer.arguments[2], "VkCommandBuffer");
            buffers.push_back(buffer.arguments[2]);
        }
    return buffers;
}
} // namespace ngm::detail
