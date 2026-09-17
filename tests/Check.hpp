#pragma once

#include <exception>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ngm::check {
inline void require(bool condition, std::string_view message,
                    std::source_location location = std::source_location::current()) {
    if(!condition) {
        throw std::runtime_error(std::string(location.file_name()) + ':' + std::to_string(location.line()) + ": " +
                                 std::string(message));
    }
}

template <typename Function>
int run(Function&& function) {
    try {
        function();
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch(...) {
        std::cerr << "Unknown check failure\n";
        return 1;
    }
}
} // namespace ngm::check
