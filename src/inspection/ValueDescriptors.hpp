#pragma once

#include "CppReplay.hpp"
#include <set>

namespace ngm::detail {
// Source-local descriptor history. Allocations come from the bounded source
// interpreter; writes must already be authenticated/typed by DescriptorRead.
// The caller accounts for all recordings, completions and resource effects and
// discards the whole composition if any operation throws. No GPU claim is made.
class ValueDescriptors {
public:
    explicit ValueDescriptors(const nlohmann::json& allocations);
    void update(const ReplayCall&, const nlohmann::json& writes, const nlohmann::json& read_evidence = nullptr);
    void host(const ReplayCall&);
    void recorded(const std::string& recording, const std::set<std::string>& sets);
    void forget(const std::string& recording);
    void submitted(const std::string& recording);
    void completed(const std::string& recording);
    void require_recording(const std::string& recording) const;
    nlohmann::json element(const std::string& set, std::uint64_t binding, std::uint64_t index) const;

private:
    struct Entry {
        nlohmann::json value = nullptr, history = nlohmann::json::array();
    };
    struct Set {
        nlohmann::json allocation;
        std::map<std::pair<std::uint64_t, std::uint64_t>, Entry> entries;
        bool alive = true;
    };
    struct Recording {
        std::set<std::string> sets;
        std::size_t pending = 0;
        std::string invalid;
    };
    const nlohmann::json& binding(const std::string&, std::uint64_t, std::uint64_t first, std::uint64_t count) const;
    Set& live(const std::string&);
    void changed(const std::string&);
    void check_budget() const;
    std::map<std::string, Set> sets_;
    std::map<std::string, Recording> recordings_;
    std::size_t updates_ = 0;
};
} // namespace ngm::detail
