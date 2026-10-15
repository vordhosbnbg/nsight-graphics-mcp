#include "ngm/SpirvUniforms.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"

#define SPV_ENABLE_UTILITY_CODE
#include <spirv.hpp11>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Op = spv::Op;
struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void valid(bool condition, const char* reason) {
    if(!condition)
        throw InspectionError(InspectionErrorCode::InvalidExport, reason);
}
void qualified(bool condition, const char* reason) {
    if(!condition)
        throw Unsupported(reason);
}
std::uint32_t word(std::span<const std::byte> data, std::size_t offset) {
    valid(offset <= data.size() && data.size() - offset >= 4, "Truncated SPIR-V word");
    std::uint32_t value = 0;
    for(std::size_t n = 0; n < 4; ++n)
        value |= std::uint32_t(std::to_integer<unsigned char>(data[offset + n])) << (8 * n);
    return value;
}
struct Instruction {
    Op op;
    std::size_t word_offset;
    std::vector<std::uint32_t> args;
    std::uint32_t function = 0;
};
using Decorations = std::map<std::uint32_t, std::vector<std::uint32_t>>;
struct Module {
    std::uint32_t bound;
    std::vector<Instruction> instructions;
    std::map<std::uint32_t, std::size_t> definitions;
    std::map<std::uint32_t, Decorations> decorations;
    std::map<std::pair<std::uint32_t, std::uint32_t>, Decorations> members;
    std::map<std::uint32_t, std::string> names;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::string> member_names;
    const Instruction& definition(std::uint32_t id) const {
        const auto at = definitions.find(id);
        valid(at != definitions.end(), "SPIR-V refers to an undefined reflected ID");
        return instructions.at(at->second);
    }
    void id(std::uint32_t value) const {
        valid(value > 0 && value < bound, "SPIR-V ID exceeds its declared bound");
    }
};
std::pair<std::string, std::size_t> literal(const Instruction& i, std::size_t start, std::size_t maximum) {
    std::string value;
    for(auto n = start; n < i.args.size(); ++n)
        for(unsigned byte = 0; byte < 4; ++byte) {
            const auto c = static_cast<unsigned char>(i.args[n] >> (8 * byte));
            if(c == 0) {
                valid((i.args[n] >> (8 * byte)) == 0, "SPIR-V literal has nonzero padding");
                return {value, n + 1};
            }
            valid(value.size() < maximum, "SPIR-V reflected name exceeds bounds");
            value.push_back(static_cast<char>(c));
        }
    throw InspectionError(InspectionErrorCode::InvalidExport, "Unterminated SPIR-V reflected literal");
}
std::uint32_t decoration(const Decorations& d, spv::Decoration kind) {
    const auto at = d.find(static_cast<std::uint32_t>(kind));
    qualified(at != d.end() && at->second.size() == 1, "Missing or unqualified SPIR-V layout decoration");
    return at->second.front();
}
void allowed(const Decorations& decorations, std::initializer_list<spv::Decoration> kinds) {
    for(const auto& [kind, values] : decorations) {
        (void)values;
        qualified(std::any_of(kinds.begin(), kinds.end(),
                              [&](auto allowed) { return kind == static_cast<std::uint32_t>(allowed); }),
                  "Unqualified decoration on reflected uniform layout");
    }
}
Module parse(std::span<const std::byte> bytes) {
    valid(bytes.size() >= 20 && bytes.size() <= 1024U * 1024U && bytes.size() % 4 == 0,
          "SPIR-V module size exceeds reflection bounds");
    valid(word(bytes, 0) == 0x07230203 && word(bytes, 16) == 0, "Unsupported SPIR-V header or word order");
    qualified(word(bytes, 4) == 0x00010600, "Only the qualified SPIR-V 1.6 profile is reflected");
    Module module;
    module.bound = word(bytes, 12);
    valid(module.bound > 0 && module.bound <= 65536, "SPIR-V ID bound exceeds reflection limits");
    std::uint32_t function = 0;
    for(std::size_t at = 5; at < bytes.size() / 4;) {
        const auto header = word(bytes, at * 4), count = header >> 16;
        valid(count > 0 && count <= bytes.size() / 4 - at, "Truncated SPIR-V instruction");
        Instruction instruction{static_cast<Op>(header & 0xffff), at, {}, function};
        qualified(std::string_view(spv::OpToString(instruction.op)) != "Unknown", "Unknown SPIR-V opcode");
        for(std::uint32_t n = 1; n < count; ++n)
            instruction.args.push_back(word(bytes, (at + n) * 4));
        bool result = false, type = false;
        spv::HasResultAndType(instruction.op, &result, &type);
        valid(!type || (result && instruction.args.size() >= 2), "Truncated SPIR-V typed result");
        if(result) {
            valid(!instruction.args.empty(), "Truncated SPIR-V result");
            const auto id = instruction.args.at(type ? 1 : 0);
            module.id(id);
            valid(module.definitions.emplace(id, module.instructions.size()).second, "Duplicate SPIR-V result ID");
            if(type)
                module.id(instruction.args[0]);
        }
        if(instruction.op == Op::OpFunction) {
            valid(function == 0 && instruction.args.size() == 4, "Unqualified SPIR-V function boundary");
            function = instruction.args[1];
            instruction.function = function;
        } else if(instruction.op == Op::OpFunctionEnd) {
            valid(function != 0 && instruction.args.empty(), "Unmatched SPIR-V function end");
            function = 0;
        }
        module.instructions.push_back(std::move(instruction));
        valid(module.instructions.size() <= 65536, "SPIR-V instruction count exceeds limits");
        at += count;
    }
    valid(function == 0, "Unclosed SPIR-V function");
    for(const auto& i : module.instructions) {
        const auto& a = i.args;
        if(i.op == Op::OpName) {
            valid(a.size() >= 2, "Truncated SPIR-V name");
            module.id(a[0]);
            const auto [name, end] = literal(i, 1, 256);
            valid(end == a.size() && module.names.emplace(a[0], name).second, "Duplicate or malformed SPIR-V name");
        } else if(i.op == Op::OpMemberName) {
            valid(a.size() >= 3, "Truncated SPIR-V member name");
            module.id(a[0]);
            const auto [name, end] = literal(i, 2, 256);
            valid(end == a.size() && module.member_names.emplace(std::make_pair(a[0], a[1]), name).second,
                  "Duplicate or malformed SPIR-V member name");
        } else if(i.op == Op::OpDecorate || i.op == Op::OpMemberDecorate) {
            const auto member = i.op == Op::OpMemberDecorate;
            valid(a.size() >= (member ? 3u : 2u), "Truncated SPIR-V decoration");
            module.id(a[0]);
            auto& destination = member ? module.members[{a[0], a[1]}] : module.decorations[a[0]];
            valid(destination
                      .emplace(a[member ? 2 : 1], std::vector<std::uint32_t>(a.begin() + (member ? 3 : 2), a.end()))
                      .second,
                  "Duplicate SPIR-V layout decoration");
        } else if(i.op == Op::OpDecorationGroup || i.op == Op::OpGroupDecorate || i.op == Op::OpGroupMemberDecorate ||
                  i.op == Op::OpDecorateId)
            throw Unsupported("Grouped or ID-valued decorations need separate layout qualification");
    }
    return module;
}
Json scalar_vector(const Module& module, std::uint32_t type) {
    auto definition = &module.definition(type);
    std::uint32_t lanes = 1;
    if(definition->op == Op::OpTypeVector) {
        valid(definition->args.size() == 3, "Truncated SPIR-V vector type");
        lanes = definition->args[2];
        qualified(lanes >= 2 && lanes <= 4, "Uniform vector width is unqualified");
        definition = &module.definition(definition->args[1]);
    }
    const auto& a = definition->args;
    std::string scalar;
    if(definition->op == Op::OpTypeFloat) {
        valid(a.size() == 2, "Truncated SPIR-V float type");
        qualified(a[1] == 32, "Only 32-bit uniform scalars are qualified");
        scalar = "float32";
    } else if(definition->op == Op::OpTypeInt) {
        valid(a.size() == 3 && a[2] <= 1, "Malformed SPIR-V integer type");
        qualified(a[1] == 32, "Only 32-bit uniform scalars are qualified");
        scalar = a[2] ? "int32" : "uint32";
    } else
        throw Unsupported("Uniform arrays, matrices, booleans or nested structures need separate qualification");
    return {{"type_id", type}, {"scalar", scalar}, {"lanes", lanes}, {"bytes", lanes * 4}};
}
Json reflect(Module& module, std::string_view entry, std::uint32_t model) {
    qualified(!entry.empty() && entry.size() <= 128 && (model == 0 || model == 4),
              "Only bounded vertex/fragment entry selections are qualified");
    std::uint32_t selected = 0;
    std::set<std::uint32_t> interface;
    std::size_t functions = 0, memories = 0;
    for(const auto& i : module.instructions) {
        const auto& a = i.args;
        if(i.op == Op::OpEntryPoint) {
            valid(a.size() >= 3, "Truncated SPIR-V entry point");
            const auto [name, after] = literal(i, 2, 128);
            if(a[0] == model && name == entry) {
                qualified(selected == 0, "Ambiguous selected SPIR-V entry point");
                selected = a[1];
                module.id(selected);
                for(auto n = after; n < a.size(); ++n) {
                    module.id(a[n]);
                    valid(interface.insert(a[n]).second, "Repeated SPIR-V entry interface ID");
                }
            }
        } else if(i.op == Op::OpMemoryModel) {
            qualified(a == std::vector<std::uint32_t>({0, 1}), "Only Logical/GLSL450 memory is qualified");
            ++memories;
        } else if(i.op == Op::OpFunction)
            ++functions;
        else if(i.op == Op::OpFunctionCall || i.op == Op::OpBranch || i.op == Op::OpBranchConditional ||
                i.op == Op::OpSwitch || i.op == Op::OpKill || i.op == Op::OpTerminateInvocation ||
                i.op == Op::OpUnreachable || i.op == Op::OpLoopMerge || i.op == Op::OpSelectionMerge)
            throw Unsupported("Control flow or calls need separate uniform-load reachability qualification");
    }
    qualified(selected != 0 && memories == 1 && functions == 1 && module.definition(selected).op == Op::OpFunction,
              "Selected entry or single-function memory/control profile is unqualified");
    qualified(std::count_if(module.instructions.begin(), module.instructions.end(),
                            [&](const auto& i) { return i.function == selected && i.op == Op::OpLabel; }) == 1 &&
                  std::count_if(module.instructions.begin(), module.instructions.end(),
                                [&](const auto& i) { return i.function == selected && i.op == Op::OpReturn; }) == 1,
              "Uniform loads require one straight-line entry block and return");
    std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> addresses;
    Json reads = Json::array();
    const auto pointer = [&](std::uint32_t id) -> const Instruction& {
        const auto& value = module.definition(id);
        qualified(value.args.size() >= 2, "Uniform pointer result is unqualified");
        const auto& type = module.definition(value.args[0]);
        qualified(type.op == Op::OpTypePointer && type.args.size() == 3,
                  "Uniform address does not have a qualified pointer type");
        return type;
    };
    std::map<std::uint32_t, std::uint32_t> variables;
    for(const auto& i : module.instructions)
        if(i.op == Op::OpVariable && i.args.size() >= 3 && i.args[2] == 2) {
            qualified(i.args.size() == 3 && i.function == 0, "Initialized/function uniform variable is unqualified");
            const auto& type = pointer(i.args[1]);
            qualified(type.args[1] == 2, "Uniform variable pointer storage mismatch");
            variables[i.args[1]] = type.args[2];
        }
    std::set<std::pair<std::uint32_t, std::uint32_t>> bindings;
    for(const auto& [variable, structure] : variables) {
        allowed(module.decorations[variable], {spv::Decoration::DescriptorSet, spv::Decoration::Binding});
        const auto set = decoration(module.decorations[variable], spv::Decoration::DescriptorSet);
        const auto binding = decoration(module.decorations[variable], spv::Decoration::Binding);
        qualified(bindings.emplace(set, binding).second, "Duplicate uniform descriptor binding");
        const auto& type = module.definition(structure);
        qualified(type.op == Op::OpTypeStruct && type.args.size() >= 2 && type.args.size() <= 65,
                  "Only bounded plain uniform blocks are qualified");
        allowed(module.decorations[structure], {spv::Decoration::Block});
        qualified(module.decorations[structure].contains(static_cast<std::uint32_t>(spv::Decoration::Block)) &&
                      module.decorations[structure].at(static_cast<std::uint32_t>(spv::Decoration::Block)).empty(),
                  "Uniform structure lacks a plain Block decoration");
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for(std::uint32_t member = 0; member + 1 < type.args.size(); ++member) {
            const auto& decorations = module.members[{structure, member}];
            allowed(decorations, {spv::Decoration::Offset});
            const auto offset = decoration(decorations, spv::Decoration::Offset);
            const auto layout = scalar_vector(module, type.args[member + 1]);
            qualified(offset <= 65536 && offset + layout.at("bytes").get<std::uint64_t>() <= 65536 && offset % 4 == 0,
                      "Uniform member byte range exceeds qualified bounds");
            const auto end = offset + layout.at("bytes").get<std::uint64_t>();
            for(const auto& other : ranges)
                qualified(end <= other.first || offset >= other.second, "Overlapping uniform member byte ranges");
            ranges.emplace_back(offset, end);
        }
    }
    for(const auto& i : module.instructions) {
        const auto& a = i.args;
        if(i.function != selected)
            continue;
        if(i.op == Op::OpAccessChain || i.op == Op::OpInBoundsAccessChain) {
            valid(a.size() >= 4, "Truncated SPIR-V access chain");
            const auto& type = pointer(a[1]);
            if(type.args[1] != 2)
                continue;
            qualified(a.size() == 4 && variables.contains(a[2]) && interface.contains(a[2]),
                      "Dynamic, nested or non-interface uniform addressing is unqualified");
            const auto& index = module.definition(a[3]);
            qualified(index.op == Op::OpConstant && index.args.size() == 3,
                      "Uniform member index is not one literal constant");
            const auto& integer = module.definition(index.args[0]);
            qualified(integer.op == Op::OpTypeInt && integer.args.size() == 3 && integer.args[1] == 32 &&
                          integer.args[2] <= 1,
                      "Uniform member index type is unqualified");
            const auto structure = variables.at(a[2]);
            const auto& members = module.definition(structure).args;
            const auto member = index.args[2];
            qualified(member < members.size() - 1 && type.args[2] == members[member + 1],
                      "Uniform member index or pointer element type mismatch");
            addresses[a[1]] = {a[2], member};
        } else if(i.op == Op::OpLoad) {
            valid(a.size() >= 3, "Truncated SPIR-V load");
            const auto& type = pointer(a[2]);
            if(type.args[1] != 2)
                continue;
            qualified(a.size() == 3 && addresses.contains(a[2]) && a[0] == type.args[2],
                      "Uniform load address/type/memory operands are unqualified");
            const auto [variable, member] = addresses.at(a[2]);
            const auto structure = variables.at(variable);
            const auto layout = scalar_vector(module, a[0]);
            reads.push_back(
                {{"variable_id", variable},
                 {"variable_name", module.names.contains(variable) ? Json(module.names.at(variable)) : Json(nullptr)},
                 {"block_type_id", structure},
                 {"block_name", module.names.contains(structure) ? Json(module.names.at(structure)) : Json(nullptr)},
                 {"member", member},
                 {"member_name", module.member_names.contains({structure, member})
                                     ? Json(module.member_names.at({structure, member}))
                                     : Json(nullptr)},
                 {"descriptor_set", decoration(module.decorations[variable], spv::Decoration::DescriptorSet)},
                 {"binding", decoration(module.decorations[variable], spv::Decoration::Binding)},
                 {"offset", decoration(module.members[{structure, member}], spv::Decoration::Offset)},
                 {"layout", layout},
                 {"load_result_id", a[1]},
                 {"word_offset", i.word_offset},
                 {"word_count", a.size() + 1}});
            qualified(reads.size() <= 1024, "Uniform load result count exceeds bounds");
        } else if(i.op == Op::OpStore) {
            valid(a.size() >= 2, "Truncated SPIR-V store");
            qualified(pointer(a[0]).args[1] != 2, "Store to uniform address invalidates read-only qualification");
        } else {
            bool result = false, type = false;
            spv::HasResultAndType(i.op, &result, &type);
            if(type) {
                const auto& definition = module.definition(a[0]);
                qualified(definition.op != Op::OpTypePointer || definition.args.size() != 3 || definition.args[1] != 2,
                          "Unqualified uniform pointer flow");
            }
        }
    }
    valid(reads.dump().size() <= 1024U * 1024U, "Uniform reflection output exceeds bounds");
    return {{"status", "qualified_static_uniform_loads"}, {"reason", nullptr}, {"reads", std::move(reads)}};
}
Json buffer_effects(Module& module, std::string_view entry, std::uint32_t model) {
    // Reuse entry/reachability and direct uniform-pointer qualification. A
    // successful uniform reflection alone deliberately does not prove effects.
    (void)reflect(module, entry, model);
    const std::set<Op> pure{Op::OpSource,
                            Op::OpSourceContinued,
                            Op::OpSourceExtension,
                            Op::OpName,
                            Op::OpMemberName,
                            Op::OpString,
                            Op::OpLine,
                            Op::OpNoLine,
                            Op::OpModuleProcessed,
                            Op::OpMemoryModel,
                            Op::OpEntryPoint,
                            Op::OpDecorate,
                            Op::OpMemberDecorate,
                            Op::OpTypeVoid,
                            Op::OpTypeBool,
                            Op::OpTypeInt,
                            Op::OpTypeFloat,
                            Op::OpTypeVector,
                            Op::OpTypeArray,
                            Op::OpTypeStruct,
                            Op::OpTypeFunction,
                            Op::OpConstant,
                            Op::OpConstantTrue,
                            Op::OpConstantFalse,
                            Op::OpConstantNull,
                            Op::OpConstantComposite,
                            Op::OpFunction,
                            Op::OpFunctionEnd,
                            Op::OpLabel,
                            Op::OpReturn,
                            Op::OpVectorExtractDynamic,
                            Op::OpVectorInsertDynamic,
                            Op::OpVectorShuffle,
                            Op::OpCompositeConstruct,
                            Op::OpCompositeExtract,
                            Op::OpCompositeInsert,
                            Op::OpCopyObject,
                            Op::OpConvertFToU,
                            Op::OpConvertFToS,
                            Op::OpConvertSToF,
                            Op::OpConvertUToF,
                            Op::OpUConvert,
                            Op::OpSConvert,
                            Op::OpFConvert,
                            Op::OpBitcast,
                            Op::OpSNegate,
                            Op::OpFNegate,
                            Op::OpIAdd,
                            Op::OpFAdd,
                            Op::OpISub,
                            Op::OpFSub,
                            Op::OpIMul,
                            Op::OpFMul,
                            Op::OpUDiv,
                            Op::OpSDiv,
                            Op::OpFDiv,
                            Op::OpUMod,
                            Op::OpSRem,
                            Op::OpSMod,
                            Op::OpFRem,
                            Op::OpFMod,
                            Op::OpVectorTimesScalar};
    const std::set<std::uint32_t> storage{1, 2, 3, 6, 7, 9}; // Input, Uniform, Output, Private, Function, PushConstant.
    const auto pointer = [&](std::uint32_t id) -> const Instruction& {
        const auto& definition = module.definition(id);
        valid(definition.args.size() >= 2, "Truncated pointer-producing SPIR-V definition");
        const auto& type = module.definition(definition.args[0]);
        qualified(type.op == Op::OpTypePointer && type.args.size() == 3 && storage.contains(type.args[1]),
                  "Unqualified external or opaque SPIR-V address");
        qualified(module.definition(type.args[2]).op != Op::OpTypePointer,
                  "Pointer-valued SPIR-V memory needs separate alias proof");
        return type;
    };
    const auto root = [&](std::uint32_t id) {
        std::set<std::uint32_t> visited;
        const auto expected = pointer(id).args[1];
        for(;;) {
            qualified(visited.size() < 64 && visited.insert(id).second,
                      "Cyclic or oversized SPIR-V pointer provenance");
            qualified(pointer(id).args[1] == expected, "SPIR-V pointer storage changes along its access chain");
            const auto& value = module.definition(id);
            if(value.op == Op::OpVariable) {
                qualified(value.args.size() >= 3 && value.args[2] == expected,
                          "SPIR-V variable/address storage mismatch");
                return id;
            }
            qualified((value.op == Op::OpAccessChain || value.op == Op::OpInBoundsAccessChain) &&
                          value.args.size() >= 4,
                      "Unqualified SPIR-V pointer origin");
            id = value.args[2];
        }
    };
    std::size_t loads = 0, stores = 0;
    for(const auto& i : module.instructions) {
        const auto& a = i.args;
        bool result = false, typed = false;
        spv::HasResultAndType(i.op, &result, &typed);
        if(typed) {
            const auto& type = module.definition(a[0]);
            qualified(type.op != Op::OpTypePointer || i.op == Op::OpVariable || i.op == Op::OpAccessChain ||
                          i.op == Op::OpInBoundsAccessChain,
                      "Unqualified SPIR-V pointer-producing operation");
        }
        if(i.op == Op::OpCapability)
            qualified(a == std::vector<std::uint32_t>{1}, "Only the core Shader capability has an effect proof");
        else if(i.op == Op::OpExtInstImport) {
            valid(a.size() >= 2, "Truncated SPIR-V instruction import");
            const auto [name, end] = literal(i, 1, 128);
            qualified(name == "GLSL.std.450" && end == a.size(), "Unqualified SPIR-V instruction set import");
            // Actual OpExtInst calls remain excluded by the allowlist below.
        } else if(i.op == Op::OpExecutionMode)
            qualified(model == 4 && a.size() == 2 && a[1] == 7,
                      "Only the fragment OriginUpperLeft execution mode has an effect proof");
        else if(i.op == Op::OpTypePointer)
            qualified(a.size() == 3 && storage.contains(a[1]) && module.definition(a[2]).op != Op::OpTypePointer,
                      "External, physical or pointer-valued SPIR-V storage is unqualified");
        else if(i.op == Op::OpVariable) {
            qualified(a.size() == 3 || a.size() == 4, "Unqualified SPIR-V variable initializer");
            (void)root(a[1]);
        } else if(i.op == Op::OpAccessChain || i.op == Op::OpInBoundsAccessChain) {
            valid(a.size() >= 4, "Truncated SPIR-V access chain");
            (void)root(a[1]);
        } else if(i.op == Op::OpLoad) {
            qualified(i.function != 0 && a.size() == 3 && pointer(a[2]).args[2] == a[0],
                      "Unqualified SPIR-V load type or memory operands");
            (void)root(a[2]);
            ++loads;
        } else if(i.op == Op::OpStore) {
            qualified(i.function != 0 && a.size() == 2, "Unqualified SPIR-V store memory operands");
            const auto& type = pointer(a[0]);
            qualified(type.args[1] == 3 || type.args[1] == 6 || type.args[1] == 7,
                      "SPIR-V store is not invocation-local or shader output");
            (void)root(a[0]);
            const auto& value = module.definition(a[1]);
            bool has_result = false, has_type = false;
            spv::HasResultAndType(value.op, &has_result, &has_type);
            valid(has_result && has_type && value.args[0] == type.args[2], "SPIR-V store value/type mismatch");
            ++stores;
        } else
            qualified(pure.contains(i.op), "SPIR-V operation lacks an external buffer-effect proof");
    }
    return {{"status", "qualified_no_external_buffer_writes"},
            {"reason", nullptr},
            {"loads", loads},
            {"local_or_output_stores", stores}};
}
} // namespace

