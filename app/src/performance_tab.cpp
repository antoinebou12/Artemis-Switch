#include "performance_tab.hpp"

#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include "features/performance/PerformanceLite.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

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

#if __has_include("streaming/StreamProfileStore.hpp")
#include "streaming/StreamProfileStore.hpp"
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 1
#else
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 0
#endif

#if __has_include("AVFrameHolder.hpp")
#include "AVFrameHolder.hpp"
#define ARTEMIS_HAS_FRAME_QUEUE 1
#else
#define ARTEMIS_HAS_FRAME_QUEUE 0
#endif

#if __has_include("video/VideoScaleStore.hpp")
#include "video/VideoScaleStore.hpp"
#define ARTEMIS_HAS_VIDEO_SCALE 1
#else
#define ARTEMIS_HAS_VIDEO_SCALE 0
#endif

#if __has_include("features/stream/AdvancedStreamOptionsStore.hpp")
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#define ARTEMIS_HAS_ADVANCED_STREAM 1
#else
#define ARTEMIS_HAS_ADVANCED_STREAM 0
#endif

#if __has_include("benchmark/SwitchRuntimeMetadata.hpp")
#include "benchmark/SwitchRuntimeMetadata.hpp"
#define ARTEMIS_HAS_SWITCH_RUNTIME 1
#else
#define ARTEMIS_HAS_SWITCH_RUNTIME 0
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <utility>

using namespace brls;

namespace {
enum class ConnectionKind : int {
    Offline,
    Wifi,
    Ethernet,
};

std::atomic<int> currentConnectionKind{
    static_cast<int>(ConnectionKind::Offline)};
std::atomic<int> currentWifiSignal{0};
std::atomic<bool> connectionQueryPending{false};

void requestConnectionStatus() {
    if (connectionQueryPending.exchange(true))
        return;

    brls::async([] {
        ConnectionKind kind = ConnectionKind::Offline;
        int signal = 0;

#ifdef __SWITCH__
        NifmInternetConnectionType type{};
        NifmInternetConnectionStatus status{};
        u32 wifiSignal = 0;
        const Result result =
            nifmGetInternetConnectionStatus(&type, &wifiSignal, &status);
        if (R_SUCCEEDED(result)) {
            if (type == NifmInternetConnectionType_WiFi) {
                kind = ConnectionKind::Wifi;
                signal = static_cast<int>(wifiSignal);
            } else if (type == NifmInternetConnectionType_Ethernet) {
                kind = ConnectionKind::Ethernet;
            }
        }
#else
        auto* platform = Application::getPlatform();
        if (platform->hasEthernetConnection()) {
            kind = ConnectionKind::Ethernet;
        } else if (platform->hasWirelessConnection()) {
            kind = ConnectionKind::Wifi;
            signal = platform->getWirelessLevel();
        }
#endif

        currentWifiSignal.store(std::clamp(signal, 0, 3));
        currentConnectionKind.store(static_cast<int>(kind));
        connectionQueryPending.store(false);
    });
}

void setDetailTextIfChanged(DetailCell* cell, const std::string& text) {
    if (cell->detail->getFullText() != text)
        cell->setDetailText(text);
}

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

std::string scaleModeLabel() {
#if ARTEMIS_HAS_VIDEO_SCALE
    switch (artemis::video::VideoScaleStore::instance().get()) {
    case artemis::video::ScaleMode::Fit:
        return "Fit";
    case artemis::video::ScaleMode::Stretch:
        return "Stretch";
    case artemis::video::ScaleMode::Fill:
    default:
        return "Fill";
    }
#else
    return "Fill";
#endif
}

std::string colorRangeLabel() {
#if ARTEMIS_HAS_ADVANCED_STREAM
    return artemis::stream::AdvancedStreamOptionsStore::instance()
                   .get()
                   .forceFullRangeVideo
               ? "Full"
               : "Limited";
#else
    return "Limited";
#endif
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

    const std::array<DetailCell*, 22> compactRows = {
        network, wifiSignal, receiveLatency, decodeLatency, renderLatency,
        packetLoss, hostFps, receivedFps, decodedFps, renderedFps, frameQueue,
        gpuRender, presentation, operationMode, cpuClock, gpuClock, memoryClock,
        battery, benchmarkSummary, benchmarkAction, benchmarkSave, benchmarkReset};
    for (auto* row : compactRows) {
        row->title->setSingleLine(true);
        row->detail->setSingleLine(true);
    }

    network->setText("artemis/performance/configured_bitrate"_i18n);
    network->setFocusable(false);
    wifiSignal->setText("artemis/performance/wifi_signal"_i18n);
    receiveLatency->setText("artemis/performance/receive_latency"_i18n);
    decodeLatency->setText("artemis/performance/decode_latency"_i18n);
    renderLatency->setText("artemis/performance/render_latency"_i18n);
    packetLoss->setText("artemis/performance/packet_loss"_i18n);
    hostFps->setText("artemis/performance/host_fps"_i18n);
    receivedFps->setText("artemis/performance/received_fps"_i18n);
    decodedFps->setText("artemis/performance/decoded_fps"_i18n);
    renderedFps->setText("artemis/performance/rendered_fps"_i18n);
    frameQueue->setText("artemis/performance/frame_queue"_i18n);
    gpuRender->setText("artemis/performance/gpu_render"_i18n);
    presentation->setText("artemis/performance/presentation"_i18n);
    operationMode->setText("artemis/performance/operation_mode"_i18n);
    cpuClock->setText("artemis/performance/cpu_clock"_i18n);
    gpuClock->setText("artemis/performance/gpu_clock"_i18n);
    memoryClock->setText("artemis/performance/memory_clock"_i18n);
    battery->setText("artemis/performance/battery"_i18n);
    benchmarkSummary->setText("artemis/performance/benchmark"_i18n);
    benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
    benchmarkSave->setText("artemis/performance/save_benchmark"_i18n);
    benchmarkReset->setText("artemis/performance/reset_benchmark"_i18n);
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkSummary->setDetailText("artemis/performance/ready"_i18n);
    benchmarkAction->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        if (runtime.running()) {
            const auto result = runtime.stop();
            benchmarkSummary->setDetailText(fmt::format(
                "{:.0f}/100 · {:.2f}% · {:.1f} ms",
                result.stabilityScore, result.networkDropPercent,
                result.clientProcessingMs.p99));
        } else {
            runtime.start(Settings::instance().fps());
        }
        updateBenchmarkStatus();
        return true;
    });
    benchmarkReset->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        runtime.reset();
        benchmarkSummary->setDetailText(
            "artemis/performance/reset_done"_i18n);
        updateBenchmarkStatus();
        return true;
    });
