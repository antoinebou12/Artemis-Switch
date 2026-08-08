#include "performance_tab.hpp"

#include "AVFrameHolder.hpp"
#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include "features/performance/PerformanceLite.hpp"

#if __has_include("benchmark/BenchmarkRuntime.hpp")
#include "benchmark/BenchmarkRuntime.hpp"
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 1
#else
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 0
#endif

#if __has_include("streaming/StreamProfileStore.hpp")
#include "streaming/StreamProfileStore.hpp"
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 1
#else
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 0
#endif

#include <algorithm>
#include <fmt/format.h>

using namespace brls;

namespace {
std::string configuredResolutionText() {
#if ARTEMIS_HAS_CUSTOM_STREAM_PROFILE
    const auto custom = artemis::streaming::StreamProfileStore::instance().get();
    if (custom.customResolutionEnabled)
        return fmt::format("{}x{}", custom.width, custom.height);
#endif

    const int resolution = Settings::instance().resolution();
    if (resolution == -1)
        return "Native";
    return fmt::format("{}x{}", resolution * 16 / 9, resolution);
}
}

PerformanceTab::PerformanceTab() {
    inflateFromXMLRes("xml/views/ingame_overlay/performance_tab.xml");

    streamProfile->setText("Stream profile");
    network->setText("Configured bitrate");
    receiveLatency->setText("Receive latency");
    decodeLatency->setText("Decode latency");
    renderLatency->setText("Render latency");
    packetLoss->setText("Packet loss");
    renderedFps->setText("Rendered FPS");
    queueDepth->setText("Frame queue");
    benchmarkSummary->setText("Benchmark");
    benchmarkAction->setText("Start benchmark");
    refreshButton->setText("Refresh performance data");

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkAction->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        if (runtime.running()) {
            const auto result = runtime.stop();
            benchmarkSummary->setDetailText(fmt::format(
                "Stopped: {:.1f}/100, {:.2f}% loss, P99 {:.2f} ms",
                result.stabilityScore, result.networkDropPercent,
                result.clientProcessingMs.p99));
        } else {
            runtime.start(Settings::instance().fps());
        }
        updateBenchmarkStatus();
        return true;
    });
#else
    benchmarkSummary->setDetailText("Benchmark core not present in this branch");
    benchmarkAction->setEnabled(false);
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
        benchmarkAction->setText("Stop benchmark");
        benchmarkSummary->setDetailText(fmt::format(
            "Running: {} samples, {:.1f}/100 stability",
            runtime.sampleCount(), summary.stabilityScore));
    } else {
        benchmarkAction->setText("Start benchmark");
        if (benchmarkSummary->getDetailText().empty())
            benchmarkSummary->setDetailText("Ready");
    }
#endif
}

void PerformanceTab::refresh() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active()) {
        streamProfile->setDetailText("No active stream");
        network->setDetailText("-");
        receiveLatency->setDetailText("-");
        decodeLatency->setDetailText("-");
        renderLatency->setDetailText("-");
        packetLoss->setDetailText("-");
        renderedFps->setDetailText("-");
        queueDepth->setDetailText("-");
        benchmarkAction->setEnabled(false);
        return;
    }

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkAction->setEnabled(true);
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
    renderLatency->setDetailText(fmt::format("{:.2f} ms", render.rendering_time));
    packetLoss->setDetailText(lite.packetLossText);
    renderedFps->setDetailText(lite.fpsText);
    queueDepth->setDetailText(fmt::format("{} / {} / {}",
        AVFrameHolder::instance().getFrameQueueSize(),
        AVFrameHolder::instance().getFrameQueueTargetDepth(),
        AVFrameHolder::instance().getFrameQueueCapacity()));

    const auto color = lite.healthy
        ? Application::getTheme()["brls/list/listItem_value_color"]
        : Application::getTheme()["brls/accent"];
    packetLoss->setDetailTextColor(color);
    renderedFps->setDetailTextColor(color);

    updateBenchmarkStatus();
}
