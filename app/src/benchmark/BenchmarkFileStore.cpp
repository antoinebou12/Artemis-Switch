#include "BenchmarkFileStore.hpp"

#include <cctype>
#include <fstream>

namespace artemis::benchmark {

std::string BenchmarkFileStore::safeStem(const std::string& requestedStem) {
    std::string result;
    result.reserve(requestedStem.size());
    for (unsigned char c : requestedStem) {
        if (std::isalnum(c) || c == '-' || c == '_')
            result.push_back(static_cast<char>(c));
        else
            result.push_back('_');
    }

    while (!result.empty() && result.front() == '_')
        result.erase(result.begin());
    while (!result.empty() && result.back() == '_')
        result.pop_back();

    return result.empty() ? "benchmark" : result;
}

std::optional<BenchmarkExportPaths>
BenchmarkFileStore::save(const std::filesystem::path& directory,
                         const std::string& requestedStem,
                         const ExportProfile& profile,
                         const ExportSummary& summary) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
        return std::nullopt;

    const std::string stem = safeStem(requestedStem);
    BenchmarkExportPaths paths{
        directory / (stem + ".json"),
        directory / (stem + ".csv")
    };

    // Snapshot device state once so the paired JSON/CSV files represent the
    // same Switch operation mode, battery state, and clock readings.
    const auto runtime = collectSwitchRuntimeMetadata();

    {
        std::ofstream output(paths.json, std::ios::binary | std::ios::trunc);
        if (!output)
            return std::nullopt;
        output << BenchmarkExport::toJson(profile, summary, runtime);
        if (!output.good())
            return std::nullopt;
    }

    {
        std::ofstream output(paths.csv, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::filesystem::remove(paths.json, error);
            return std::nullopt;
        }
        output << BenchmarkExport::csvHeader()
               << BenchmarkExport::toCsvRow(profile, summary, runtime);
        if (!output.good()) {
            std::filesystem::remove(paths.json, error);
            std::filesystem::remove(paths.csv, error);
            return std::nullopt;
        }
    }

    return paths;
}

} // namespace artemis::benchmark
