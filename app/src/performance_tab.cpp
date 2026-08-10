#include "performance_tab.hpp"

#include "AVFrameHolder.hpp"
#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include "features/performance/PerformanceLite.hpp"
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#include "video/VideoScaleStore.hpp"

#if __has_include("benchmark/BenchmarkRuntime.hpp")
#include "benchmark/BenchmarkRuntime.hpp"
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 1
#else
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 0
#endif

#if ARTEMIS_HAS_BENCHMARK_RUNTIME && __has_include("benchmark/BenchmarkFileStore.hpp")
#include "benchmark/BenchmarkFileStore.hpp"
#define ARTEMIS_HAS_BENCHMARK_EXPORT 1
#else
#define ARTEMIS_HAS_BENCHMARK_EXPORT 0
#endif

#if __has_include("benchmark/AutoTuneRuntime.hpp")
#include "benchmark/AutoTuneRuntime.hpp"
#define ARTEMIS_HAS_AUTO_TUNE 1
#else
#define ARTEMIS_HAS_AUTO_TUNE 0
#endif

#if __has_include("streaming/StreamProfileStore.hpp")
#include "streaming/StreamProfileStore.hpp"
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 1
#else
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 0
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <utility>

using namespace brls;

namespace {
std::pair<int, int> configuredDimensions() {
#if ARTEMIS_HAS_CUSTOM_STREAM_PROFILE
    const auto custom = artemis::streaming::StreamProfileStore::instance().get();
    if (custom.customResolutionEnabled)
        return {custom.width, custom.height};
#endif

    const int resolution = Settings::instance().resolution();
    if (resolution == -1)
        return {Application::windowWidth, Application::windowHeight};
    return {resolution * 16 / 9, resolution};
}

std::string configuredResolutionText() {
    const auto [width, height] = configuredDimensions();
    if (Settings::instance().resolution() == -1
#if ARTEMIS_HAS_CUSTOM_STREAM_PROFILE
        && !artemis::streaming::StreamProfileStore::instance().get().customResolutionEnabled
#endif
    )
        return fmt::format("{} ({}x{})", "settings/resolution_native"_i18n, width, height);
    return fmt::format("{}x{}", width, height);
}

std::string presentationText() {
    const auto mode = artemis::video::VideoScaleStore::instance().get();
    const char* scale = mode == artemis::video::ScaleMode::Fit ? "Fit"
                      : mode == artemis::video::ScaleMode::Stretch ? "Stretch"
                                                                   : "Fill";
    const bool fullRange =
        artemis::stream::AdvancedStreamOptionsStore::instance()
            .get()
            .forceFullRangeVideo;
    return fmt::format("{} | {} range", scale, fullRange ? "Full" : "Limited");
}

std::string rendererTimingText(const VideoRenderStats& render) {
    return fmt::format(
        "Frame {:.2f} | Post {:.2f} | FSR {:.2f} | RCAS {:.2f} | Dither {:.2f} ms",
        render.rendering_time, render.post_processing_time, render.upscaling_time,
        render.sharpening_time, render.dithering_time);
}

#if ARTEMIS_HAS_BENCHMARK_EXPORT
artemis::benchmark::ExportProfile exportProfile() {
    const auto [width, height] = configuredDimensions();
    artemis::benchmark::ExportProfile profile;
    profile.width = width;
    profile.height = height;
    profile.fps = Settings::instance().fps();
    profile.bitrateKbps = Settings::instance().bitrate();
    profile.decoderThreads = Settings::instance().decoder_threads();
    profile.codec = Settings::instance().video_codec() == H264 ? "H264" : "HEVC";
    return profile;
}

artemis::benchmark::ExportSummary exportSummary(
    const artemis::benchmark::BenchmarkSummary& summary) {
    artemis::benchmark::ExportSummary result;
    result.durationSeconds = summary.durationSeconds;
    result.renderedFpsMean = summary.renderedFps.mean;
    result.renderedFpsP99 = summary.renderedFps.p99;
    result.networkDropPercent = summary.networkDropPercent;
    result.receiveMsP99 = summary.receiveMs.p99;
    result.decodeMsP99 = summary.decodeMs.p99;
    result.clientProcessingMsP99 = summary.clientProcessingMs.p99;
    result.stabilityScore = summary.stabilityScore;
    return result;
}
#endif
}