Json inspect_spirv_uniform_reads(std::span<const std::byte> bytes, std::string_view entry, std::uint32_t model) {
    Json result;
    try {
        auto module = parse(bytes);
        result = reflect(module, entry, model);
    } catch(const Unsupported& error) {
        result = {{"status", "unavailable"}, {"reason", error.what()}, {"reads", Json::array()}};
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Invalid reflected SPIR-V text or layout");
    }
    result["module_sha256"] = sha256(bytes);
    result["entry_point"] = entry;
    result["execution_model"] = model;
    result["scope"] = "Static uniform-load type/layout only; no resource identity, upload/lifetime proof or GPU "
                      "invocation observation";
    return result;
}

Json inspect_spirv_buffer_effects(std::span<const std::byte> bytes, std::string_view entry, std::uint32_t model) {
    Json result;
    try {
        auto module = parse(bytes);
        result = buffer_effects(module, entry, model);
    } catch(const Unsupported& error) {
        result = {{"status", "unavailable"}, {"reason", error.what()}};
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Invalid SPIR-V buffer-effect input");
    }
    result["module_sha256"] = sha256(bytes);
    result["entry_point"] = entry;
    result["execution_model"] = model;
    result["scope"] = "Static instruction/storage proof only; no resource alias, GPU visibility or invocation count";
    return result;
}

