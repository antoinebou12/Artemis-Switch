#pragma once

#include "BenchmarkExport.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace artemis::benchmark {

struct BenchmarkExportPaths {
    std::filesystem::path json;
    std::filesystem::path csv;
};

class BenchmarkFileStore {
public:
    static std::string safeStem(const std::string& requestedStem);
    static std::optional<BenchmarkExportPaths>
    save(const std::filesystem::path& directory,
         const std::string& requestedStem,
         const ExportProfile& profile,
         const ExportSummary& summary);
};

} // namespace artemis::benchmark