PerformanceTab::PerformanceTab() {
    inflateFromXMLRes("xml/views/ingame_overlay/performance_tab.xml");

    streamProfile->setText("artemis/performance/stream_profile"_i18n);
    network->setText("artemis/performance/configured_bitrate"_i18n);
    receiveLatency->setText("artemis/performance/receive_latency"_i18n);
    decodeLatency->setText("artemis/performance/decode_latency"_i18n);
    renderLatency->setText("artemis/performance/render_latency"_i18n);
    packetLoss->setText("artemis/performance/packet_loss"_i18n);
    renderedFps->setText("artemis/performance/rendered_fps"_i18n);
    queueDepth->setText("artemis/performance/frame_queue"_i18n);
    benchmarkSummary->setText("artemis/performance/benchmark"_i18n);
    benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
    benchmarkSave->setText("artemis/performance/save_benchmark"_i18n);
    autoTuneSummary->setText("artemis/performance/auto_tune_status"_i18n);
    autoTuneAction->setText("artemis/performance/start_auto_tune"_i18n);
    refreshButton->setText("artemis/performance/refresh"_i18n);

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkSummary->setDetailText("artemis/performance/ready"_i18n);
    benchmarkAction->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        if (runtime.running()) {
            const auto result = runtime.stop();
            benchmarkSummary->setDetailText(fmt::format(
                "{:.1f}/100, {:.2f}% loss, P99 {:.2f} ms",
                result.stabilityScore, result.networkDropPercent,
                result.clientProcessingMs.p99));
        } else {
            runtime.start(Settings::instance().fps());
        }
        updateBenchmarkStatus();
        return true;
    });
#else
    benchmarkSummary->setDetailText("artemis/performance/benchmark_unavailable"_i18n);
    benchmarkAction->setEnabled(false);
#endif

#if ARTEMIS_HAS_BENCHMARK_EXPORT
    benchmarkSave->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        const auto summary = runtime.snapshot();
        if (runtime.running() || summary.sampleCount < 2) {
            benchmarkSave->setDetailText("artemis/performance/stop_collect_first"_i18n);
            return true;
        }

        const auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto directory = std::filesystem::path(Settings::instance().working_dir()) /
                               "benchmarks";
        const auto paths = artemis::benchmark::BenchmarkFileStore::save(
            directory, fmt::format("benchmark_{}", epoch), exportProfile(),
            exportSummary(summary));
        benchmarkSave->setDetailText(paths
            ? paths->json.filename().string()
            : "artemis/performance/save_failed"_i18n);
        return true;
    });
#else
    benchmarkSave->setDetailText("artemis/performance/export_unavailable"_i18n);
    benchmarkSave->setEnabled(false);
#endif

#if ARTEMIS_HAS_AUTO_TUNE
    autoTuneAction->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::AutoTuneRuntime::instance();
        if (runtime.running())
            runtime.cancel();
        else
            runtime.start(false);
        updateAutoTuneStatus();
        return true;
    });
#else
    autoTuneSummary->setDetailText("artemis/performance/auto_tune_unavailable"_i18n);
    autoTuneAction->setEnabled(false);
#endif

    refreshButton->registerClickAction([this](View*) {
        refresh();
        return true;
    });

    refresh();
}

void PerformanceTab::updateBenchmarkStatus() {
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
    if (runtime.running()) {
        const auto summary = runtime.snapshot();
        benchmarkAction->setText("artemis/performance/stop_benchmark"_i18n);
        benchmarkSummary->setDetailText(fmt::format(
            "{} samples, {:.1f}/100",
            runtime.sampleCount(), summary.stabilityScore));
        benchmarkSave->setEnabled(false);
    } else {
        benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
#if ARTEMIS_HAS_BENCHMARK_EXPORT
        benchmarkSave->setEnabled(runtime.sampleCount() >= 2);
#endif
    }
#endif
}

