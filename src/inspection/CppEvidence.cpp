#include "ngm/CppEvidence.hpp"
#include "ngm/Inspection.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

namespace ngm {
namespace {
using Json = nlohmann::json;
constexpr std::size_t none = static_cast<std::size_t>(-1);
struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void check(bool value, const char* reason) {
    if(!value)
        throw Unsupported(reason);
}
struct ResultBudget {
    std::size_t bytes = 1024, records = 0;
    void reserve(const Json& value) {
        bytes += value.dump().size() + 1;
        if(++records > 10000 || bytes > 16U * 1024U * 1024U)
            throw InspectionError(InspectionErrorCode::LimitExceeded, "Generated evidence result exceeds bounds");
    }
    void append(Json& array, Json value) {
        reserve(value);
        array.push_back(std::move(value));
    }
};
struct Token {
    std::string_view value;
    std::size_t begin, end, line, pair = none, brace = none;
    bool conditional = false;
};
struct Document {
    const CppSource& source;
    std::vector<Token> tokens;
    explicit Document(const CppSource& input) : source(input) {
        check(!input.path.empty() && input.path.size() <= 4096 && input.path.find('\0') == std::string::npos,
              "source path exceeds bounds or contains NUL");
        (void)Json(input.path).dump();
        check(input.text.size() <= 4U * 1024U * 1024U, "source exceeds 4 MiB");
        check(input.text.find('\0') == std::string::npos, "source contains NUL");
        check(input.text.find("\\\n") == std::string::npos && input.text.find("\\\r\n") == std::string::npos,
              "C++ line splicing is unsupported");
        (void)Json(input.text).dump(); // Reject damaged UTF-8 rather than repairing evidence.
        const std::string_view text(input.text);
        std::vector<std::size_t> stack;
        std::size_t line = 1, brace = none, conditional = 0;
        for(std::size_t i = 0; i < text.size();) {
            const auto start = i, start_line = line;
            if(std::isspace(static_cast<unsigned char>(text[i]))) {
                line += text[i++] == '\n';
                continue;
            }
            if(text.substr(i, 2) == "//") {
                while(i < text.size() && text[i] != '\n')
                    ++i;
                continue;
            }
            if(text.substr(i, 2) == "/*") {
                i += 2;
                while(i + 1 < text.size() && text.substr(i, 2) != "*/")
                    line += text[i++] == '\n';
                check(i + 1 < text.size(), "unterminated block comment");
                i += 2;
                continue;
            }
            if(text[i] == '#') {
                while(i < text.size() && text[i] != '\n')
                    ++i;
                const auto directive = text.substr(start, i - start);
                auto keyword = directive.substr(1);
                keyword.remove_prefix(std::min(keyword.find_first_not_of(" \t"), keyword.size()));
                keyword = keyword.substr(0, keyword.find_first_of(" \t\r(<\""));
                check(directive.find('\\') == std::string_view::npos, "continued preprocessor directive unsupported");
                if(keyword == "if" || keyword == "ifdef" || keyword == "ifndef") {
                    check(conditional < 64, "preprocessor nesting exceeds limit");
                    ++conditional;
                } else if(keyword == "endif") {
                    check(conditional > 0, "unmatched preprocessor condition");
                    --conditional;
                } else if(keyword == "else" || keyword == "elif")
                    check(conditional > 0, "unmatched preprocessor branch");
                else
                    check(keyword == "include", "unsupported preprocessor directive");
                check(tokens.size() < 500000 && directive.size() <= 65536, "preprocessor token limit exceeded");
                tokens.push_back({directive, start, i, line, none, brace, true});
                continue;
            }
            if(text[i] == '"' || text[i] == '\'') {
                const auto quote = text[i++];
                bool closed = false;
                while(i < text.size()) {
                    if(text[i] == '\\') {
                        check(i + 1 < text.size(), "unterminated escape");
                        line += text[i + 1] == '\n';
                        i += 2;
                    } else if(text[i++] == quote) {
                        closed = true;
                        break;
                    } else
                        check(text[i - 1] != '\n', "newline in quoted literal");
                }
                check(closed, "unterminated literal");
                check(i == text.size() || (!std::isalnum(static_cast<unsigned char>(text[i])) && text[i] != '_'),
                      "user-defined literal suffix unsupported");
            } else if(std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_') {
                while(i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
                    ++i;
                const auto identifier = text.substr(start, i - start);
                check(i == text.size() || text[i] != '"' ||
                          (identifier != "R" && identifier != "u8R" && identifier != "uR" && identifier != "UR" &&
                           identifier != "LR"),
                      "raw C++ string literal unsupported");
            } else {
                ++i;
            }
            check(i - start <= 65536, "source token exceeds 64 KiB");
            check(tokens.size() < 500000, "source token count exceeds limit");
            const auto n = tokens.size();
            tokens.push_back({text.substr(start, i - start), start, i, start_line, none, brace, conditional != 0});
            const auto v = tokens.back().value;
            if(v == "(" || v == "[" || v == "{") {
                check(stack.size() < 64, "source delimiter nesting exceeds limit");
                stack.push_back(n);
                if(v == "{")
                    brace = n;
            } else if(v == ")" || v == "]" || v == "}") {
                check(!stack.empty(), "unmatched closing delimiter");
                const auto opening = stack.back();
                stack.pop_back();
                check((tokens[opening].value == "(" && v == ")") || (tokens[opening].value == "[" && v == "]") ||
                          (tokens[opening].value == "{" && v == "}"),
                      "mismatched delimiters");
                tokens[opening].pair = n;
                tokens[n].pair = opening;
                if(v == "}")
                    brace = tokens[opening].brace;
            }
        }
        check(stack.empty() && conditional == 0, "unclosed source delimiter or preprocessor condition");
    }
    Json span(std::size_t first, std::size_t last) const {
        return {{"path", source.path}, {"start_line", tokens.at(first).line}, {"end_line", tokens.at(last).line}};
    }
    std::string expression(std::size_t first, std::size_t last) const {
        std::string result;
        for(auto i = first; i < last; ++i) {
            if(!result.empty())
                result += ' ';
            result += tokens[i].value;
            check(result.size() <= 16384, "expression exceeds 16 KiB");
        }
        return result;
    }
};
using Range = std::pair<std::size_t, std::size_t>;
std::vector<Range> arguments(const Document& d, std::size_t opening) {
    const auto& t = d.tokens;
    check(t.at(opening).pair != none, "argument list lacks closing delimiter");
    const auto closing = t[opening].pair;
    std::vector<Range> out;
    auto start = opening + 1;
    for(auto i = start; i < closing; ++i) {
        if(t[i].value == ",") {
            check(i > start, "empty argument");
            out.emplace_back(start, i);
            start = i + 1;
        } else if(t[i].value == "(" || t[i].value == "[" || t[i].value == "{")
            i = t[i].pair;
    }
    if(start < closing)
        out.emplace_back(start, closing);
    check(out.size() <= 64, "too many initializer/call arguments");
    return out;
}
std::string expr(const Document& d, Range r) {
    return d.expression(r.first, r.second);
}
std::uint64_t integer(std::string_view s) {
    const auto end = s.find_first_not_of("0123456789");
    if(end != std::string_view::npos) {
        const auto suffix = s.substr(end);
        check(suffix == "u" || suffix == "U" || suffix == "l" || suffix == "L" || suffix == "ul" || suffix == "UL" ||
                  suffix == "ull" || suffix == "ULL" || suffix == "ll" || suffix == "LL",
              "unsupported integer suffix");
        s = s.substr(0, end);
    }
    check(s.size() == 1 || !s.starts_with('0'), "noncanonical decimal literal");
    std::uint64_t n = 0;
    auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
    check(!s.empty() && e == std::errc{} && p == s.data() + s.size(), "expected bounded literal unsigned decimal");
    return n;
}
std::string uid(std::string value, std::string_view type) {
    check(value.starts_with(type), "expected literal capture-local object symbol");
    const auto number = std::string_view(value).substr(type.size());
    check(!number.empty() && number.find_first_not_of("0123456789") == std::string_view::npos,
          "object identifier suffix must contain only decimal digits");
    (void)integer(number);
    return value;
}
struct Call {
    std::size_t at, end;
    std::string name;
    std::vector<Range> args;
    std::uint64_t event = 0;
};
Call call(const Document& d, std::size_t at) {
    const auto& t = d.tokens;
    check(t.at(at).value == "NV_VK_REPLAY" && t.at(at + 1).value == "(" && t.at(at + 3).value == "(",
          "expected replay call");
    const auto inner_end = t[at + 3].pair, outer_end = t[at + 1].pair;
    check(inner_end + 1 == outer_end && t.at(outer_end + 1).value == ";", "unsupported replay expression");
    return {at, outer_end + 1, std::string(t[at + 2].value), arguments(d, at + 3), 0};
}
// These are the value/cast helpers observed in qualified recording/creation
// blocks. This is deliberately a generated-source grammar, not a C++ evaluator.
const std::set<std::string_view> value_types{"uint32_t",
                                             "VkCommandBufferBeginInfo",
                                             "VkCommandBufferUsageFlags",
                                             "VkDebugUtilsLabelEXT",
                                             "VkClearValue",
                                             "VkRenderPassBeginInfo",
                                             "VkRect2D",
                                             "VkOffset2D",
                                             "VkExtent2D",
                                             "VkMemoryBarrier",
                                             "VkBufferMemoryBarrier",
                                             "VkImageMemoryBarrier",
                                             "VkAccessFlags",
                                             "VkImageSubresourceRange",
                                             "VkImageAspectFlags",
                                             "VkPipelineStageFlags",
                                             "VkDependencyFlags",
                                             "VkShaderStageFlags",
                                             "VkShaderModuleCreateInfo",
                                             "VkShaderModuleCreateFlags",
                                             "VkPipelineShaderStageCreateInfo",
                                             "VkPipelineShaderStageCreateFlags",
                                             "VkPipelineVertexInputStateCreateInfo",
                                             "VkPipelineVertexInputStateCreateFlags",
                                             "VkPipelineInputAssemblyStateCreateInfo",
                                             "VkPipelineInputAssemblyStateCreateFlags",
                                             "VkViewport",
                                             "VkPipelineViewportStateCreateInfo",
                                             "VkPipelineViewportStateCreateFlags",
                                             "VkPipelineRasterizationStateCreateInfo",
                                             "VkPipelineRasterizationStateCreateFlags",
                                             "VkCullModeFlags",
                                             "VkPipelineMultisampleStateCreateInfo",
                                             "VkPipelineMultisampleStateCreateFlags",
                                             "VkPipelineColorBlendAttachmentState",
                                             "VkColorComponentFlags",
                                             "VkPipelineColorBlendStateCreateInfo",
                                             "VkPipelineColorBlendStateCreateFlags",
                                             "VkGraphicsPipelineCreateInfo",
                                             "VkPipelineCreateFlags",
                                             "VkPipeline",
                                             "VkPipelineCache"};
void value_expression(const Document& d, Range r) {
    const auto& t = d.tokens;
    for(auto i = r.first; i < r.second; ++i) {
        const auto v = t[i].value;
        check(!t[i].conditional && !v.starts_with('#'), "conditional expression unsupported");
        check(v != "=" && v != "?" && v != ":" && v != ";" && v != "[" && v != "]" && v != "+" && v != "\\",
              "unsupported expression operator");
        if(v == "-")
            check(i + 1 < r.second && std::isdigit(static_cast<unsigned char>(t[i + 1].value.front())),
                  "unsupported minus expression");
        if(v == ".")
            check(i > r.first && i + 1 < r.second && std::isdigit(static_cast<unsigned char>(t[i - 1].value.front())) &&
                      std::isdigit(static_cast<unsigned char>(t[i + 1].value.front())),
                  "member access unsupported");
        if(i + 1 < r.second && t[i + 1].value == "(") {
            check(value_types.contains(v) || v == "HexToFloat" || v == "HexToDouble" || v == "NV_TO_ARRAY" ||
                      v == "NV_TO_UNION" || v == "NV_GET_RESOURCE_CHECKED" || v == "NV_GET_RESOURCE_STATIC" ||
                      v == "NV_TO_STACK_PTR" || v == "NV_TO_OUTPUT_ARRAY",
                  "unrecognized expression helper");
        }
        if(i + 1 < r.second && t[i + 1].value == "{")
            check(value_types.contains(v), "unrecognized aggregate type");
    }
}
struct Declaration {
    std::string type, name;
    std::vector<std::size_t> constructors;
    Range scope;
    std::uint64_t extent = 0;
};
Declaration declaration(const Document& d, std::size_t at, std::size_t& next) {
    const auto& t = d.tokens;
    check(t.at(at).value == "static" && value_types.contains(t.at(at + 1).value), "unsupported declaration type");
    Declaration result{std::string(t.at(at + 1).value), std::string(t.at(at + 2).value), {}, {at, at}, 0};
    check(result.name.starts_with(result.type + "_") && result.name.find("_uid_") == std::string::npos,
          "unsupported or shadowing declaration name");
    auto i = at + 3;
    if(t.at(i).value == "[") {
        check(t[i].pair == i + 2, "declaration requires literal array extent");
        result.extent = integer(t.at(i + 1).value);
        check(result.extent > 0 && result.extent <= 65536, "array extent out of bounds");
        i += 3;
    }
    if(t.at(i).value == "=") {
        check(result.extent != 0 && t.at(i + 1).value == "{", "expected array aggregate initializer");
        const auto opening = i + 1;
        i = t[opening].pair + 1;
        for(const auto element : arguments(d, opening)) {
            value_expression(d, element);
            if(t[element.first].value == result.type && element.first + 1 < element.second &&
               t[element.first + 1].value == "{" && t[element.first + 1].pair + 1 == element.second)
                result.constructors.push_back(element.first + 1);
        }
        check(arguments(d, opening).size() == result.extent, "array initializer/extent mismatch");
    }
    check(t.at(i).value == ";", "unsupported declaration form");
    result.scope.second = i;
    next = i + 1;
    return result;
}
struct Body {
    std::map<std::string, Declaration> declarations;
    std::vector<Call> calls;
};
Body body(const Document& d, std::size_t opening, bool recording) {
    const auto& t = d.tokens;
    const auto end = t.at(opening).pair;
    Body result;
    check(!t[opening].conditional, "conditional block unsupported");
    for(auto i = opening + 1; i < end;) {
        check(!t[i].conditional && !t[i].value.starts_with('#'), "conditional source is unsupported");
        if(t[i].value == "BEGIN_DATA_SCOPE_FUNCTION" || t[i].value == "BEGIN_DATA_SCOPE") {
            check(t.at(i + 1).value == "(" && t.at(i + 2).value == ")" && t.at(i + 3).value == ";",
                  "unsupported data-scope macro");
            i += 4;
        } else if(t[i].value == "static") {
            auto value = declaration(d, i, i);
            check(result.declarations.emplace(value.name, std::move(value)).second, "duplicate declaration");
        } else if(t[i].value == "NV_VK_REPLAY") {
            auto value = call(d, i);
            i = value.end + 1;
            for(const auto argument : value.args)
                value_expression(d, argument);
            if(recording) {
                const auto start = t[value.end].end;
                const auto newline = d.source.text.find('\n', start);
                const std::string_view suffix(d.source.text.data() + start,
                                              (newline == std::string::npos ? d.source.text.size() : newline) - start);
                const auto comment = suffix.find("// Event #");
                check(comment != std::string_view::npos &&
                          suffix.substr(0, comment).find_first_not_of(" \t\r") == std::string_view::npos,
                      "missing event annotation");
                auto number = suffix.substr(comment + 10);
                number = number.substr(0, number.find_first_not_of("0123456789"));
                value.event = integer(number);
            }
            result.calls.push_back(std::move(value));
        } else
            throw Unsupported("unrecognized executable statement, nested scope or control flow");
    }
    return result;
}
std::vector<Range> initializer(const Document& d, const Body& b, const std::string& name, const char* type,
                               std::size_t used_at) {
    auto it = b.declarations.find(name);
    check(it != b.declarations.end() && it->second.type == type && it->second.constructors.size() == 1 &&
              it->second.extent == 1 && it->second.scope.second < used_at,
          "missing or ambiguous typed initializer");
    return arguments(d, it->second.constructors[0]);
}
struct Creation {
    const Document* doc;
    std::size_t opening;
    Body content;
    Call invocation;
};
// Resource setup includes generated window-system and allocation helpers that
// are unrelated to pipeline/shader creation. Still reject unknown executable
// helpers and unmodeled references to the handles we join, including sibling
// anonymous blocks. This qualification does not interpret helper implementations.
bool qualified_resource_sibling(const Document& d, std::size_t opening) {
    const auto& t = d.tokens;
    const auto end = t[opening].pair;
    const std::set<std::string_view> helpers{"BEGIN_DATA_SCOPE",
                                             "BEGIN_DATA_SCOPE_FUNCTION",
                                             "CreateDisplay",
                                             "CreateNativeWindow",
                                             "GetDisplayAddr",
                                             "GetWindowAddr",
                                             "HexToFloat",
                                             "NV_GET_RESOURCE_CHECKED",
                                             "NV_MESSAGE_VERBOSE",
                                             "NV_TO_ARRAY",
                                             "NV_TO_OUTPUT_ARRAY",
                                             "NV_VK_REPLAY",
                                             "VK_MAKE_API_VERSION",
                                             "WindowSystemInstance",
                                             "uint64_t",
                                             "VkAttachmentDescriptionFlags",
                                             "VkBufferCreateFlags",
                                             "VkBufferUsageFlags",
                                             "VkCommandPoolCreateFlags",
                                             "VkDescriptorPoolCreateFlags",
                                             "VkDescriptorSetLayoutCreateFlags",
                                             "VkDeviceAddress",
                                             "VkDeviceCreateFlags",
                                             "VkDeviceQueueCreateFlags",
                                             "VkFenceCreateFlags",
                                             "VkFramebufferCreateFlags",
                                             "VkImageCreateFlags",
                                             "VkImageUsageFlags",
                                             "VkImageViewCreateFlags",
                                             "VkInstanceCreateFlags",
                                             "VkMemoryHeapFlags",
                                             "VkMemoryPropertyFlags",
                                             "VkPipelineLayoutCreateFlags",
                                             "VkQueueFlags",
                                             "VkRenderPassCreateFlags",
                                             "VkSampleCountFlags",
                                             "VkSamplerCreateFlags",
                                             "VkSemaphoreCreateFlags",
                                             "VkSubpassDescriptionFlags",
                                             "VkSwapchainCreateFlagsKHR",
                                             "VkSwapchainKHR",
                                             "VkXcbSurfaceCreateFlagsKHR",
                                             "VulkanHelper_AllocateAndInitializeMemory3",
                                             "VulkanHelper_CreateBuffer3",
                                             "VulkanHelper_CreateImage",
                                             "VulkanHelper_CreateImageView",
                                             "VulkanHelper_CreateSampler",
                                             "VulkanHelper_CreateWindowSystemSurface",
                                             "VulkanHelper_GetCompatiblePhysicalDevice",
                                             "VulkanHelper_RegisterObject",
                                             "VulkanHelper_ValidateImageMemoryRequirements",
                                             "VulkanReplay_AllocateCommandBuffers",
                                             "VulkanReplay_AllocateDescriptorSets",
                                             "VulkanReplay_CreateCommandPool",
                                             "VulkanReplay_CreateDescriptorPool",
                                             "VulkanReplay_CreateDescriptorSetLayout",
                                             "VulkanReplay_CreateDevice",
                                             "VulkanReplay_CreateFence",
                                             "VulkanReplay_CreateFramebuffer",
                                             "VulkanReplay_CreateGraphicsPipelines",
                                             "VulkanReplay_CreateInstance",
                                             "VulkanReplay_CreatePipelineLayout",
                                             "VulkanReplay_CreateRenderPass",
                                             "VulkanReplay_CreateSemaphore",
                                             "VulkanReplay_CreateShaderModule",
                                             "VulkanReplay_CreateSwapchainKHR",
                                             "VulkanReplay_CreateXcbSurfaceKHR",
                                             "VulkanReplay_GetDeviceQueue",
                                             "VulkanReplay_GetSwapchainImagesKHR"};
    const std::set<std::string_view> aggregate_types{"VkApplicationInfo",
                                                     "VkAttachmentDescription",
                                                     "VkAttachmentReference",
                                                     "VkBindBufferMemoryInfo",
                                                     "VkBindImageMemoryInfo",
                                                     "VkBufferCreateInfo",
                                                     "VkCommandBufferAllocateInfo",
                                                     "VkCommandPoolCreateInfo",
                                                     "VkComponentMapping",
                                                     "VkDescriptorPoolCreateInfo",
                                                     "VkDescriptorPoolSize",
                                                     "VkDescriptorSetAllocateInfo",
                                                     "VkDescriptorSetLayoutBinding",
                                                     "VkDescriptorSetLayoutCreateInfo",
                                                     "VkDeviceCreateInfo",
                                                     "VkDeviceQueueCreateInfo",
                                                     "VkExtent2D",
                                                     "VkExtent3D",
                                                     "VkFenceCreateInfo",
                                                     "VkFramebufferCreateInfo",
                                                     "VkGraphicsPipelineCreateInfo",
                                                     "VkImageCreateInfo",
                                                     "VkImageSubresourceRange",
                                                     "VkImageViewCreateInfo",
                                                     "VkInstanceCreateInfo",
                                                     "VkMemoryAllocateInfo",
                                                     "VkMemoryHeap",
                                                     "VkMemoryRequirements",
                                                     "VkMemoryType",
                                                     "VkOffset2D",
                                                     "VkPhysicalDeviceLimits",
                                                     "VkPhysicalDeviceMemoryProperties",
                                                     "VkPhysicalDeviceProperties",
                                                     "VkPhysicalDeviceSparseProperties",
                                                     "VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR",
                                                     "VkPhysicalDeviceVulkan12Features",
                                                     "VkPipelineColorBlendAttachmentState",
                                                     "VkPipelineColorBlendStateCreateInfo",
                                                     "VkPipelineInputAssemblyStateCreateInfo",
                                                     "VkPipelineLayoutCreateInfo",
                                                     "VkPipelineMultisampleStateCreateInfo",
                                                     "VkPipelineRasterizationStateCreateInfo",
                                                     "VkPipelineShaderStageCreateInfo",
                                                     "VkPipelineVertexInputStateCreateInfo",
                                                     "VkPipelineViewportStateCreateInfo",
                                                     "VkPushConstantRange",
                                                     "VkQueueFamilyProperties",
                                                     "VkRect2D",
                                                     "VkRenderPassCreateInfo",
                                                     "VkSamplerCreateInfo",
                                                     "VkSemaphoreCreateInfo",
                                                     "VkShaderModuleCreateInfo",
                                                     "VkSubpassDependency",
                                                     "VkSubpassDescription",
                                                     "VkSwapchainCreateInfoKHR",
                                                     "VkViewport",
                                                     "VkXcbSurfaceCreateInfoKHR",
                                                     "VulkanHelper_InitializeDeviceMemoryInfo"};
    bool creation = false, relevant_handle = false;
    const auto numbered = [](std::string_view value, std::string_view prefix) {
        return value.starts_with(prefix) && value.size() > prefix.size() &&
               value.substr(prefix.size()).find_first_not_of("0123456789") == std::string_view::npos;
    };
    // Qualify executable statement boundaries too: arbitrary pointer writes or
    // default-constructed local classes cannot hide behind the helper scan.
    for(auto i = opening + 1; i < end;) {
        if(t[i].value.starts_with('#')) {
            ++i;
            continue;
        }
        if(t[i].value == "{") {
            check(qualified_resource_sibling(d, i), "nested resource setup references an unmodeled handle");
            i = t[i].pair + 1;
            continue;
        }
        auto finish = i;
        while(finish < end && t[finish].value != ";") {
            if(t[finish].value == "(" || t[finish].value == "[" || t[finish].value == "{")
                finish = t[finish].pair;
            ++finish;
        }
        check(finish < end, "unterminated resource setup statement");
        const auto first = t[i].value;
        if(first == "static") {
            const auto type = t.at(i + 1).value;
            check(value_types.contains(type) || aggregate_types.contains(type) || type == "float" ||
                      type == "VkDescriptorSetLayout" || type == "VkImageView" ||
                      (type == "const" && t.at(i + 2).value == "char" && t.at(i + 3).value == "*"),
                  "unrecognized resource declaration type");
            std::size_t assignments = 0;
            for(auto j = i; j < finish; ++j)
                assignments += t[j].value == "=";
            check(assignments <= 1, "side effect in resource initializer");
        } else if(numbered(first, "VkImage_uid_")) {
            check(finish == i + 6 && t[i + 1].value == "=" && numbered(t[i + 2].value, "std_vector_of_VkImage_temp_") &&
                      t[i + 3].value == "[" && t[i + 5].value == "]",
                  "unrecognized swapchain image assignment");
            (void)integer(t[i + 4].value);
        } else if(first == "uint32_t") {
            check(i + 3 < finish && numbered(t[i + 1].value, "uint32_t_temp_") && t[i + 2].value == "(",
                  "unrecognized local count declaration");
        } else if(first == "std") {
            check(i + 9 < finish && d.expression(i, i + 6) == "std : : vector < VkImage" && t[i + 6].value == ">" &&
                      numbered(t[i + 7].value, "std_vector_of_VkImage_temp_") && t[i + 8].value == "(",
                  "unrecognized local image vector declaration");
        } else {
            check(helpers.contains(first) && i + 1 < finish && t[i + 1].value == "(",
                  "unrecognized executable resource setup statement");
            for(auto j = i; j < finish; ++j)
                check(t[j].value != "=", "side effect in resource setup call");
        }
        i = finish + 1;
    }
    for(auto i = opening + 1; i < end; ++i) {
        const auto v = t[i].value;
        check(v != "+" && v != "new" && v != "delete" && v != "throw", "unsupported resource expression");
        if(i + 1 < end && t[i + 1].value == "{" &&
           (std::isalpha(static_cast<unsigned char>(v.front())) || v.front() == '_'))
            check(aggregate_types.contains(v), "unrecognized aggregate in resource sibling");
        creation |= v == "VulkanReplay_CreateGraphicsPipelines" || v == "VulkanReplay_CreateShaderModule";
        relevant_handle |= v.starts_with("VkPipeline_uid_") || v.starts_with("VkShaderModule_uid_");
        if(i + 3 < end && t[i + 1].value == "(" && t[i + 2].value == "*" && t[i + 3].value == "reinterpret_cast") {
            const auto expression = d.expression(i + 1, t[i + 1].pair + 1);
            check(expression == "( * reinterpret_cast < xcb_connection_t * * > ( WindowSystemInstance ( ) . "
                                "GetDisplayAddr ( ) ) )" ||
                      expression ==
                          "( * reinterpret_cast < xcb_window_t * > ( WindowSystemInstance ( ) . GetWindowAddr ( ) ) )",
                  "unrecognized native window handle expression");
            i = t[i + 1].pair;
            continue;
        }
        if(i + 1 < end && t[i + 1].value == "(") {
            const bool vector_data = v == "data" && i >= 2 && t[i - 1].value == "." &&
                                     numbered(t[i - 2].value, "std_vector_of_VkImage_temp_");
            if(!helpers.contains(v) && !value_types.contains(v) && !vector_data && !numbered(v, "uint32_t_temp_") &&
               !numbered(v, "std_vector_of_VkImage_temp_"))
                throw Unsupported("unrecognized helper in resource sibling: " + std::string(v.substr(0, 128)));
        }
    }
    if(creation) {
        const auto parsed = body(d, opening, false);
        check(parsed.calls.size() == 1, "creation sibling must have one direct replay call");
        return true;
    }
    return !relevant_handle;
}
std::map<std::string, Creation> creations(const std::vector<Document>& docs, Json& unsupported, ResultBudget& budget) {
    std::map<std::string, Creation> result;
    std::set<std::string> duplicates;
    bool unassignable_output = false;
    for(const auto& d : docs) {
        if(!std::filesystem::path(d.source.path).filename().string().starts_with("Resources"))
            continue;
        const auto& t = d.tokens;
        std::map<std::size_t, bool> qualified_parents;
        for(std::size_t i = 0; i + 3 < t.size(); ++i) {
            if(t[i].value != "NV_VK_REPLAY" || (t[i + 2].value != "VulkanReplay_CreateGraphicsPipelines" &&
                                                t[i + 2].value != "VulkanReplay_CreateShaderModule"))
                continue;
            std::string symbol;
            try {
                auto invocation = call(d, i);
                const bool pipeline = invocation.name == "VulkanReplay_CreateGraphicsPipelines";
                check(invocation.args.size() == (pipeline ? 6 : 4), "unsupported creation call arity");
                auto output = expr(d, invocation.args.back());
                if(pipeline) {
                    const std::string prefix = "NV_TO_OUTPUT_ARRAY ( & ";
                    check(output.starts_with(prefix) && output.ends_with(" )"), "unsupported pipeline output");
                    output = output.substr(prefix.size(), output.size() - prefix.size() - 2);
                } else {
                    check(output.starts_with("& "), "unsupported shader output");
                    output = output.substr(2);
                }
                symbol = uid(output, pipeline ? "VkPipeline_uid_" : "VkShaderModule_uid_");
                const auto block = t[i].brace;
                check(block != none && t[block].brace != none, "creation lacks object block");
                const auto parent = t[block].brace;
                check(t[parent].brace == none && parent >= 4 && t[parent - 1].value == ")" && t[parent - 1].pair >= 2 &&
                          t[parent - 1].pair + 1 == parent - 1 && t[t[parent - 1].pair - 2].value == "void" &&
                          t[t[parent - 1].pair - 1].value.starts_with("CreateResources"),
                      "creation is not a direct resource setup block");
                if(!qualified_parents.contains(parent)) {
                    // Cache refusal even if validating a sibling throws. Otherwise
                    // every later creation could rescan the same invalid parent.
                    qualified_parents.emplace(parent, false);
                    bool qualified = true;
                    for(auto j = parent + 1; j < t[parent].pair;) {
                        if(t[j].value == "{") {
                            if(!qualified_resource_sibling(d, j)) {
                                qualified = false;
                                break;
                            }
                            j = t[j].pair + 1;
                        } else if(t[j].value == "BEGIN_DATA_SCOPE_FUNCTION" && j + 3 < t[parent].pair &&
                                  t[j + 1].value == "(" && t[j + 2].value == ")" && t[j + 3].value == ";")
                            j += 4;
                        else if(t[j].value == "NV_MESSAGE_VERBOSE" && j + 4 < t[parent].pair && t[j + 1].value == "(" &&
                                t[j + 2].value.starts_with('"') && t[j + 3].value == ")" && t[j + 4].value == ";")
                            j += 5;
                        else {
                            qualified = false;
                            break;
                        }
                    }
                    qualified_parents.at(parent) = qualified;
                }
                check(qualified_parents.at(parent), "resource setup parent contains unsupported direct statements");
                auto content = body(d, block, false);
                check(content.calls.size() == 1, "ambiguous creation block");
                check(!t[i].conditional, "conditional creation unsupported");
                if(!result.emplace(symbol, Creation{&d, block, std::move(content), std::move(invocation)}).second)
                    duplicates.insert(symbol);
            } catch(const Unsupported& e) {
                budget.append(unsupported, {{"source", d.span(i, i)}, {"symbol", symbol}, {"reason", e.what()}});
                if(symbol.empty())
                    unassignable_output = true;
                if(!symbol.empty())
                    duplicates.insert(symbol);
            }
        }
    }
    for(const auto& symbol : duplicates) {
        result.erase(symbol);
        budget.append(unsupported,
                      {{"symbol", symbol}, {"reason", "duplicate or unsupported definition prevents resolution"}});
    }
    if(unassignable_output) {
        result.clear();
        budget.append(unsupported,
                      {{"symbol", ""},
                       {"reason", "unassignable creation output prevents safe resolution of all object definitions"}});
    }
    return result;
}
Json pipeline_evidence(const Creation& pipeline, const std::map<std::string, Creation>& objects) {
    const auto& d = *pipeline.doc;
    const auto& args = pipeline.invocation.args;
    check(expr(d, args[2]) == "1u", "only one literal pipeline creation supported");
    const auto fields =
        initializer(d, pipeline.content, expr(d, args[3]), "VkGraphicsPipelineCreateInfo", pipeline.invocation.at);
    check(fields.size() == 19 && expr(d, fields[0]) == "VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO" &&
              expr(d, fields[1]) == "nullptr",
          "unsupported graphics pipeline initializer");
    const auto stage_count = integer(expr(d, fields[3]));
    check(stage_count > 0 && stage_count <= 8, "stage count out of bounds");
    const auto stages = pipeline.content.declarations.find(expr(d, fields[4]));
    check(stages != pipeline.content.declarations.end() && stages->second.type == "VkPipelineShaderStageCreateInfo" &&
              stages->second.constructors.size() == stage_count && stages->second.extent == stage_count &&
              stages->second.scope.second < fields[4].first,
          "stage array/count mismatch");
    Json result{{"source", d.span(pipeline.opening, d.tokens[pipeline.opening].pair)}, {"stages", Json::array()}};
    std::set<std::string> stage_names;
    for(const auto constructor : stages->second.constructors) {
        const auto f = arguments(d, constructor);
        check(f.size() == 7 && expr(d, f[0]) == "VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO" &&
                  expr(d, f[1]) == "nullptr" && expr(d, f[6]) == "nullptr",
              "unsupported shader stage extensions/specialization");
        check(f[5].second == f[5].first + 1 && d.tokens[f[5].first].value.starts_with('"') &&
                  d.tokens[f[5].first].value.size() > 2 && d.tokens[f[5].first].value.size() <= 258,
              "entry point must be a bounded string literal");
        const auto stage = expr(d, f[3]);
        const std::set<std::string> supported_stages{
            "VK_SHADER_STAGE_VERTEX_BIT", "VK_SHADER_STAGE_FRAGMENT_BIT", "VK_SHADER_STAGE_GEOMETRY_BIT",
            "VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT", "VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT"};
        check(supported_stages.contains(stage) && stage_names.insert(stage).second,
              "invalid or duplicate shader stage");
        const auto module = uid(expr(d, f[4]), "VkShaderModule_uid_");
        const auto found = objects.find(module);
        check(found != objects.end(), "shader module creation unavailable or ambiguous");
        const auto& c = found->second;
        const auto& sd = *c.doc;
        const auto sf =
            initializer(sd, c.content, expr(sd, c.invocation.args[1]), "VkShaderModuleCreateInfo", c.invocation.at);
        check(sf.size() == 5 && expr(sd, sf[0]) == "VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO" &&
                  expr(sd, sf[1]) == "nullptr",
              "unsupported shader module initializer");
        const auto code = sf[4];
        check(sd.tokens[code.first].value == "NV_GET_RESOURCE_CHECKED" && sd.tokens[code.first + 1].value == "(" &&
                  sd.tokens[code.first + 1].pair + 1 == code.second,
              "unsupported shader resource expression");
        const auto resource = arguments(sd, code.first + 1);
        check(resource.size() == 3 && expr(sd, resource[0]) == "const uint32_t *", "unsupported shader resource type");
        const auto bytes = integer(expr(sd, sf[3]));
        check(bytes > 0 && bytes <= 16U * 1024U * 1024U && bytes % 4 == 0 && bytes == integer(expr(sd, resource[2])),
              "shader byte counts disagree or exceed bounds");
        result["stages"].push_back({{"stage", stage},
                                    {"module", module},
                                    {"entry_point_expression", expr(d, f[5])},
                                    {"stage_source", d.span(constructor, d.tokens[constructor].pair)},
                                    {"module_source", sd.span(c.opening, sd.tokens[c.opening].pair)},
                                    {"resource_handle", integer(expr(sd, resource[1]))},
                                    {"declared_bytes", bytes}});
    }
    return result;
}
} // namespace

nlohmann::json inspect_cpp_draws(const std::vector<CppSource>& sources) {
    Json result{{"draws", Json::array()},
                {"unsupported_recordings", Json::array()},
                {"unsupported_objects", Json::array()},
                {"association_scope",
                 "Literal generated replay source relationships in qualified single-part recordings; not "
                 "executed GPU state, submission order, descriptor contents or extracted shader bytes."}};
    std::vector<Document> docs;
    std::size_t total = 0;
    ResultBudget budget;
    try {
        check(sources.size() <= 64, "too many generated source files");
        docs.reserve(sources.size());
        std::set<std::string> paths;
        for(const auto& source : sources) {
            check(paths.insert(source.path).second, "duplicate source path");
            total += source.text.size();
            check(total <= 16U * 1024U * 1024U, "aggregate source exceeds 16 MiB");
            docs.emplace_back(source);
        }
        const auto objects = creations(docs, result["unsupported_objects"], budget);
        const std::set<std::string> allowed{"VulkanReplay_BeginCommandBuffer",
                                            "VulkanReplay_EndCommandBuffer",
                                            "VulkanReplay_CmdBeginDebugUtilsLabelEXT",
                                            "VulkanReplay_CmdEndDebugUtilsLabelEXT",
                                            "VulkanReplay_CmdPipelineBarrier",
                                            "VulkanReplay_CmdBeginRenderPass",
                                            "VulkanReplay_CmdEndRenderPass",
                                            "VulkanReplay_CmdBindPipeline",
                                            "VulkanReplay_CmdBindDescriptorSets",
                                            "VulkanReplay_CmdPushConstants",
                                            "VulkanReplay_CmdDraw",
                                            "VulkanReplay_CmdDrawIndirect"};
        std::set<std::string> all_functions;
        for(const auto& d : docs) {
            if(!std::filesystem::path(d.source.path).filename().string().starts_with("CommandList"))
                continue;
            const auto& t = d.tokens;
            struct Function {
                std::size_t at, opening;
            };
            std::map<std::string, Function> functions;
            for(std::size_t i = 0; i + 3 < t.size(); ++i)
                if(t[i].value == "void" && t[i + 1].value.starts_with("Record_") && t[i + 2].value == "(") {
                    const auto end = t[i + 2].pair;
                    check(end != none && end + 1 < t.size() && t[end + 1].value == "{",
                          "recording function shape unsupported");
                    check(all_functions.size() < 4096 && all_functions.insert(std::string(t[i + 1].value)).second,
                          "duplicate recording name or too many recording functions");
                    check(functions.emplace(std::string(t[i + 1].value), Function{i, end + 1}).second,
                          "duplicate recording function");
                }
            check(!functions.empty(), "command-list source has no recognized recording functions");
            for(const auto& [name, fn] : functions) {
                if(const auto suffix = name.find("_Part"); suffix != std::string::npos) {
                    if(!functions.contains(name.substr(0, suffix)))
                        budget.append(result["unsupported_recordings"], {{"recording", name},
                                                                         {"source", d.span(fn.at, t[fn.opening].pair)},
                                                                         {"reason", "recording part lacks wrapper"}});
                    continue;
                }
                try {
                    const auto part = functions.find(name + "_Part00");
                    check(part != functions.end(), "single Part00 recording missing");
                    const auto prefix = name + "_Part";
                    for(auto candidate = functions.lower_bound(prefix);
                        candidate != functions.end() && candidate->first.starts_with(prefix); ++candidate)
                        check(candidate->first == part->first, "multiple recording parts unsupported");
                    const auto signature = [&](const Function& f) {
                        return !t[f.at].conditional && t[f.opening].brace == none &&
                               d.expression(f.at + 3, t[f.at + 2].pair) == "VkCommandBuffer cmdBuffer";
                    };
                    check(signature(fn) && signature(part->second), "recording parameter shape unsupported");
                    check(d.expression(fn.opening + 1, t[fn.opening].pair) ==
                              "BEGIN_DATA_SCOPE_FUNCTION ( ) ; " + part->first + " ( cmdBuffer ) ;",
                          "recording wrapper is not one direct part call");
                    auto parsed = body(d, part->second.opening, true);
                    check(parsed.calls.size() >= 2 && parsed.calls.front().name == "VulkanReplay_BeginCommandBuffer" &&
                              parsed.calls.back().name == "VulkanReplay_EndCommandBuffer",
                          "recording requires explicit Begin/End");
                    std::set<std::uint64_t> events;
                    Json draws = Json::array();
                    const Call* binding = nullptr;
                    for(std::size_t n = 0; n < parsed.calls.size(); ++n) {
                        const auto& c = parsed.calls[n];
                        check(allowed.contains(c.name) && !c.args.empty() && expr(d, c.args[0]) == "cmdBuffer",
                              "unknown command or different command-buffer operand");
                        check(events.insert(c.event).second, "duplicate event annotation");
                        check((c.name != "VulkanReplay_BeginCommandBuffer" || n == 0) &&
                                  (c.name != "VulkanReplay_EndCommandBuffer" || n + 1 == parsed.calls.size()),
                              "unexpected recording boundary");
                        if(c.name == "VulkanReplay_CmdBindPipeline") {
                            check(c.args.size() == 3 && expr(d, c.args[1]) == "VK_PIPELINE_BIND_POINT_GRAPHICS",
                                  "unsupported pipeline bind");
                            (void)uid(expr(d, c.args[2]), "VkPipeline_uid_");
                            binding = &c;
                        }
                        if(c.name == "VulkanReplay_CmdDraw" || c.name == "VulkanReplay_CmdDrawIndirect") {
                            check(c.args.size() == 5, "unsupported draw arity");
                            Json arguments_json = Json::array();
                            for(const auto r : c.args)
                                arguments_json.push_back(expr(d, r));
                            Json draw{{"recording", name},
                                      {"event_index", c.event},
                                      {"function", c.name},
                                      {"arguments", arguments_json},
                                      {"source", d.span(c.at, c.end)},
                                      {"association_status", "unavailable"},
                                      {"reason", "no graphics pipeline bind in this recording"},
                                      {"pipeline", nullptr}};
                            if(binding) {
                                const auto symbol = expr(d, binding->args[2]);
                                draw["pipeline_bind"] = {{"symbol", symbol},
                                                         {"event_index", binding->event},
                                                         {"source", d.span(binding->at, binding->end)}};
                                try {
                                    const auto object = objects.find(symbol);
                                    check(object != objects.end(), "pipeline creation unavailable or ambiguous");
                                    draw["pipeline"] = pipeline_evidence(object->second, objects);
                                    draw["pipeline"]["symbol"] = symbol;
                                    draw["association_status"] = "resolved_source_relationship";
                                    draw["reason"] = nullptr;
                                } catch(const Unsupported& e) {
                                    draw["reason"] = e.what();
                                }
                            } else
                                draw["pipeline_bind"] = nullptr;
                            budget.reserve(draw);
                            draws.push_back(std::move(draw));
                        }
                    }
                    for(auto& draw : draws)
                        result["draws"].push_back(std::move(draw));
                } catch(const Unsupported& e) {
                    budget.append(
                        result["unsupported_recordings"],
                        {{"recording", name}, {"source", d.span(fn.at, t[fn.opening].pair)}, {"reason", e.what()}});
                }
            }
        }
    } catch(const Unsupported& e) {
        throw InspectionError(InspectionErrorCode::InvalidExport, e.what());
    } catch(const std::out_of_range&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Truncated generated-source expression");
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Generated source contains invalid UTF-8");
    }
    return result;
}
} // namespace ngm
