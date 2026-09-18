#include "Check.hpp"
#include "ngm/Hash.hpp"

#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

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
        struct Scratch {
            std::filesystem::path path;
            Scratch() {
                auto pattern = (std::filesystem::temp_directory_path() / "ngm-hash-check-XXXXXX").string();
                const auto created = mkdtemp(pattern.data());
                ngm::check::require(created != nullptr, "allocate isolated hash files");
                path = created;
            }
            ~Scratch() {
                std::error_code error;
                std::filesystem::remove_all(path, error);
            }
        } scratch;
        const auto regular = scratch.path / "regular";
        std::ofstream(regular) << "abc";
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        ngm::check::require(ngm::sha256_regular_file(regular, deadline) == hash("abc"),
                            "regular-file hashing retains the known answer");
        const auto rejected = [&](const std::filesystem::path& path, auto until, std::stop_token stop = {}) {
            try {
                ngm::sha256_regular_file(path, until, stop);
            } catch(const std::runtime_error&) {
                return;
            }
            ngm::check::require(false, "unsafe or cancelled hash input must be rejected");
        };
        const auto fifo = scratch.path / "fifo";
        ngm::check::require(mkfifo(fifo.c_str(), 0600) == 0, "create FIFO with no writer");
        rejected(fifo, deadline);
        std::filesystem::create_symlink(regular, scratch.path / "link");
        rejected(scratch.path / "link", deadline);
        rejected(scratch.path, deadline);
        rejected(regular, std::chrono::steady_clock::now());
        std::stop_source stopped;
        stopped.request_stop();
        rejected(regular, deadline, stopped.get_token());
    });
}