void PerformanceTab::updateAutoTuneStatus() {
#if ARTEMIS_HAS_AUTO_TUNE
    auto& runtime = artemis::benchmark::AutoTuneRuntime::instance();
    if (runtime.running()) {
        autoTuneAction->setText("artemis/performance/cancel_auto_tune"_i18n);
        autoTuneSummary->setDetailText(fmt::format(
            "{} / {}", runtime.currentStep() + 1, runtime.totalSteps()));
        return;
    }

    autoTuneAction->setText("artemis/performance/start_auto_tune"_i18n);
    if (const auto best = runtime.recommendation()) {
        autoTuneSummary->setDetailText(fmt::format(
            "{}x{} @ {} FPS, {:.1f} Mbps, {} threads",
            best->profile.width, best->profile.height, best->profile.fps,
            static_cast<double>(best->profile.bitrateKbps) / 1000.0,
            best->profile.decoderThreads));
    } else if (runtime.available()) {
        autoTuneSummary->setDetailText("artemis/performance/auto_tune_ready"_i18n);
    } else {
        autoTuneSummary->setDetailText("artemis/performance/auto_tune_requires_stream"_i18n);
    }
#endif
}

void PerformanceTab::refresh() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active()) {
        streamProfile->setDetailText("artemis/performance/no_active_stream"_i18n);
        network->setDetailText("-");
        receiveLatency->setDetailText("-");
        decodeLatency->setDetailText("-");
        renderLatency->setDetailText("-");
        packetLoss->setDetailText("-");
        renderedFps->setDetailText("-");
        queueDepth->setDetailText("-");
        benchmarkAction->setEnabled(false);
        benchmarkSave->setEnabled(false);
        autoTuneAction->setEnabled(false);
        return;
    }

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkAction->setEnabled(true);
#endif
#if ARTEMIS_HAS_AUTO_TUNE
    autoTuneAction->setEnabled(artemis::benchmark::AutoTuneRuntime::instance().available() ||
                               artemis::benchmark::AutoTuneRuntime::instance().running());
#endif

    const auto* stats = session->session_stats();
    const auto& decode = stats->video_decode_stats;
    const auto& render = stats->video_render_stats;

    const uint64_t currentReceived = decode.total_received_frames;
    const uint64_t currentDropped = decode.network_dropped_frames;
    uint64_t receivedDelta = 0;
    uint64_t droppedDelta = 0;
    if (previousReceived != 0 || previousDropped != 0) {
        receivedDelta = currentReceived >= previousReceived
            ? currentReceived - previousReceived : 0;
        droppedDelta = currentDropped >= previousDropped
            ? currentDropped - previousDropped : 0;
    }
    previousReceived = currentReceived;
    previousDropped = currentDropped;

    const uint64_t networkTotal = receivedDelta + droppedDelta;
    const double lossPercent = networkTotal > 0
        ? static_cast<double>(droppedDelta) * 100.0 / static_cast<double>(networkTotal)
        : 0.0;

    artemis::performance::LitePerformanceSnapshot snapshot;
    snapshot.networkMbps = static_cast<double>(Settings::instance().bitrate()) / 1000.0;
    snapshot.receiveLatencyMs = decode.current_receive_time;
    snapshot.decodeLatencyMs = decode.current_decoding_time;
    snapshot.packetLossPercent = lossPercent;
    snapshot.renderedFps = render.rendered_fps;
    const auto lite = artemis::performance::buildLiteStatus(snapshot);

    streamProfile->setDetailText(fmt::format("{} @ {} FPS, {}",
        configuredResolutionText(), Settings::instance().fps(),
        getVideoCodecName(Settings::instance().video_codec())));
    network->setDetailText(lite.networkText);
    receiveLatency->setDetailText(lite.latencyText);
    decodeLatency->setDetailText(lite.decodeText);
    renderLatency->setDetailText(rendererTimingText(render));
    packetLoss->setDetailText(lite.packetLossText);
    renderedFps->setDetailText(lite.fpsText);

    const std::string gpuText = render.gpu_rendering_time > 0.0f
        ? fmt::format(" | GPU {:.2f} ms", render.gpu_rendering_time)
        : "";
    queueDepth->setDetailText(fmt::format("{} / {} / {}{} | {}",
        AVFrameHolder::instance().getFrameQueueSize(),
        AVFrameHolder::instance().getFrameQueueTargetDepth(),
        AVFrameHolder::instance().getFrameQueueCapacity(),
        gpuText, presentationText()));

    const auto color = lite.healthy
        ? Application::getTheme()["brls/list/listItem_value_color"]
        : Application::getTheme()["brls/accent"];
    packetLoss->setDetailTextColor(color);
    renderedFps->setDetailTextColor(color);

    updateBenchmarkStatus();
    updateAutoTuneStatus();
}