#else
    benchmarkSummary->setDetailText("artemis/performance/benchmark_unavailable"_i18n);
    benchmarkAction->setFocusable(false);
    benchmarkReset->setFocusable(false);
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
    benchmarkSave->setFocusable(false);
#endif

    refresh();
    scheduleRefresh();
}

PerformanceTab::~PerformanceTab() {
    if (refreshTask != 0)
        cancelDelay(refreshTask);
}

void PerformanceTab::scheduleRefresh() {
    refreshTask = delay(250, [this] {
        refreshTask = 0;
        refresh();
        scheduleRefresh();
    });
}

void PerformanceTab::updateBenchmarkStatus() {
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
    if (runtime.running()) {
        const auto summary = runtime.snapshot();
        benchmarkAction->setText("artemis/performance/stop_benchmark"_i18n);
        benchmarkAction->setFocusable(true);
        benchmarkSummary->setDetailText(fmt::format(
            "{} · {:.0f}/100",
            runtime.sampleCount(), summary.stabilityScore));
        benchmarkSave->setFocusable(false);
        benchmarkReset->setFocusable(false);
    } else {
        benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
        benchmarkAction->setFocusable(true);
#if ARTEMIS_HAS_BENCHMARK_EXPORT
        benchmarkSave->setFocusable(runtime.sampleCount() >= 2);
#endif
        benchmarkReset->setFocusable(runtime.sampleCount() > 0);
    }
#endif
}

