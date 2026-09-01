#include "../app/src/features/diagnostics/RotatingFileLog.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using artemis::diagnostics::RotatingFileLog;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

} // namespace

int main() {
    const auto base = std::filesystem::temp_directory_path() /
                      "artemis-rotating-file-log-test.log";
    const auto backup = std::filesystem::path(base.string() + ".1");
    std::error_code error;
    std::filesystem::remove(base, error);
    std::filesystem::remove(backup, error);

    assert(!RotatingFileLog::append("", "Main",
                                    RotatingFileLog::Severity::Info,
                                    "empty path"));
    // Empty message is permitted (only an empty path is rejected).
    assert(RotatingFileLog::append(base.string(), "Main",
                                   RotatingFileLog::Severity::Info, ""));

    assert(RotatingFileLog::append(base.string(), "Main",
                                   RotatingFileLog::Severity::Info,
                                   "hello world"));
    auto content = read_file(base);
    assert(content.find("[INFO] [Main] hello world") != std::string::npos);

    assert(RotatingFileLog::append(base.string(), "Decoder",
                                   RotatingFileLog::Severity::Error,
                                   "PrivateKey = top-secret"));
    content = read_file(base);
    assert(content.find("top-secret") == std::string::npos);
    assert(content.find("PrivateKey = <redacted>") != std::string::npos);

    // Rotation: crossing the size cap moves the current file to ".1".
    const std::string filler(RotatingFileLog::kMaxLogBytes, 'x');
    assert(RotatingFileLog::append(base.string(), "Main",
                                   RotatingFileLog::Severity::Info, filler));
    content = read_file(base);
    assert(content.find('x') != std::string::npos);
    assert(RotatingFileLog::append(base.string(), "Main",
                                   RotatingFileLog::Severity::Info,
                                   "after rotation"));
    assert(std::filesystem::exists(backup));
    assert(read_file(base).find("after rotation") != std::string::npos);
    assert(read_file(backup).find('x') != std::string::npos);

    std::filesystem::remove(base, error);
    std::filesystem::remove(backup, error);
    return 0;
}