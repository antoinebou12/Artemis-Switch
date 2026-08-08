#include "BenchmarkExport.hpp"

#include <iomanip>
#include <sstream>

namespace artemis::benchmark {
namespace {
std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(c) << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

std::string csvEscape(const std::string& value) {
    const bool quote = value.find_first_of(",\"\r\n") != std::string::npos;
    if (!quote)
        return value;

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char c : value) {
        if (c == '"')
            escaped.push_back('"');
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}
}

std::string BenchmarkExport::toJson(const ExportProfile& p,
                                    const ExportSummary& s) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << "{\n"
        << "  \"profile\": {\"width\": " << p.width
        << ", \"height\": " << p.height
        << ", \"fps\": " << p.fps
        << ", \"bitrate_kbps\": " << p.bitrateKbps
        << ", \"decoder_threads\": " << p.decoderThreads
        << ", \"codec\": \"" << jsonEscape(p.codec) << "\"},\n"
        << "  \"summary\": {\"duration_seconds\": " << s.durationSeconds
        << ", \"rendered_fps_mean\": " << s.renderedFpsMean
        << ", \"rendered_fps_p99\": " << s.renderedFpsP99
        << ", \"network_drop_percent\": " << s.networkDropPercent
        << ", \"receive_ms_p99\": " << s.receiveMsP99
        << ", \"decode_ms_p99\": " << s.decodeMsP99
        << ", \"client_processing_ms_p99\": " << s.clientProcessingMsP99
        << ", \"stability_score\": " << s.stabilityScore << "}\n"
        << "}\n";
    return out.str();
}

std::string BenchmarkExport::csvHeader() {
    return "width,height,fps,bitrate_kbps,decoder_threads,codec,duration_seconds,"
           "rendered_fps_mean,rendered_fps_p99,network_drop_percent,receive_ms_p99,"
           "decode_ms_p99,client_processing_ms_p99,stability_score\n";
}

std::string BenchmarkExport::toCsvRow(const ExportProfile& p,
                                      const ExportSummary& s) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << p.width << ',' << p.height << ',' << p.fps << ',' << p.bitrateKbps << ','
        << p.decoderThreads << ',' << csvEscape(p.codec) << ',' << s.durationSeconds << ','
        << s.renderedFpsMean << ',' << s.renderedFpsP99 << ',' << s.networkDropPercent << ','
        << s.receiveMsP99 << ',' << s.decodeMsP99 << ',' << s.clientProcessingMsP99 << ','
        << s.stabilityScore << '\n';
    return out.str();
}

} // namespace artemis::benchmark