void PerformanceTab::refresh() {
    wifiQueryTicks++;
    if (wifiQueryTicks >= 8) {
        wifiQueryTicks = 0;
        requestConnectionStatus();
    }

    const auto connectionKind = static_cast<ConnectionKind>(
        currentConnectionKind.load());
    const int signalLevel = currentWifiSignal.load();
    if (connectionKind == ConnectionKind::Wifi) {
        setDetailTextIfChanged(wifiSignal,
                               fmt::format("{} / 3", signalLevel));
    } else if (connectionKind == ConnectionKind::Ethernet) {
        setDetailTextIfChanged(wifiSignal, "LAN");
    } else {
        setDetailTextIfChanged(wifiSignal, "-");
    }
    wifiGraph->pushSample(connectionKind == ConnectionKind::Wifi
        ? artemis::performance::normalizeWifiSignal(signalLevel)
        : 0.0f);
    setDetailTextIfChanged(network, fmt::format(
        "{:.1f} Mbps",
        static_cast<double>(Settings::instance().bitrate()) / 1000.0));

    auto* session = MoonlightSession::activeSession();

#if ARTEMIS_HAS_SWITCH_RUNTIME
    const auto runtime = artemis::benchmark::collectSwitchRuntimeMetadata();
    setDetailTextIfChanged(operationMode, runtime.operationMode);
    setDetailTextIfChanged(
        cpuClock,
        runtime.cpuClockHz == 0
            ? "-"
            : fmt::format("{:.0f} MHz", runtime.cpuClockHz / 1000000.0));
    setDetailTextIfChanged(
        gpuClock,
        runtime.gpuClockHz == 0
            ? "-"
            : fmt::format("{:.0f} MHz", runtime.gpuClockHz / 1000000.0));
    setDetailTextIfChanged(
        memoryClock,
        runtime.memoryClockHz == 0
            ? "-"
            : fmt::format("{:.0f} MHz", runtime.memoryClockHz / 1000000.0));
    if (runtime.batteryPercent < 0) {
        setDetailTextIfChanged(battery, "-");
    } else if (runtime.batteryChargingKnown) {
        setDetailTextIfChanged(
            battery,
            fmt::format("{}% · {}", runtime.batteryPercent,
                        runtime.batteryCharging
                            ? "artemis/performance/charging"_i18n
                            : "artemis/performance/on_battery"_i18n));
    } else {
        setDetailTextIfChanged(battery,
                               fmt::format("{}%", runtime.batteryPercent));
    }
#else
    setDetailTextIfChanged(operationMode, "-");
    setDetailTextIfChanged(cpuClock, "-");
    setDetailTextIfChanged(gpuClock, "-");
    setDetailTextIfChanged(memoryClock, "-");
    setDetailTextIfChanged(battery, "-");
#endif

    if (!session || !session->is_active()) {
        network->setFocusable(false);
        setDetailTextIfChanged(receiveLatency, "-");
        setDetailTextIfChanged(decodeLatency, "-");
        setDetailTextIfChanged(renderLatency, "-");
        setDetailTextIfChanged(packetLoss, "-");
        setDetailTextIfChanged(hostFps, "-");
        setDetailTextIfChanged(receivedFps, "-");
        setDetailTextIfChanged(decodedFps, "-");
        setDetailTextIfChanged(renderedFps, "-");
        setDetailTextIfChanged(frameQueue, "-");
        setDetailTextIfChanged(gpuRender, "-");
        setDetailTextIfChanged(presentation, "-");
        benchmarkAction->setFocusable(false);
        benchmarkSave->setFocusable(false);
        benchmarkReset->setFocusable(false);
        return;
    }

    network->setFocusable(false);

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkAction->setFocusable(true);
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
    snapshot.renderLatencyMs = render.rendering_time;
    snapshot.packetLossPercent = lossPercent;
    snapshot.hostFps = decode.current_host_fps;
    snapshot.receivedFps = decode.current_received_fps;
    snapshot.decodedFps = decode.current_decoded_fps;
    snapshot.renderedFps = render.rendered_fps;
    snapshot.gpuRenderMs = render.gpu_rendering_time;
#if ARTEMIS_HAS_FRAME_QUEUE
    snapshot.queueDepth = AVFrameHolder::instance().getFrameQueueSize();
    snapshot.queueTarget = AVFrameHolder::instance().getFrameQueueTargetDepth();
    snapshot.queueCapacity = AVFrameHolder::instance().getFrameQueueCapacity();
#endif
    snapshot.presentationMode = scaleModeLabel();
    snapshot.colorRange = colorRangeLabel();
    const auto lite = artemis::performance::buildLiteStatus(snapshot);

    setDetailTextIfChanged(receiveLatency, lite.latencyText);
    setDetailTextIfChanged(decodeLatency, lite.decodeText);
    setDetailTextIfChanged(renderLatency, lite.renderText);
    setDetailTextIfChanged(packetLoss, lite.packetLossText);
    setDetailTextIfChanged(hostFps, lite.hostFpsText);
    setDetailTextIfChanged(receivedFps, lite.receivedFpsText);
    setDetailTextIfChanged(decodedFps, lite.decodedFpsText);
    setDetailTextIfChanged(renderedFps, lite.fpsText);
    setDetailTextIfChanged(frameQueue, lite.frameQueueText);
    setDetailTextIfChanged(gpuRender, lite.gpuText);
    setDetailTextIfChanged(presentation, lite.presentationText);

    const auto color = lite.healthy
        ? Application::getTheme()["brls/list/listItem_value_color"]
        : Application::getTheme()["brls/accent"];
    packetLoss->setDetailTextColor(color);
    renderedFps->setDetailTextColor(color);

    updateBenchmarkStatus();
}
