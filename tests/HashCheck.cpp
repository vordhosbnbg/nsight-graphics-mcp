#include "Check.hpp"
#include "ngm/Hash.hpp"

#include <string>

int main() {
    return ngm::check::run([] {
        const auto hash = [](const std::string& text) { return ngm::sha256(std::as_bytes(std::span(text))); };
        ngm::check::require(hash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                            "empty SHA-256 known answer");
        ngm::check::require(hash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                            "short SHA-256 known answer");
        ngm::check::require(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                            "padding crossing a SHA-256 block");
        ngm::check::require(hash(std::string(1000000, 'a')) ==
                                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
                            "million-byte SHA-256 known answer");
    });
}
