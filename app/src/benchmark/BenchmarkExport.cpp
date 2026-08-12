#include "BenchmarkExport.hpp"

#include <chrono>
#include <ctime>
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

const char* boolJson(bool value) {
    return value ? "true" : "false";
}

std::string isoUtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

void writeDistJson(std::ostringstream& out, const char* key,
                   const DistributionSummary& d) {
    out << "    \"" << key << "\": {\"mean\": " << d.mean
        << ", \"median\": " << d.median
        << ", \"p95\": " << d.p95
        << ", \"p99\": " << d.p99
        << ", \"min\": " << d.min
        << ", \"max\": " << d.max << "}";
}

void writeDistCsv(std::ostringstream& out, const DistributionSummary& d) {
    out << ',' << d.mean << ',' << d.median << ',' << d.p95 << ',' << d.p99
        << ',' << d.min << ',' << d.max;
}
}

std::string BenchmarkExport::toJson(const ExportProfile& profile,
                                    const ExportSummary& summary) {
    return toJson(profile, summary, collectSwitchRuntimeMetadata());
}

std::string BenchmarkExport::toJson(const ExportProfile& p,
                                    const ExportSummary& s,
                                    const SwitchRuntimeMetadata& r) {
    const std::string captured =
        s.capturedAtIso.empty() ? isoUtcNow() : s.capturedAtIso;
    const BenchmarkSummary& st = s.stats;

    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << "{\n"
        << "  \"profile\": {\"width\": " << p.width
        << ", \"height\": " << p.height
        << ", \"fps\": " << p.fps
        << ", \"bitrate_kbps\": " << p.bitrateKbps
        << ", \"decoder_threads\": " << p.decoderThreads
        << ", \"codec\": \"" << jsonEscape(p.codec) << "\"},\n"
        << "  \"runtime\": {\"platform\": \"" << jsonEscape(r.platform)
        << "\", \"operation_mode\": \"" << jsonEscape(r.operationMode)
        << "\", \"battery_percent\": " << r.batteryPercent
        << ", \"battery_charging_enabled\": ";

    if (r.batteryChargingKnown)
        out << boolJson(r.batteryCharging);
    else
        out << "null";

    out << ", \"cpu_clock_hz\": " << r.cpuClockHz
        << ", \"gpu_clock_hz\": " << r.gpuClockHz
        << ", \"memory_clock_hz\": " << r.memoryClockHz << "},\n"
        << "  \"summary\": {\"duration_seconds\": " << s.durationSeconds
        << ", \"rendered_fps_mean\": " << s.renderedFpsMean
        << ", \"rendered_fps_p99\": " << s.renderedFpsP99
        << ", \"network_drop_percent\": " << s.networkDropPercent
        << ", \"receive_ms_p99\": " << s.receiveMsP99
        << ", \"decode_ms_p99\": " << s.decodeMsP99
        << ", \"client_processing_ms_p99\": " << s.clientProcessingMsP99
        << ", \"stability_score\": " << s.stabilityScore << "},\n"
        << "  \"stats\": {\n"
        << "    \"captured_at\": \"" << jsonEscape(captured) << "\",\n"
        << "    \"host\": \"" << jsonEscape(s.hostName) << "\",\n"
        << "    \"app\": \"" << jsonEscape(s.appName) << "\",\n"
        << "    \"sample_count\": " << st.sampleCount << ",\n"
        << "    \"duration_seconds\": " << st.durationSeconds << ",\n";
    writeDistJson(out, "host_fps", st.hostFps);
    out << ",\n";
    writeDistJson(out, "received_fps", st.receivedFps);
    out << ",\n";
    writeDistJson(out, "decoded_fps", st.decodedFps);
    out << ",\n";
    writeDistJson(out, "rendered_fps", st.renderedFps);
    out << ",\n";
    writeDistJson(out, "receive_ms", st.receiveMs);
    out << ",\n";
    writeDistJson(out, "decode_ms", st.decodeMs);
    out << ",\n";
    writeDistJson(out, "decoder_delay_ms", st.decoderDelayMs);
    out << ",\n";
    writeDistJson(out, "render_ms", st.renderMs);
    out << ",\n";
    writeDistJson(out, "gpu_render_ms", st.gpuRenderMs);
    out << ",\n";
    writeDistJson(out, "client_processing_ms", st.clientProcessingMs);
    out << ",\n";
    writeDistJson(out, "queue_depth", st.queueDepth);
    out << ",\n";
    writeDistJson(out, "queue_jitter_ms", st.queueJitterMs);
    out << ",\n";
    writeDistJson(out, "queue_wait_ms", st.queueWaitMs);
    out << ",\n";
    writeDistJson(out, "client_pipeline_ms", st.clientPipelineMs);
    out << ",\n";
    writeDistJson(out, "present_interval_ms", st.presentIntervalMs);
    out << ",\n";
    writeDistJson(out, "actual_video_mbps", st.actualVideoMbps);
    out << ",\n"
        << "    \"received_frames\": " << st.receivedFrames << ",\n"
        << "    \"network_dropped_frames\": " << st.networkDroppedFrames << ",\n"
        << "    \"network_drop_percent\": " << st.networkDropPercent << ",\n"
        << "    \"queue_underflows\": " << st.queueUnderflows << ",\n"
        << "    \"queue_skipped_frames\": " << st.queueSkippedFrames << ",\n"
        << "    \"queue_overflow_drops\": " << st.queueOverflowDrops << ",\n"
        << "    \"queue_pacing_skips\": " << st.queuePacingSkips << ",\n"
        << "    \"queue_resyncs\": " << st.queueResyncs << ",\n"
        << "    \"time_to_first_packet_ms\": " << st.timeToFirstPacketMs << ",\n"
        << "    \"time_to_first_decode_ms\": " << st.timeToFirstDecodeMs << ",\n"
        << "    \"time_to_first_present_ms\": " << st.timeToFirstPresentMs << ",\n"
        << "    \"stability_score\": " << st.stabilityScore << "\n"
        << "  }\n"
        << "}\n";
    return out.str();
}

