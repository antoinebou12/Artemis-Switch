#include "../app/src/vpn/VpnFileLogger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

} // namespace

int main() {
    const auto base = std::filesystem::temp_directory_path() /
                      "artemis-vpn-file-logger-test.log";
    const auto backup = std::filesystem::path(base.string() + ".1");
    std::error_code error;
    std::filesystem::remove(base, error);
    std::filesystem::remove(backup, error);

    assert(VpnFileLogger::append(base.string(), "WireGuard",
                                 VpnFileLogger::Severity::Info,
                                 "first entry"));
    assert(VpnFileLogger::append(base.string(), "NetBird",
                                 VpnFileLogger::Severity::Warning,
                                 "second entry"));
    auto content = read_file(base);
    assert(content.find("[INFO] [WireGuard] first entry") != std::string::npos);
    assert(content.find("[WARN] [NetBird] second entry") != std::string::npos);

    const std::string privateKey = "PrivateKey = super-secret-key";
    assert(VpnFileLogger::append(base.string(), "WireGuard",
                                 VpnFileLogger::Severity::Error, privateKey));
    content = read_file(base);
    assert(content.find("super-secret-key") == std::string::npos);
    assert(content.find("PrivateKey = <redacted>") != std::string::npos);

    const std::string filler(VpnFileLogger::kMaxLogBytes, 'x');
    assert(VpnFileLogger::append(base.string(), "NetBird",
                                 VpnFileLogger::Severity::Info, filler));
    assert(VpnFileLogger::append(base.string(), "NetBird",
                                 VpnFileLogger::Severity::Info, "after rotation"));
    assert(std::filesystem::exists(backup));
    assert(read_file(base).find("after rotation") != std::string::npos);
    assert(read_file(backup).find('x') != std::string::npos);

    std::filesystem::remove(base, error);
    std::filesystem::remove(backup, error);
    return 0;
}

