#pragma once

#include "CppReplay.hpp"
#include <set>

namespace ngm::detail {
// Internal source-effect model. The composing inspector must authenticate the
// exact exported helper implementation and account for every submitted command
// and shader effect. This class neither reads payloads nor observes GPU values.
class ValueUploads {
public:
    explicit ValueUploads(const nlohmann::json& memory_graph);
    void host(const ReplayCall&, bool initialization = false);
    void submit(const std::string& queue, const std::string& device, const std::string& fence,
                const std::set<std::string>& buffers, const nlohmann::json& source);
    void invalidate(const std::string& buffer, const std::string& reason);
    nlohmann::json range(const std::string& buffer, std::uint64_t offset, std::uint64_t length) const;

private:
    struct Write {
        std::uint64_t offset, bytes;
        nlohmann::json payload, source;
        std::string kind;
        nlohmann::json completions;
    };
    struct Buffer {
        nlohmann::json binding;
        std::vector<Write> writes;
        std::string reason;
        bool alive = true;
        nlohmann::json completions = nlohmann::json::array();
    };
    struct Mapping {
        std::string device, pointer;
        std::uint64_t offset, bytes;
        nlohmann::json source;
    };
    struct Pending {
        std::string queue, device, fence;
        std::set<std::string> buffers;
        nlohmann::json source;
    };
    Buffer& buffer(const std::string&);
    bool pending(const std::string&) const;
    void write(const std::string&, std::uint64_t offset, std::uint64_t bytes, nlohmann::json payload, const ReplayCall&,
               const std::string& kind);
    std::map<std::string, Buffer> buffers_;
    std::map<std::string, Mapping> mappings_;
    std::vector<Pending> pending_;
    std::size_t writes_ = 0;
};
} // namespace ngm::detail