std::string BenchmarkExport::csvHeader() {
    return "width,height,fps,bitrate_kbps,decoder_threads,codec,duration_seconds,"
           "rendered_fps_mean,rendered_fps_p99,network_drop_percent,receive_ms_p99,"
           "decode_ms_p99,client_processing_ms_p99,stability_score,platform,"
           "operation_mode,battery_percent,battery_charging_enabled,cpu_clock_hz,"
           "gpu_clock_hz,memory_clock_hz,captured_at,host,app,sample_count,"
           "received_frames,network_dropped_frames,queue_underflows,"
           "queue_skipped_frames,queue_overflow_drops,queue_pacing_skips,"
           "queue_resyncs,"
           "stats_host_fps_mean,stats_host_fps_median,stats_host_fps_p95,"
           "stats_host_fps_p99,stats_host_fps_min,stats_host_fps_max,"
           "stats_received_fps_mean,stats_received_fps_median,"
           "stats_received_fps_p95,stats_received_fps_p99,"
           "stats_received_fps_min,stats_received_fps_max,"
           "stats_decoded_fps_mean,stats_decoded_fps_median,"
           "stats_decoded_fps_p95,stats_decoded_fps_p99,"
           "stats_decoded_fps_min,stats_decoded_fps_max,"
           "stats_rendered_fps_mean,stats_rendered_fps_median,"
           "stats_rendered_fps_p95,stats_rendered_fps_p99,"
           "stats_rendered_fps_min,stats_rendered_fps_max,"
           "stats_receive_ms_mean,stats_receive_ms_median,stats_receive_ms_p95,"
           "stats_receive_ms_p99,stats_receive_ms_min,stats_receive_ms_max,"
           "stats_decode_ms_mean,stats_decode_ms_median,stats_decode_ms_p95,"
           "stats_decode_ms_p99,stats_decode_ms_min,stats_decode_ms_max,"
           "stats_decoder_delay_ms_mean,stats_decoder_delay_ms_median,"
           "stats_decoder_delay_ms_p95,stats_decoder_delay_ms_p99,"
           "stats_decoder_delay_ms_min,stats_decoder_delay_ms_max,"
           "stats_render_ms_mean,stats_render_ms_median,stats_render_ms_p95,"
           "stats_render_ms_p99,stats_render_ms_min,stats_render_ms_max,"
           "stats_gpu_render_ms_mean,stats_gpu_render_ms_median,"
           "stats_gpu_render_ms_p95,stats_gpu_render_ms_p99,"
           "stats_gpu_render_ms_min,stats_gpu_render_ms_max,"
           "stats_client_processing_ms_mean,stats_client_processing_ms_median,"
           "stats_client_processing_ms_p95,stats_client_processing_ms_p99,"
           "stats_client_processing_ms_min,stats_client_processing_ms_max\n";
}

std::string BenchmarkExport::toCsvRow(const ExportProfile& profile,
                                      const ExportSummary& summary) {
    return toCsvRow(profile, summary, collectSwitchRuntimeMetadata());
}

std::string BenchmarkExport::toCsvRow(const ExportProfile& p,
                                      const ExportSummary& s,
                                      const SwitchRuntimeMetadata& r) {
    const std::string captured =
        s.capturedAtIso.empty() ? isoUtcNow() : s.capturedAtIso;
    const BenchmarkSummary& st = s.stats;

    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << p.width << ',' << p.height << ',' << p.fps << ',' << p.bitrateKbps << ','
        << p.decoderThreads << ',' << csvEscape(p.codec) << ',' << s.durationSeconds << ','
        << s.renderedFpsMean << ',' << s.renderedFpsP99 << ',' << s.networkDropPercent << ','
        << s.receiveMsP99 << ',' << s.decodeMsP99 << ',' << s.clientProcessingMsP99 << ','
        << s.stabilityScore << ',' << csvEscape(r.platform) << ','
        << csvEscape(r.operationMode) << ',' << r.batteryPercent << ',';

    if (r.batteryChargingKnown)
        out << (r.batteryCharging ? "true" : "false");

    out << ',' << r.cpuClockHz << ',' << r.gpuClockHz << ',' << r.memoryClockHz
        << ',' << csvEscape(captured)
        << ',' << csvEscape(s.hostName)
        << ',' << csvEscape(s.appName)
        << ',' << st.sampleCount
        << ',' << st.receivedFrames
        << ',' << st.networkDroppedFrames
        << ',' << st.queueUnderflows
        << ',' << st.queueSkippedFrames
        << ',' << st.queueOverflowDrops
        << ',' << st.queuePacingSkips
        << ',' << st.queueResyncs;
    writeDistCsv(out, st.hostFps);
    writeDistCsv(out, st.receivedFps);
    writeDistCsv(out, st.decodedFps);
    writeDistCsv(out, st.renderedFps);
    writeDistCsv(out, st.receiveMs);
    writeDistCsv(out, st.decodeMs);
    writeDistCsv(out, st.decoderDelayMs);
    writeDistCsv(out, st.renderMs);
    writeDistCsv(out, st.gpuRenderMs);
    writeDistCsv(out, st.clientProcessingMs);
    out << '\n';
    return out.str();
}

} // namespace artemis::benchmark