Json decode_spirv_uniform_read(const Json& read, std::span<const std::byte> bytes) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    const auto& layout = read.at("layout");
    valid(read.at("offset").is_number_unsigned() && layout.at("lanes").is_number_unsigned() &&
              layout.at("bytes").is_number_unsigned() && layout.at("scalar").is_string(),
          "Unqualified uniform decode layout fields");
    const auto offset = read.at("offset").get<std::uint64_t>(), lanes = layout.at("lanes").get<std::uint64_t>();
    const auto scalar = layout.at("scalar").get<std::string>();
    valid((scalar == "float32" || scalar == "int32" || scalar == "uint32") && lanes >= 1 && lanes <= 4 &&
              layout.at("bytes") == lanes * 4 && offset <= bytes.size() && lanes * 4 <= bytes.size() - offset,
          "Uniform decode range or scalar type is unqualified");
    Json values = Json::array();
    for(std::uint64_t lane = 0; lane < lanes; ++lane) {
        const auto bits = word(bytes, offset + lane * 4);
        Json value = bits;
        std::string classification = "integer";
        if(scalar == "int32")
            value = std::bit_cast<std::int32_t>(bits);
        else if(scalar == "float32") {
            const auto number = std::bit_cast<float>(bits);
            value = std::isfinite(number) ? Json(number) : Json(nullptr);
            classification = std::isnan(number) ? "nan" : std::isinf(number) ? "infinity" : "finite";
        }
        values.push_back({{"bits", bits}, {"value", value}, {"classification", classification}});
    }
    return {{"offset", offset}, {"bytes", lanes * 4}, {"scalar", scalar}, {"components", std::move(values)}};
}
} // namespace ngm
